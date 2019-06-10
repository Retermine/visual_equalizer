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
