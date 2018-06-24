
#include "FingerPrint.h"
#include "FingerPrintConfig.h"
#include "usart.h"
#include "task.h"
#include <string.h>




//#########################################################################################################################
#define FINGERPRINT_OK 													0x00
#define FINGERPRINT_PACKETRECIEVEERR 						0x01
#define FINGERPRINT_NOFINGER 										0x02
#define FINGERPRINT_IMAGEFAIL 									0x03
#define FINGERPRINT_IMAGEMESS 									0x06
#define FINGERPRINT_FEATUREFAIL 								0x07
#define FINGERPRINT_NOMATCH 										0x08
#define FINGERPRINT_NOTFOUND 										0x09
#define FINGERPRINT_ENROLLMISMATCH 							0x0A
#define FINGERPRINT_BADLOCATION 								0x0B
#define FINGERPRINT_DBRANGEFAIL 								0x0C
#define FINGERPRINT_UPLOADFEATUREFAIL 					0x0D
#define FINGERPRINT_PACKETRESPONSEFAIL 					0x0E
#define FINGERPRINT_UPLOADFAIL 									0x0F
#define FINGERPRINT_DELETEFAIL 									0x10
#define FINGERPRINT_DBCLEARFAIL 								0x11
#define FINGERPRINT_PASSFAIL 										0x13
#define FINGERPRINT_INVALIDIMAGE 								0x15
#define FINGERPRINT_FLASHERR 										0x18
#define FINGERPRINT_INVALIDREG 									0x1A
#define FINGERPRINT_ADDRCODE 										0x20
#define FINGERPRINT_PASSVERIFY 									0x21

#define FINGERPRINT_STARTCODE_BYTE0							0xEF
#define FINGERPRINT_STARTCODE_BYTE1							0x01

#define FINGERPRINT_COMMANDPACKET 							0x1
#define FINGERPRINT_DATAPACKET 									0x2
#define FINGERPRINT_ACKPACKET 									0x7
#define FINGERPRINT_ENDDATAPACKET 							0x8

#define FINGERPRINT_TIMEOUT 										0xFF
#define FINGERPRINT_BADPACKET 									0xFE

#define FINGERPRINT_GETIMAGE 										0x01
#define FINGERPRINT_IMAGE2TZ 										0x02
#define FINGERPRINT_SEARCH	 										0x04
#define FINGERPRINT_REGMODEL 										0x05
#define FINGERPRINT_STORE 											0x06
#define FINGERPRINT_LOAD 												0x07
#define FINGERPRINT_UPLOAD 											0x08
#define FINGERPRINT_DELETE 											0x0C
#define FINGERPRINT_EMPTY 											0x0D
#define FINGERPRINT_SETPASSWORD 								0x12
#define FINGERPRINT_VERIFYPASSWORD 							0x13
#define FINGERPRINT_HISPEEDSEARCH 							0x1B
#define FINGERPRINT_TEMPLATECOUNT 							0x1D
//#########################################################################################################################
osThreadId FingerPrintTaskHandle;
osThreadId FingerPrintBufferHandle;

FingerPrint_t	FingerPrint;


