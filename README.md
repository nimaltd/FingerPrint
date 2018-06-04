# FingerPrint R308 library
<br />
I hope use it and enjoy.
<br />
I use Stm32f103vc and Keil Compiler and Stm32CubeMX wizard.
 <br />
Please Do This ...
<br />
<br />
1) Enable FreeRTOS.  
<br />
2) Config your usart and enable RX interrupt on CubeMX.
<br />
3) Config an input pin and a output pin.
<br />
4) Select "General peripheral Initalizion as a pair of '.c/.h' file per peripheral" on project settings.
<br />
5) Config your FingerPrintConfig.h file.
<br />
6) Add FingerPrint_RxCallBack() on usart interrupt routin. 
<br />
7) call  FingerPrint_Init(osPriorityLow) in your any task.
<br />
8) In R308 module must be first read datasheet. need a digital switch for power. should be connect to output pin and another input pin connect to touch detect pin.

