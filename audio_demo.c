/*
 * audio_demo.c描述：
 * 	SSM2603音频Codec驱动使用例程
 * 	通过axi_i2s_adi核传输音频数据
 * 	同时使用PS内的PL330 DMA控制器和i2s核通信
 * 	参考Xilin官方xiicps_polled_master_example.c
 */

#include "audio_demo.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "xparameters.h"
#include "xil_types.h"
#include "xil_io.h"
#include "xil_printf.h"
#include "timer_ps.h"
#include "xiicps.h"
#include "xuartps.h"
#include "fftdma.h"

/* 重定义设备宏定义 */
#define IIC_DEVICE_ID		XPAR_XIICPS_0_DEVICE_ID
#define I2S_ADDRESS 		XPAR_AXI_I2S_ADI_0_BASEADDR
#define TIMER_DEVICE_ID 	XPAR_SCUTIMER_DEVICE_ID

XIicPs Iic;

int recordingValid = 0;  /*指示录音是否可用*/

u32 recDataL[REC_SAMPLES]; //左声道数据
u32 recDataR[REC_SAMPLES]; //右声道数据

/*
 * 音频Codec初始化
 * 参数：
 * 	timerID 定时器的宏定义
 * 	iicID PS与SSM2603Codec连接的I2C设备的宏定义
 * 	i2sAddr I2S设备物理地址
 *
 * 返回初始化成功状态
 */
int AudioInitialize(u16 timerID,  u16 iicID, u32 i2sAddr)
{
	int Status;
	XIicPs_Config *Config;
	u32 i2sClkDiv;

	TimerInitialize(timerID);

	//初始化I2C
	Config = XIicPs_LookupConfig(iicID);
	if (NULL == Config) {
		return XST_FAILURE;
	}

	Status = XIicPs_CfgInitialize(&Iic, Config, Config->BaseAddress);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	Status = XIicPs_SelfTest(&Iic);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	//设置I2C时钟频率
	Status = XIicPs_SetSClk(&Iic, IIC_SCLK_RATE);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}


	//往SSM2603音频Codec写数据设置设备，参考SSM2603数据手册
	Status = AudioRegSet(&Iic, 15, 0b000000000); //复位
	TimerDelay(75000);
	Status |= AudioRegSet(&Iic, 6, 0b000110000); //启动电源
	Status |= AudioRegSet(&Iic, 0, 0b000010111);
	Status |= AudioRegSet(&Iic, 1, 0b000010111);
	Status |= AudioRegSet(&Iic, 2, 0b101111001);
	Status |= AudioRegSet(&Iic, 4, 0b000010000);
	Status |= AudioRegSet(&Iic, 5, 0b000000000);
	Status |= AudioRegSet(&Iic, 7, 0b000001010); //字长改为24
	Status |= AudioRegSet(&Iic, 8, 0b000000000); //取消使用CLKDIV2
	TimerDelay(75000);
	Status |= AudioRegSet(&Iic, 9, 0b000000001);
	Status |= AudioRegSet(&Iic, 6, 0b000100000);

	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	i2sClkDiv = 1; //设置BCLK=MCLK / 4
	i2sClkDiv = i2sClkDiv | (31 << 16); //设置LRCLK=BCLK / 64

	Xil_Out32(i2sAddr + I2S_CLK_CTRL_REG, i2sClkDiv); //写时钟分频计数器

	recordingValid = 0;

	return XST_SUCCESS;
}

//音频例程打印消息
void AudioPrintMenu()
{
	xil_printf("\x1B[H"); //Set cursor to top left of terminal
	xil_printf("\x1B[2J"); //Clear terminal
	xil_printf("**************************************************\n\r");
	xil_printf("*         ZYBO Audio Codec User Demo             *\n\r");
	xil_printf("**************************************************\n\r");
	xil_printf("*         * Remove headphones from ears and turn *\n\r");
	xil_printf("* WARNING * down the volume of any external      *\n\r");
	xil_printf("*         * speakers connected to HPH OUT        *\n\r");
	xil_printf("**************************************************\n\r");
	xil_printf("\n\r");
	xil_printf("1 - Generate tone on HPH OUT\n\r");
	xil_printf("2 - Record from LINE IN\n\r");
	xil_printf("3 - Record from MIC IN\n\r");
	if (recordingValid)
		xil_printf("4 - Play recording on HPH OUT\n\r");
	xil_printf("q - Quit\n\r");
	xil_printf("\n\r");
	xil_printf("Enter a selection:");
}


/*
 * 运行音频例程
 * 参数为I2S 串口 开关 按钮4个外设的基地址
 */