//#########################################################################################################################
void	FingerPrint_RxCallback(void)
{
	FingerPrint.RxLastTime=HAL_GetTick();
	FingerPrint.RxBuffer[FingerPrint.RxIndex] = FingerPrint.RxTmp;	
	if(FingerPrint.RxIndex<sizeof(FingerPrint.RxBuffer)-2)
		FingerPrint.RxIndex++;
	HAL_UART_Receive_IT(&_FINGERPRINT_USART,&FingerPrint.RxTmp,1);
}
//#########################################################################################################################
void FingerPrintTask(void const * argument)
{
	FingerPrint.TouchIndex = 0;
	memset(FingerPrint.RxBuffer,0,sizeof(FingerPrint.RxBuffer));
	memset(FingerPrint.TxBuffer,0,sizeof(FingerPrint.TxBuffer));
	HAL_UART_Receive_IT(&_FINGERPRINT_USART,&FingerPrint.RxTmp,1);
  for(;;)
  {
		//+++	Touch sensor process
		if(FingerPrint.Lock==0)
		{
			if(HAL_GPIO_ReadPin(_FINGERPRINT_IRQ_GPIO,_FINGERPRINT_IRQ_PIN)==GPIO_PIN_RESET)
			{
				HAL_GPIO_WritePin(_FINGERPRINT_POWER_GPIO,_FINGERPRINT_POWER_PIN,GPIO_PIN_SET);
				if(FingerPrint.TouchIndex<5)
					FingerPrint.TouchIndex++;			
			}
			else
			{
				FingerPrint.TouchIndex = 0;
				HAL_GPIO_WritePin(_FINGERPRINT_POWER_GPIO,_FINGERPRINT_POWER_PIN,GPIO_PIN_RESET);
			}
		}
		//---	Touch sensor process

		//+++	Finger pressed and do job	
		if(FingerPrint.TouchIndex==5)
		{
			
			FingerPrint.LastDetectedLocation = FingerPrint_Scan();
			if(FingerPrint.LastDetectedLocation >= 0)
				FingerPrint_Detect(FingerPrint.LastDetectedLocation);				
			while(HAL_GPIO_ReadPin(_FINGERPRINT_IRQ_GPIO,_FINGERPRINT_IRQ_PIN)==GPIO_PIN_RESET)
				osDelay(500);
			osDelay(500);				
		}
		//---	Finger pressed and do job
		osDelay(100);
  }
}
//#########################################################################################################################
void FingerPrintBufferTask(void const * argument)
{
	osDelay(100);
	while(1)
	{
				//+++	Serial process
    if((HAL_GetTick()-FingerPrint.RxLastTime > 100) && (FingerPrint.RxIndex>0))
		{
			//+++	Rx Data from Sensor
			if((FingerPrint.RxBuffer[0]==0xEF) && (FingerPrint.RxBuffer[1]==0x01) && (FingerPrint.RxBuffer[2]==0xFF) && (FingerPrint.RxBuffer[3]==0xFF) && (FingerPrint.RxBuffer[4]==0xFF) && (FingerPrint.RxBuffer[5]==0xFF) && (FingerPrint.RxBuffer[6]==0x07))
			{				
				memset(FingerPrint.AnswerBuffer,0,sizeof(FingerPrint.AnswerBuffer));
				memcpy(FingerPrint.AnswerBuffer,&FingerPrint.RxBuffer[7],FingerPrint.RxBuffer[8]+2);
				FingerPrint.GotAnswer=1;
			}		
			//---	Rx Data from Sensor		
			memset(FingerPrint.RxBuffer,0,sizeof(FingerPrint.RxBuffer));
			FingerPrint.RxIndex=0;
		}
		//---	Serial process
		osDelay(100);		
	}	
}
//#########################################################################################################################
void	FingerPrint_Init(osPriority Priority)
{	
  osThreadDef(NameFingerPrintTask, FingerPrintTask, Priority, 0, 128);
  FingerPrintTaskHandle = osThreadCreate(osThread(NameFingerPrintTask), NULL);
	osThreadDef(NameFingerPrintBufferTask, FingerPrintBufferTask, Priority, 0, 128);
  FingerPrintBufferHandle = osThreadCreate(osThread(NameFingerPrintBufferTask), NULL);
	osDelay(200);
	FingerPrint_ReadTemplateNumber();
}
//#########################################################################################################################
bool	Fingerprint_VerifyPassword(uint32_t pass)
{
	uint16_t	checksum=0;
	if(FingerPrint.Lock==1)
		return false;
	FingerPrint.Lock=1;
	HAL_GPIO_WritePin(_FINGERPRINT_POWER_GPIO,_FINGERPRINT_POWER_PIN,GPIO_PIN_SET);
	osDelay(500);
	memset(FingerPrint.TxBuffer,0,sizeof(FingerPrint.TxBuffer));
	FingerPrint.TxBuffer[0]=FINGERPRINT_STARTCODE_BYTE0;
	FingerPrint.TxBuffer[1]=FINGERPRINT_STARTCODE_BYTE1;
	FingerPrint.TxBuffer[2]=0xFF;
	FingerPrint.TxBuffer[3]=0xFF;
	FingerPrint.TxBuffer[4]=0xFF;
	FingerPrint.TxBuffer[5]=0xFF;
	FingerPrint.TxBuffer[6]=FINGERPRINT_COMMANDPACKET;
	FingerPrint.TxBuffer[7]=0x00;
	FingerPrint.TxBuffer[8]=0x07;
	FingerPrint.TxBuffer[9]=FINGERPRINT_VERIFYPASSWORD;
	FingerPrint.TxBuffer[10]=pass>>24;
	FingerPrint.TxBuffer[11]=pass>>16;
	FingerPrint.TxBuffer[12]=pass>>8;
	FingerPrint.TxBuffer[13]=pass&0x000000FF;
	for(uint8_t	i=6 ; i<=13 ; i++)
		checksum+=FingerPrint.TxBuffer[i];
	FingerPrint.TxBuffer[14] = checksum >> 8; 
	FingerPrint.TxBuffer[15] = checksum & 0x00FF; 
	
	HAL_UART_Transmit(&_FINGERPRINT_USART,FingerPrint.TxBuffer,16,100);
	osDelay(200);
	HAL_GPIO_WritePin(_FINGERPRINT_POWER_GPIO,_FINGERPRINT_POWER_PIN,GPIO_PIN_RESET);
	FingerPrint.Lock=0;		
	if(FingerPrint.GotAnswer==1)
	{
		if((FingerPrint.AnswerBuffer[0]==0x00) && (FingerPrint.AnswerBuffer[1]==0x03) && (FingerPrint.AnswerBuffer[2]==0x00) && (FingerPrint.AnswerBuffer[3]==0x00) && (FingerPrint.AnswerBuffer[4]==0x0A))
		{
			FingerPrint.Lock=0;
			return true;		
		}
		else
		{
			FingerPrint.Lock=0;
			return false;
		}
	}
	else
	{		
		FingerPrint.Lock=0;
		return false;
	}	
}
//#########################################################################################################################
bool	Fingerprint_SaveNewFinger(uint16_t	Location,uint8_t	WaitForFingerInSecond)
{
	uint8_t TimeOut;
	if(FingerPrint.Lock==1)
		return false;
	FingerPrint.Lock=1;
	FingerPrint.GotAnswer=0;
	HAL_GPIO_WritePin(_FINGERPRINT_POWER_GPIO,_FINGERPRINT_POWER_PIN,GPIO_PIN_SET);
	osDelay(500);
	//+++ take Image
	memset(FingerPrint.TxBuffer,0,sizeof(FingerPrint.TxBuffer));
	FingerPrint.TxBuffer[0]=FINGERPRINT_STARTCODE_BYTE0;
	FingerPrint.TxBuffer[1]=FINGERPRINT_STARTCODE_BYTE1;
	FingerPrint.TxBuffer[2]=0xFF;
	FingerPrint.TxBuffer[3]=0xFF;
	FingerPrint.TxBuffer[4]=0xFF;
	FingerPrint.TxBuffer[5]=0xFF;
	FingerPrint.TxBuffer[6]=FINGERPRINT_COMMANDPACKET;
	FingerPrint.TxBuffer[7]=0x00;
	FingerPrint.TxBuffer[8]=0x03;
	FingerPrint.TxBuffer[9]=FINGERPRINT_GETIMAGE;
	FingerPrint.TxBuffer[10]=0x00;
	FingerPrint.TxBuffer[11]=0x05;	
	TimeOut=0;
	while(TimeOut < WaitForFingerInSecond)
	{
		memset(FingerPrint.AnswerBuffer,0,sizeof(FingerPrint.AnswerBuffer));
		HAL_UART_Transmit(&_FINGERPRINT_USART,FingerPrint.TxBuffer,12,100);
		osDelay(1000);
		if((FingerPrint.AnswerBuffer[0]==0x00) && (FingerPrint.AnswerBuffer[1]==0x03) && (FingerPrint.AnswerBuffer[2]==0x00) && (FingerPrint.AnswerBuffer[3]==0x00) && (FingerPrint.AnswerBuffer[4]==0x0A))
			break;
		TimeOut++;
		if(TimeOut==WaitForFingerInSecond)
			goto Faild;
	}
	//--- take Image
	//+++	put image to buffer 1
	do
	{
		FingerPrint.TxBuffer[8]=0x04;
		FingerPrint.TxBuffer[9]=FINGERPRINT_IMAGE2TZ;
		FingerPrint.TxBuffer[10]=1;
		FingerPrint.TxBuffer[11]=0x00;
		FingerPrint.TxBuffer[12]=0x08;
		memset(FingerPrint.AnswerBuffer,0,sizeof(FingerPrint.AnswerBuffer));	
		HAL_UART_Transmit(&_FINGERPRINT_USART,FingerPrint.TxBuffer,13,100);
		osDelay(1000);
		if((FingerPrint.AnswerBuffer[0]==0x00) && (FingerPrint.AnswerBuffer[1]==0x03) && (FingerPrint.AnswerBuffer[2]==0x00) && (FingerPrint.AnswerBuffer[3]==0x00) && (FingerPrint.AnswerBuffer[4]==0x0A))
			break;
		else
			goto Faild;
	}while(0);
	//---	put image to buffer 1	

	//+++ Wait for put your finger up
	TimeOut=0;
	while(TimeOut < WaitForFingerInSecond)
	{
		osDelay(1000);
		TimeOut++;
		if(TimeOut==WaitForFingerInSecond)
			goto Faild;
		if( HAL_GPIO_ReadPin(_FINGERPRINT_IRQ_GPIO,_FINGERPRINT_IRQ_PIN)==GPIO_PIN_SET)
			break;	
	}	
	//--- Wait for put your finger up
	
	//+++ take Image
	FingerPrint.TxBuffer[8]=0x03;
	FingerPrint.TxBuffer[9]=FINGERPRINT_GETIMAGE;
	FingerPrint.TxBuffer[10]=0x00;
	FingerPrint.TxBuffer[11]=0x05;	
	TimeOut=0;
	while(TimeOut < WaitForFingerInSecond)
	{
		memset(FingerPrint.AnswerBuffer,0,sizeof(FingerPrint.AnswerBuffer));
		HAL_UART_Transmit(&_FINGERPRINT_USART,FingerPrint.TxBuffer,12,100);
		osDelay(1000);
		if((FingerPrint.AnswerBuffer[0]==0x00) && (FingerPrint.AnswerBuffer[1]==0x03) && (FingerPrint.AnswerBuffer[2]==0x00) && (FingerPrint.AnswerBuffer[3]==0x00) && (FingerPrint.AnswerBuffer[4]==0x0A))
			break;
		TimeOut++;
		if(TimeOut==WaitForFingerInSecond)
			goto Faild;
	}
	//--- take Image

	//+++	put image to buffer 2
	do
	{
		FingerPrint.TxBuffer[8]=0x04;
		FingerPrint.TxBuffer[9]=FINGERPRINT_IMAGE2TZ;
		FingerPrint.TxBuffer[10]=2;
		FingerPrint.TxBuffer[11]=0x00;
		FingerPrint.TxBuffer[12]=0x09;
		memset(FingerPrint.AnswerBuffer,0,sizeof(FingerPrint.AnswerBuffer));	
		HAL_UART_Transmit(&_FINGERPRINT_USART,FingerPrint.TxBuffer,13,100);
		osDelay(1000);
		if((FingerPrint.AnswerBuffer[0]==0x00) && (FingerPrint.AnswerBuffer[1]==0x03) && (FingerPrint.AnswerBuffer[2]==0x00) && (FingerPrint.AnswerBuffer[3]==0x00) && (FingerPrint.AnswerBuffer[4]==0x0A))
			break;
		else
			goto Faild;
	}while(0);
	//---	put image to buffer 2
	
	//+++ Wait for put your finger up
	TimeOut=0;
	while(TimeOut < WaitForFingerInSecond)
	{
		osDelay(1000);
		TimeOut++;
		if(TimeOut==WaitForFingerInSecond)
			goto Faild;
		if( HAL_GPIO_ReadPin(_FINGERPRINT_IRQ_GPIO,_FINGERPRINT_IRQ_PIN)==GPIO_PIN_SET)
			break;	
	}	
	//--- Wait for put your finger up

	//+++ Create Register model
	do
	{
		FingerPrint.TxBuffer[8]=0x03;
		FingerPrint.TxBuffer[9]=FINGERPRINT_REGMODEL;
		FingerPrint.TxBuffer[10]=0x00;
		FingerPrint.TxBuffer[11]=0x09;	
		memset(FingerPrint.AnswerBuffer,0,sizeof(FingerPrint.AnswerBuffer));	
		HAL_UART_Transmit(&_FINGERPRINT_USART,FingerPrint.TxBuffer,12,100);
		osDelay(1000);
		if((FingerPrint.AnswerBuffer[0]==0x00) && (FingerPrint.AnswerBuffer[1]==0x03) && (FingerPrint.AnswerBuffer[2]==0x00) && (FingerPrint.AnswerBuffer[3]==0x00) && (FingerPrint.AnswerBuffer[4]==0x0A))
			break;
		else
			goto Faild;
		
	}while(0);		
	//---	Create Register model
	
	//+++ Store in memory
	do
	{
		FingerPrint.TxBuffer[8]=0x06;
		FingerPrint.TxBuffer[9]=FINGERPRINT_STORE;
		FingerPrint.TxBuffer[10]=0x01;
		FingerPrint.TxBuffer[11]=Location>>8;	
		FingerPrint.TxBuffer[12]=Location&0x00ff;
		uint8_t	Checksum=0;
		for(uint8_t i=0;i<9; i++)
			Checksum+=FingerPrint.TxBuffer[i+6];
		FingerPrint.TxBuffer[13]=Checksum>>8;
		FingerPrint.TxBuffer[14]=Checksum&0x00FF;
		memset(FingerPrint.AnswerBuffer,0,sizeof(FingerPrint.AnswerBuffer));	
		HAL_UART_Transmit(&_FINGERPRINT_USART,FingerPrint.TxBuffer,15,100);
		osDelay(1000);
		if((FingerPrint.AnswerBuffer[0]==0x00) && (FingerPrint.AnswerBuffer[1]==0x03) && (FingerPrint.AnswerBuffer[2]==0x00) && (FingerPrint.AnswerBuffer[3]==0x00) && (FingerPrint.AnswerBuffer[4]==0x0A))
			break;
		else
			goto Faild;
		
	}while(0);				
	//--- Store in memory
	
	HAL_GPIO_WritePin(_FINGERPRINT_POWER_GPIO,_FINGERPRINT_POWER_PIN,GPIO_PIN_RESET);
	FingerPrint.Lock=0;
	FingerPrint_ReadTemplateNumber();
	return true;
	
	Faild:
	HAL_GPIO_WritePin(_FINGERPRINT_POWER_GPIO,_FINGERPRINT_POWER_PIN,GPIO_PIN_RESET);
	FingerPrint.Lock=0;
	return false;
}
//#########################################################################################################################
int16_t	FingerPrint_Scan(void)
{
	uint8_t Timeout;
	if(FingerPrint.Lock==1)
		return -1;
	FingerPrint.Lock=1;
	FingerPrint.GotAnswer=0;
	uint8_t	IfModuleIsOff=0;
	if(HAL_GPIO_ReadPin(_FINGERPRINT_POWER_GPIO,_FINGERPRINT_POWER_PIN)==GPIO_PIN_RESET)
			IfModuleIsOff=1;
	//+++ take Image
	memset(FingerPrint.TxBuffer,0,sizeof(FingerPrint.TxBuffer));
	FingerPrint.TxBuffer[0]=FINGERPRINT_STARTCODE_BYTE0;
	FingerPrint.TxBuffer[1]=FINGERPRINT_STARTCODE_BYTE1;
	FingerPrint.TxBuffer[2]=0xFF;
	FingerPrint.TxBuffer[3]=0xFF;
	FingerPrint.TxBuffer[4]=0xFF;
	FingerPrint.TxBuffer[5]=0xFF;
	FingerPrint.TxBuffer[6]=FINGERPRINT_COMMANDPACKET;
	FingerPrint.TxBuffer[7]=0x00;
	FingerPrint.TxBuffer[8]=0x03;
	FingerPrint.TxBuffer[9]=FINGERPRINT_GETIMAGE;
	FingerPrint.TxBuffer[10]=0x00;
	FingerPrint.TxBuffer[11]=0x05;	
	do
	{
		if(IfModuleIsOff==1)
		{
			HAL_GPIO_WritePin(_FINGERPRINT_POWER_GPIO,_FINGERPRINT_POWER_PIN,GPIO_PIN_SET);
			osDelay(500);
		}	
		memset(FingerPrint.AnswerBuffer,0,sizeof(FingerPrint.AnswerBuffer));
		HAL_UART_Transmit(&_FINGERPRINT_USART,FingerPrint.TxBuffer,12,100);
		for(Timeout=0; Timeout<20 ; Timeout++)
		{	
			osDelay(100);
			if((FingerPrint.AnswerBuffer[0]==0x00) && (FingerPrint.AnswerBuffer[1]==0x03) && (FingerPrint.AnswerBuffer[2]==0x00) && (FingerPrint.AnswerBuffer[3]==0x00) && (FingerPrint.AnswerBuffer[4]==0x0A))
				break;
		}		
		if(Timeout>19)
			goto Faild;
	}while(0);
	//--- take Image
	
	//+++	put image to buffer 1
	do
	{
		FingerPrint.TxBuffer[8]=0x04;
		FingerPrint.TxBuffer[9]=FINGERPRINT_IMAGE2TZ;
		FingerPrint.TxBuffer[10]=1;
		FingerPrint.TxBuffer[11]=0x00;
		FingerPrint.TxBuffer[12]=0x08;
		memset(FingerPrint.AnswerBuffer,0,sizeof(FingerPrint.AnswerBuffer));	
		HAL_UART_Transmit(&_FINGERPRINT_USART,FingerPrint.TxBuffer,13,100);
		for(Timeout=0; Timeout<20 ; Timeout++)
		{
			osDelay(100);
			if((FingerPrint.AnswerBuffer[0]==0x00) && (FingerPrint.AnswerBuffer[1]==0x03) && (FingerPrint.AnswerBuffer[2]==0x00) && (FingerPrint.AnswerBuffer[3]==0x00) && (FingerPrint.AnswerBuffer[4]==0x0A))
				break;
			if(Timeout>19)
				goto Faild;
		}
	}while(0);
	//---	put image to buffer 1	
	
	//+++	Searching
	do
	{
		FingerPrint.TxBuffer[8]=0x08;
		FingerPrint.TxBuffer[9]=FINGERPRINT_SEARCH;
		FingerPrint.TxBuffer[10]=1;
		FingerPrint.TxBuffer[11]=0x00;
		FingerPrint.TxBuffer[12]=0x00;
		FingerPrint.TxBuffer[13]=0x01;
		FingerPrint.TxBuffer[14]=0xF4;
		uint16_t	Checksum=0;
		for(uint8_t i=0;i<11; i++)
			Checksum+=FingerPrint.TxBuffer[i+6];
		FingerPrint.TxBuffer[15]=Checksum>>8;
		FingerPrint.TxBuffer[16]=Checksum&0x00FF;
		
		memset(FingerPrint.AnswerBuffer,0,sizeof(FingerPrint.AnswerBuffer));	
		HAL_UART_Transmit(&_FINGERPRINT_USART,FingerPrint.TxBuffer,17,100);
		for(Timeout=0; Timeout<20 ; Timeout++)
		{
			osDelay(100);
			if((FingerPrint.AnswerBuffer[0]==0x00) && (FingerPrint.AnswerBuffer[1]==0x07) && (FingerPrint.AnswerBuffer[2]==0x00))
			{
				FingerPrint.Lock=0;
				if(IfModuleIsOff==1)
					HAL_GPIO_WritePin(_FINGERPRINT_POWER_GPIO,_FINGERPRINT_POWER_PIN,GPIO_PIN_RESET);
				return FingerPrint.AnswerBuffer[3]*256+FingerPrint.AnswerBuffer[4];
			}
			if(Timeout>19)
				goto Faild;
		}
	}while(0);
	//---	Searching	
	
	Faild:
	if(IfModuleIsOff==1)
		HAL_GPIO_WritePin(_FINGERPRINT_POWER_GPIO,_FINGERPRINT_POWER_PIN,GPIO_PIN_RESET);
	FingerPrint.Lock=0;
	
	return -1;
	
	
}
//#########################################################################################################################
int16_t		FingerPrint_ReadTemplateNumber(void)
{
	uint8_t Timeout;
	uint8_t	IfModuleIsOff=0;
	if(FingerPrint.Lock==1)
		return -1;
	FingerPrint.Lock=1;
	FingerPrint.GotAnswer=0;
	//+++ Read Template
	if(HAL_GPIO_ReadPin(_FINGERPRINT_POWER_GPIO,_FINGERPRINT_POWER_PIN)==GPIO_PIN_RESET)
		IfModuleIsOff=1;
	if(IfModuleIsOff==1)
	{
		HAL_GPIO_WritePin(_FINGERPRINT_POWER_GPIO,_FINGERPRINT_POWER_PIN,GPIO_PIN_SET);
		osDelay(500);
	}
	memset(FingerPrint.TxBuffer,0,sizeof(FingerPrint.TxBuffer));
	FingerPrint.TxBuffer[0]=FINGERPRINT_STARTCODE_BYTE0;
	FingerPrint.TxBuffer[1]=FINGERPRINT_STARTCODE_BYTE1;
	FingerPrint.TxBuffer[2]=0xFF;
	FingerPrint.TxBuffer[3]=0xFF;
	FingerPrint.TxBuffer[4]=0xFF;
	FingerPrint.TxBuffer[5]=0xFF;
	FingerPrint.TxBuffer[6]=FINGERPRINT_COMMANDPACKET;
	FingerPrint.TxBuffer[7]=0x00;
	FingerPrint.TxBuffer[8]=0x03;
	FingerPrint.TxBuffer[9]=FINGERPRINT_TEMPLATECOUNT;
	FingerPrint.TxBuffer[10]=0x00;
	FingerPrint.TxBuffer[11]=0x21;	
	do
	{
			
		memset(FingerPrint.AnswerBuffer,0,sizeof(FingerPrint.AnswerBuffer));
		HAL_UART_Transmit(&_FINGERPRINT_USART,FingerPrint.TxBuffer,12,100);
		for(Timeout=0; Timeout<20 ; Timeout++)
		{	
			osDelay(100);
			if((FingerPrint.AnswerBuffer[0]==0x00) && (FingerPrint.AnswerBuffer[1]==0x05) && (FingerPrint.AnswerBuffer[2]==0x00))
			{
				FingerPrint.Lock=0;
				if(IfModuleIsOff==1)
					HAL_GPIO_WritePin(_FINGERPRINT_POWER_GPIO,_FINGERPRINT_POWER_PIN,GPIO_PIN_RESET);
				return FingerPrint.Template=(FingerPrint.AnswerBuffer[3]*256+FingerPrint.AnswerBuffer[4]);
			}
		}		
		if(Timeout>19)
			goto Faild;
	}while(0);
	//--- Read Template	
	
	Faild:
	if(IfModuleIsOff==1)
		HAL_GPIO_WritePin(_FINGERPRINT_POWER_GPIO,_FINGERPRINT_POWER_PIN,GPIO_PIN_RESET);
	FingerPrint.Lock=0;
	return -1;
}
//#########################################################################################################################
//#########################################################################################################################
bool		FingerPrint_DeleteAll(void)
{
	uint8_t Timeout;
	uint8_t	IfModuleIsOff=0;
	if(FingerPrint.Lock==1)
		return -1;
	FingerPrint.Lock=1;
	FingerPrint.GotAnswer=0;
	//+++ Delete All
	if(HAL_GPIO_ReadPin(_FINGERPRINT_POWER_GPIO,_FINGERPRINT_POWER_PIN)==GPIO_PIN_RESET)
		IfModuleIsOff=1;
	if(IfModuleIsOff==1)
	{
		HAL_GPIO_WritePin(_FINGERPRINT_POWER_GPIO,_FINGERPRINT_POWER_PIN,GPIO_PIN_SET);
		osDelay(500);
	}
	memset(FingerPrint.TxBuffer,0,sizeof(FingerPrint.TxBuffer));
	FingerPrint.TxBuffer[0]=FINGERPRINT_STARTCODE_BYTE0;
	FingerPrint.TxBuffer[1]=FINGERPRINT_STARTCODE_BYTE1;
	FingerPrint.TxBuffer[2]=0xFF;
	FingerPrint.TxBuffer[3]=0xFF;
	FingerPrint.TxBuffer[4]=0xFF;
	FingerPrint.TxBuffer[5]=0xFF;
	FingerPrint.TxBuffer[6]=FINGERPRINT_COMMANDPACKET;
	FingerPrint.TxBuffer[7]=0x00;
	FingerPrint.TxBuffer[8]=0x03;
	FingerPrint.TxBuffer[9]=FINGERPRINT_EMPTY;
	FingerPrint.TxBuffer[10]=0x00;
	FingerPrint.TxBuffer[11]=0x11;	
	do
	{
			
		memset(FingerPrint.AnswerBuffer,0,sizeof(FingerPrint.AnswerBuffer));
		HAL_UART_Transmit(&_FINGERPRINT_USART,FingerPrint.TxBuffer,12,100);
		for(Timeout=0; Timeout<20 ; Timeout++)
		{	
			osDelay(100);
			if((FingerPrint.AnswerBuffer[0]==0x00) && (FingerPrint.AnswerBuffer[1]==0x03) && (FingerPrint.AnswerBuffer[2]==0x00) && (FingerPrint.AnswerBuffer[3]==0x00) && (FingerPrint.AnswerBuffer[4]==0x0A))
			{
				FingerPrint.Template=0;
				FingerPrint.Lock=0;
				if(IfModuleIsOff==1)
					HAL_GPIO_WritePin(_FINGERPRINT_POWER_GPIO,_FINGERPRINT_POWER_PIN,GPIO_PIN_RESET);
				return true;
			}
		}		
		if(Timeout>19)
			goto Faild;
	}while(0);
	//--- Delete All	
	
	Faild:
	if(IfModuleIsOff==1)
		HAL_GPIO_WritePin(_FINGERPRINT_POWER_GPIO,_FINGERPRINT_POWER_PIN,GPIO_PIN_RESET);
	FingerPrint.Lock=0;
	return false;
}
//#########################################################################################################################
bool		FingerPrint_DeleteByLocation(uint16_t	Location)
{
	uint8_t Timeout;
	uint8_t	IfModuleIsOff=0;
	if(FingerPrint.Lock==1)
		return -1;
	FingerPrint.Lock=1;
	FingerPrint.GotAnswer=0;
	//+++ Delete 
	if(HAL_GPIO_ReadPin(_FINGERPRINT_POWER_GPIO,_FINGERPRINT_POWER_PIN)==GPIO_PIN_RESET)
		IfModuleIsOff=1;
	if(IfModuleIsOff==1)
	{
		HAL_GPIO_WritePin(_FINGERPRINT_POWER_GPIO,_FINGERPRINT_POWER_PIN,GPIO_PIN_SET);
		osDelay(500);
	}
	memset(FingerPrint.TxBuffer,0,sizeof(FingerPrint.TxBuffer));
	FingerPrint.TxBuffer[0]=FINGERPRINT_STARTCODE_BYTE0;
	FingerPrint.TxBuffer[1]=FINGERPRINT_STARTCODE_BYTE1;
	FingerPrint.TxBuffer[2]=0xFF;
	FingerPrint.TxBuffer[3]=0xFF;
	FingerPrint.TxBuffer[4]=0xFF;
	FingerPrint.TxBuffer[5]=0xFF;
	FingerPrint.TxBuffer[6]=FINGERPRINT_COMMANDPACKET;
	FingerPrint.TxBuffer[7]=0x00;
	FingerPrint.TxBuffer[8]=0x07;
	FingerPrint.TxBuffer[9]=FINGERPRINT_DELETE;
	FingerPrint.TxBuffer[10]=Location>>8;
	FingerPrint.TxBuffer[11]=Location&0x00FF;	
	FingerPrint.TxBuffer[12]=0x00;
	FingerPrint.TxBuffer[13]=0x01;
	uint16_t	Checksum=0;
		for(uint8_t i=0;i<8; i++)
			Checksum+=FingerPrint.TxBuffer[i+6];
	FingerPrint.TxBuffer[14]=Checksum>>8;
	FingerPrint.TxBuffer[15]=Checksum&0x00FF;
	do
	{
			
		memset(FingerPrint.AnswerBuffer,0,sizeof(FingerPrint.AnswerBuffer));
		HAL_UART_Transmit(&_FINGERPRINT_USART,FingerPrint.TxBuffer,16,100);
		for(Timeout=0; Timeout<20 ; Timeout++)
		{	
			osDelay(100);
			if((FingerPrint.AnswerBuffer[0]==0x00) && (FingerPrint.AnswerBuffer[1]==0x03) && (FingerPrint.AnswerBuffer[2]==0x00) && (FingerPrint.AnswerBuffer[3]==0x00) && (FingerPrint.AnswerBuffer[4]==0x0A))
			{
				FingerPrint.Lock=0;
				FingerPrint_ReadTemplateNumber();
				if(IfModuleIsOff==1)
					HAL_GPIO_WritePin(_FINGERPRINT_POWER_GPIO,_FINGERPRINT_POWER_PIN,GPIO_PIN_RESET);				
				return true;
			}
		}		
		if(Timeout>19)
			goto Faild;
	}while(0);
	//--- Delete 	
	
	Faild:
	if(IfModuleIsOff==1)
		HAL_GPIO_WritePin(_FINGERPRINT_POWER_GPIO,_FINGERPRINT_POWER_PIN,GPIO_PIN_RESET);
	FingerPrint.Lock=0;
	return false;
}
//#########################################################################################################################
int16_t		FingerPrint_DeleteByFinger(uint8_t	TimoutInSecond)
{
	uint8_t TimeOut;
	uint8_t	IfModuleIsOff=0;
	int16_t	Location=-1;
	uint16_t	Checksum=0;
	if(FingerPrint.Lock==1)
		return false;
	FingerPrint.Lock=1;
	HAL_GPIO_WritePin(_FINGERPRINT_POWER_GPIO,_FINGERPRINT_POWER_PIN,GPIO_PIN_SET);
	osDelay(500);
	//+++ take Image
	memset(FingerPrint.TxBuffer,0,sizeof(FingerPrint.TxBuffer));
	FingerPrint.TxBuffer[0]=FINGERPRINT_STARTCODE_BYTE0;
	FingerPrint.TxBuffer[1]=FINGERPRINT_STARTCODE_BYTE1;
	FingerPrint.TxBuffer[2]=0xFF;
	FingerPrint.TxBuffer[3]=0xFF;
	FingerPrint.TxBuffer[4]=0xFF;
	FingerPrint.TxBuffer[5]=0xFF;
	FingerPrint.TxBuffer[6]=FINGERPRINT_COMMANDPACKET;
	FingerPrint.TxBuffer[7]=0x00;
	FingerPrint.TxBuffer[8]=0x03;
	FingerPrint.TxBuffer[9]=FINGERPRINT_GETIMAGE;
	FingerPrint.TxBuffer[10]=0x00;
	FingerPrint.TxBuffer[11]=0x05;	
	TimeOut=0;
	while(TimeOut < TimoutInSecond)
	{
		memset(FingerPrint.AnswerBuffer,0,sizeof(FingerPrint.AnswerBuffer));
		HAL_UART_Transmit(&_FINGERPRINT_USART,FingerPrint.TxBuffer,12,100);
		osDelay(1000);
		if((FingerPrint.AnswerBuffer[0]==0x00) && (FingerPrint.AnswerBuffer[1]==0x03) && (FingerPrint.AnswerBuffer[2]==0x00) && (FingerPrint.AnswerBuffer[3]==0x00) && (FingerPrint.AnswerBuffer[4]==0x0A))
			break;
		TimeOut++;
		if(TimeOut==TimoutInSecond)
			goto Faild;
	}
	//--- take Image
	//+++	put image to buffer 1
	do
	{
		FingerPrint.TxBuffer[8]=0x04;
		FingerPrint.TxBuffer[9]=FINGERPRINT_IMAGE2TZ;
		FingerPrint.TxBuffer[10]=1;
		FingerPrint.TxBuffer[11]=0x00;
		FingerPrint.TxBuffer[12]=0x08;
		memset(FingerPrint.AnswerBuffer,0,sizeof(FingerPrint.AnswerBuffer));	
		HAL_UART_Transmit(&_FINGERPRINT_USART,FingerPrint.TxBuffer,13,100);
		osDelay(1000);
		if((FingerPrint.AnswerBuffer[0]==0x00) && (FingerPrint.AnswerBuffer[1]==0x03) && (FingerPrint.AnswerBuffer[2]==0x00) && (FingerPrint.AnswerBuffer[3]==0x00) && (FingerPrint.AnswerBuffer[4]==0x0A))
			break;
		else
			goto Faild;
	}while(0);
	//---	put image to buffer 1	

	//+++ Wait for put your finger up
	TimeOut=0;
	while(TimeOut < TimoutInSecond)
	{
		osDelay(1000);
		TimeOut++;
		if(TimeOut==TimoutInSecond)
			goto Faild;
		if( HAL_GPIO_ReadPin(_FINGERPRINT_IRQ_GPIO,_FINGERPRINT_IRQ_PIN)==GPIO_PIN_SET)
			break;	
	}	
	//--- Wait for put your finger up
	
	//+++	Searching
	do
	{
		FingerPrint.TxBuffer[8]=0x08;
		FingerPrint.TxBuffer[9]=FINGERPRINT_SEARCH;
		FingerPrint.TxBuffer[10]=1;
		FingerPrint.TxBuffer[11]=0x00;
		FingerPrint.TxBuffer[12]=0x00;
		FingerPrint.TxBuffer[13]=0x01;
		FingerPrint.TxBuffer[14]=0xF4;
		for(uint8_t i=0;i<11; i++)
			Checksum+=FingerPrint.TxBuffer[i+6];
		FingerPrint.TxBuffer[15]=Checksum>>8;
		FingerPrint.TxBuffer[16]=Checksum&0x00FF;
		
		memset(FingerPrint.AnswerBuffer,0,sizeof(FingerPrint.AnswerBuffer));	
		HAL_UART_Transmit(&_FINGERPRINT_USART,FingerPrint.TxBuffer,17,100);
		for(TimeOut=0; TimeOut<20 ; TimeOut++)
		{
			osDelay(100);
			if((FingerPrint.AnswerBuffer[0]==0x00) && (FingerPrint.AnswerBuffer[1]==0x07) && (FingerPrint.AnswerBuffer[2]==0x00))
			{
				FingerPrint.Lock=0;
				if(IfModuleIsOff==1)
					HAL_GPIO_WritePin(_FINGERPRINT_POWER_GPIO,_FINGERPRINT_POWER_PIN,GPIO_PIN_RESET);
				Location =  FingerPrint.AnswerBuffer[3]*256+FingerPrint.AnswerBuffer[4];
				break;
			}
			if(TimeOut>19)
				goto Faild;
		}
	}while(0);
	//---	Searching	
	
	//+++ Delete 
	memset(FingerPrint.TxBuffer,0,sizeof(FingerPrint.TxBuffer));
	FingerPrint.TxBuffer[0]=FINGERPRINT_STARTCODE_BYTE0;
	FingerPrint.TxBuffer[1]=FINGERPRINT_STARTCODE_BYTE1;
	FingerPrint.TxBuffer[2]=0xFF;
	FingerPrint.TxBuffer[3]=0xFF;
	FingerPrint.TxBuffer[4]=0xFF;
	FingerPrint.TxBuffer[5]=0xFF;
	FingerPrint.TxBuffer[6]=FINGERPRINT_COMMANDPACKET;
	FingerPrint.TxBuffer[7]=0x00;
	FingerPrint.TxBuffer[8]=0x07;
	FingerPrint.TxBuffer[9]=FINGERPRINT_DELETE;
	FingerPrint.TxBuffer[10]=Location>>8;
	FingerPrint.TxBuffer[11]=Location&0x00FF;	
	FingerPrint.TxBuffer[12]=0x00;
	FingerPrint.TxBuffer[13]=0x01;
	Checksum=0;
		for(uint8_t i=0;i<8; i++)
			Checksum+=FingerPrint.TxBuffer[i+6];
	FingerPrint.TxBuffer[14]=Checksum>>8;
	FingerPrint.TxBuffer[15]=Checksum&0x00FF;
	do
	{
			
		memset(FingerPrint.AnswerBuffer,0,sizeof(FingerPrint.AnswerBuffer));
		HAL_UART_Transmit(&_FINGERPRINT_USART,FingerPrint.TxBuffer,16,100);
		for(TimeOut=0; TimeOut<20 ; TimeOut++)
		{	
			osDelay(100);
			if((FingerPrint.AnswerBuffer[0]==0x00) && (FingerPrint.AnswerBuffer[1]==0x03) && (FingerPrint.AnswerBuffer[2]==0x00) && (FingerPrint.AnswerBuffer[3]==0x00) && (FingerPrint.AnswerBuffer[4]==0x0A))
			{
				FingerPrint.Lock=0;
				FingerPrint_ReadTemplateNumber();
				if(IfModuleIsOff==1)
					HAL_GPIO_WritePin(_FINGERPRINT_POWER_GPIO,_FINGERPRINT_POWER_PIN,GPIO_PIN_RESET);				
				return Location;
			}
		}		
		if(TimeOut>19)
			goto Faild;
	}while(0);
	//--- Delete 	
	
	Faild:
	if(IfModuleIsOff==1)
		HAL_GPIO_WritePin(_FINGERPRINT_POWER_GPIO,_FINGERPRINT_POWER_PIN,GPIO_PIN_RESET);
	FingerPrint.Lock=0;
	return -1;
	
}
//#########################################################################################################################

