#include "xil_types.h"
#include "display_ctrl.h"
#include "audio_demo.h"
#include "timer_ps.h"
#include "xparameters.h"
#include "xuartps.h"
#include "xil_cache.h"
#include "fftdma.h"

/*
 * 设备宏定义
 */
#define VGA_BASEADDR XPAR_AXI_DISPCTRL_0_S_AXI_BASEADDR
#define VGA_VDMA_ID XPAR_AXIVDMA_0_DEVICE_ID
#define AUDIO_IIC_ID XPAR_XIICPS_0_DEVICE_ID
#define AUDIO_CTRL_BASEADDR XPAR_AXI_I2S_ADI_0_S_AXI_BASEADDR
#define SCU_TIMER_ID XPAR_SCUTIMER_DEVICE_ID
#define UART_BASEADDR XPAR_PS7_UART_1_BASEADDR
#define SW_BASEADDR XPAR_SWS_4BITS_BASEADDR
#define BTN_BASEADDR XPAR_BTNS_4BITS_BASEADDR

/*
 * 全局变量
 */

DisplayCtrl vgaCtrl;//显示控制结构体
u32 vgaBuf[1920*1080];//帧显存数组

int main(void)
{
	u32 *vgaPtr=vgaBuf;

	//初始化VGA，声卡和定时器
	TimerInitialize(SCU_TIMER_ID);

	DisplayInitialize(&vgaCtrl, VGA_VDMA_ID, VGA_BASEADDR, DISPLAY_NOT_HDMI, vgaPtr, 1920 * 4);
	DisplayStart(&vgaCtrl);

	AudioInitialize(SCU_TIMER_ID, AUDIO_IIC_ID, AUDIO_CTRL_BASEADDR);

	/*
	 * 初始化VGA显示结构体，设置显示模式
	 */
	DisplayStop(&vgaCtrl);
	DisplaySetMode(&vgaCtrl, &VMODE_1280x1024);
	DisplayStart(&vgaCtrl);
	//更新显示内容
	refresh_frame();
	FFTDMA_Init();
	FFT_Config();
	set_colorpattern();
	TimerDelay(5000);
	FFTDMA_test();

	while(1)
	{
		AudioRecIn();
		FFT_Transfer();
		VGAResOut();
	}

	return 0;
}