int AudioRunDemo(u32 i2sAddr, u32 uartAddr, u32 swAddr, u32 btnAddr)
{
	char userInput = 0;

	/* 刷新UART FIFO */
	while (XUartPs_IsReceiveData(uartAddr))
	{
		XUartPs_ReadReg(uartAddr, XUARTPS_FIFO_OFFSET);
	}

	while (userInput != 'q')
	{
		AudioPrintMenu();

		/* 等待UART输入 */
		while (!XUartPs_IsReceiveData(uartAddr))
		{}

		/* Store the first character in the UART recieve FIFO and echo it */
		userInput = XUartPs_ReadReg(uartAddr, XUARTPS_FIFO_OFFSET);
		xil_printf("%c", userInput);

		switch (userInput)
		{
		case '1':
			AudioPlayTone(i2sAddr, uartAddr, swAddr, btnAddr);
			//播放单频信号
			break;
		case '2':
			AudioRec(i2sAddr, 0);//录音
			break;
		case '3':
			AudioRec(i2sAddr, 1);//录音
			break;
		case '4':
			if (recordingValid)
				AudioPlayRec(i2sAddr);//回放
			else
			{
				xil_printf("\n\rInvalid Selection");
				TimerDelay(500000);
			}
			break;
		case 'q':
			break;
		default :
			xil_printf("\n\rInvalid Selection");
			TimerDelay(500000);
		}
	}

	return XST_SUCCESS;
}

/*
 * 播放单频信号
 */
int AudioPlayTone(u32 i2sAddr, u32 uartAddr, u32 swAddr, u32 btnAddr)
{
	u32 userInput = 0;
	u32 btnState = 0;
	u32 swState = 0;
	int Index = 0;

	xil_printf("\x1B[H"); //Set cursor to top left of terminal
	xil_printf("\x1B[2J"); //Clear terminal
	xil_printf("\n\r");
	xil_printf("--Press BTN0 to play a 500 Hz tone on the enabled channels\n\r");
	xil_printf("  of the HPH OUT port.\n\r");
	xil_printf("--Set SW1 high to enable the Left channel of HPH Out\n\r");
	xil_printf("--Set SW0 high to enable the Right channel of HPH Out\n\r");
	xil_printf("--Enter 'q' at the terminal to quit\n\r");


	Xil_Out32(i2sAddr + I2S_RESET_REG, 0b010); //复位 TX Fifo
	Xil_Out32(i2sAddr + I2S_CTRL_REG, 0b001); //使能 TX Fifo, 取消静音

	/*
	 * 刷新输入 buffer
	 */
	while (XUartPs_IsReceiveData(uartAddr))
	{
		XUartPs_ReadReg(uartAddr, XUARTPS_FIFO_OFFSET);
	}

	while (userInput != 'q')
	{
		for (Index = 0; Index < 96; Index++) //96 =(采样率 /单音频率) (48000 / 500)
		{
			btnState = Xil_In32(btnAddr);
			swState = Xil_In32(swAddr);
			//生成50%占空比方波
			if ((Index < 48) && (btnState & 0b1))
			{
				if (swState & 0b10) //Left channel
					I2SFifoWrite(i2sAddr, (1 << 31));
				else
					I2SFifoWrite(i2sAddr, 0);

				if (swState & 0b01) //Right channel
					I2SFifoWrite(i2sAddr, (1 << 31));
				else
					I2SFifoWrite(i2sAddr, 0);
			}
			else
			{
				I2SFifoWrite(i2sAddr, 0); //Left channel
				I2SFifoWrite(i2sAddr, 0); //Right channel
			}


		}
		if (XUartPs_IsReceiveData(uartAddr))
		{
			/* Store the first character in the UART recieve FIFO */
			userInput = XUartPs_ReadReg(uartAddr, XUARTPS_FIFO_OFFSET);
		}
	}

	Xil_Out32(i2sAddr + I2S_CTRL_REG, 0b00); //Disable TX Fifo

	return XST_SUCCESS;
}
/* ------------------------------------------------------------ */


/*
 * 录音例程
 * 参数定义：
 * i2sAddr 物理地址
 * useMic 0为line in 1为mic in
 * 功能是将录音结果放入内存
 */
int AudioRec(u32 i2sAddr, u8 useMic)
{
	int Status = 0;
	int index = 0;

	if (useMic)
		Status = AudioRegSet(&Iic, 4, 0b000010100);
	else
		Status = AudioRegSet(&Iic, 4, 0b000010000);

	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	xil_printf("\n\r");
	xil_printf("\n\r");
	xil_printf("Recording...");

	Xil_Out32(i2sAddr + I2S_RESET_REG, 0b100); //复位 RX Fifo
	Xil_Out32(i2sAddr + I2S_CTRL_REG, 0b010); //使能 RX Fifo, 使能静音 mute

	for (index = 0; index < REC_SAMPLES ; index++)
	{
		recDataL[index] = I2SFifoRead(i2sAddr);
		recDataR[index] = I2SFifoRead(i2sAddr);
	}

	Xil_Out32(i2sAddr + I2S_CTRL_REG, 0b00); //Disable RX Fifo
	xil_printf("Done!\n\r");

	TimerDelay(500000);

	recordingValid = 1;

	return XST_SUCCESS;
}
extern cplx_data_t* stim_buf;
short lastsample;
void AudioRecIn()
{
	int index;
	AudioRegSet(&Iic, 4, 0b000010100);
	Xil_Out32(XPAR_AXI_I2S_ADI_0_S_AXI_BASEADDR + I2S_RESET_REG, 0b100); //复位 RX Fifo
	Xil_Out32(XPAR_AXI_I2S_ADI_0_S_AXI_BASEADDR + I2S_CTRL_REG, 0b010); //使能 RX Fifo, 使能静音 mute

	for (index = 0; index < FFT_MAX_NUM_PTS ; index++)
	{
		stim_buf[index].data_re = (short)(I2SFifoRead(XPAR_AXI_I2S_ADI_0_S_AXI_BASEADDR)>>17);
		stim_buf[index].data_re += (short)(I2SFifoRead(XPAR_AXI_I2S_ADI_0_S_AXI_BASEADDR)>>17);
		if(stim_buf[index].data_re<16384&&stim_buf[index].data_re>-16384)
		{
			lastsample=stim_buf[index].data_re;
		}else{
			stim_buf[index].data_re=lastsample;
		}
	}
	Xil_Out32(XPAR_AXI_I2S_ADI_0_S_AXI_BASEADDR + I2S_CTRL_REG, 0b00); //Disable RX Fifo

}
/*
 * 回放录音，同理播放单音
 */
int AudioPlayRec(u32 i2sAddr)
{
	int index = 0;

	xil_printf("\n\r");
	xil_printf("\n\r");
	xil_printf("Playing...");

	/*
	 * Reset TX FIFO and enable it
	 */
	Xil_Out32(i2sAddr + I2S_RESET_REG, 0b010); //Reset TX Fifo
	Xil_Out32(i2sAddr + I2S_CTRL_REG, 0b001); //Enable TX Fifo, disable mute

	for (index = 0; index < REC_SAMPLES ; index++)
	{
		I2SFifoWrite(i2sAddr, recDataL[index]);
		I2SFifoWrite(i2sAddr, recDataR[index]);
	}

	Xil_Out32(i2sAddr + I2S_CTRL_REG, 0b00); //Disable TX Fifo
	xil_printf("Done!\n\r");

	TimerDelay(500000);

	return XST_SUCCESS;
}
//以下是底层操作，别改
/* ------------------------------------------------------------ */

/***	AudioRegSet(XIicPs *IIcPtr, u8 regAddr, u16 regData)
**
**	Parameters:
**		IIcPtr - Pointer to the initialized XIicPs struct
**		regAddr - Register in the SSM2603 to write to
**		regData - Data to write to the register (lower 9 bits are used)
**
**	Return Value: int
**		XST_SUCCESS if successful
**
**	Errors:
**
**	Description:
**		Writes a value to a register in the SSM2603 device over IIC.
**
*/
int AudioRegSet(XIicPs *IIcPtr, u8 regAddr, u16 regData)
{
	int Status;
	u8 SendBuffer[2];

	SendBuffer[0] = regAddr << 1;
	SendBuffer[0] = SendBuffer[0] | ((regData >> 8) & 0b1);

	SendBuffer[1] = regData & 0xFF;

	Status = XIicPs_MasterSendPolled(IIcPtr, SendBuffer,
				 2, IIC_SLAVE_ADDR);
	if (Status != XST_SUCCESS) {
		xil_printf("IIC send failed\n\r");
		return XST_FAILURE;
	}
	/*
	 * Wait until bus is idle to start another transfer.
	 */
	while (XIicPs_BusIsBusy(IIcPtr)) {
		/* NOP */
	}
	return XST_SUCCESS;

}
/* ------------------------------------------------------------ */

/***	I2SFifoWrite (u32 i2sBaseAddr, u32 audioData)
**
**	Parameters:
**		i2sBaseAddr - Physical Base address of the I2S controller
**		audioData - Audio data to be written to FIFO
**
**	Return Value: none
**
**	Errors:
**
**	Description:
**		Blocks execution until space is available in the I2S TX fifo, then
**		writes data to it.
**
*/
void I2SFifoWrite (u32 i2sBaseAddr, u32 audioData)
{
	while ((Xil_In32(i2sBaseAddr + I2S_FIFO_STS_REG)) & 0b0010)
	{}
	Xil_Out32(i2sBaseAddr + I2S_TX_FIFO_REG, audioData);
}
/* ------------------------------------------------------------ */

/***	I2SFifoRead (u32 i2sBaseAddr)
**
**	Parameters:
**		i2sBaseAddr - Physical Base address of the I2S controller
**
**	Return Value: u32
**		Audio data from the I2S RX FIFO
**
**	Errors:
**
**	Description:
**		Blocks execution until data is available in the I2S RX fifo, then
**		reads it out.
**
*/
u32 I2SFifoRead (u32 i2sBaseAddr)
{
	while ((Xil_In32(i2sBaseAddr + I2S_FIFO_STS_REG)) & 0b0100)
	{}
	return Xil_In32(i2sBaseAddr + I2S_RX_FIFO_REG);
}
/* ------------------------------------------------------------ */

/************************************************************************/
