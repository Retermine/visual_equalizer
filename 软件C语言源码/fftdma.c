/*
 * fftdma.c
 *
 *  Created on: 2019年4月19日
 *      Author: guosheng
 *      参考了xaxidma_example_simple_intr.c，实现dma传输音频数据到fft核再传输回来的功能
 */

#include "xaxidma.h"
#include "xparameters.h"
#include "xil_exception.h"
#include "xdebug.h"
#include "xscugic.h"
#include "fftdma.h"
#include "xgpio.h"
#include "stim.h"
static XAxiDma AxiDma;		/* Instance of the XAxiDma */

static INTC Intc;	/* Instance of the Interrupt Controller */
float Hann[FFT_MAX_NUM_PTS];
float Blackman[FFT_MAX_NUM_PTS];
cplx_data_t* stim_buf=(cplx_data_t*)TX_BUFFER_BASE;
cplx_data_t* result_buf=(cplx_data_t*)RX_BUFFER_BASE;
/*
 * 确定中断状态的标志位
 */
volatile int TxDone;
volatile int RxDone;
volatile int Error;
/*
 * 数据内存指针
 */

//初始化dma外设
void FFTDMA_Init()
{
	int Status;
	XAxiDma_Config *Config;

	//初始化DMA外设
	Config = XAxiDma_LookupConfig(DMA_DEV_ID);

	Status = XAxiDma_CfgInitialize(&AxiDma, Config);

	XAxiDma_Reset(&AxiDma);
	while (!XAxiDma_ResetIsDone(&AxiDma)) {}

	//设置中断
	XScuGic_Config *IntcConfig;

	IntcConfig = XScuGic_LookupConfig(INTC_DEVICE_ID);

	Status = XScuGic_CfgInitialize(&Intc, IntcConfig,
					IntcConfig->CpuBaseAddress);



	XScuGic_SetPriorityTriggerType(&Intc, TX_INTR_ID, 0xA0, 0x3);

	XScuGic_SetPriorityTriggerType(&Intc, RX_INTR_ID, 0xA8, 0x3);

	/*
	 * 连接设备驱动中断函数，中断函数具体实现在下面
	 */
	Status = XScuGic_Connect(&Intc, TX_INTR_ID,
				(Xil_InterruptHandler)TxIntrHandler,
				&AxiDma);


	Status = XScuGic_Connect(&Intc, RX_INTR_ID,
				(Xil_InterruptHandler)RxIntrHandler,
				&AxiDma);


	XScuGic_Enable(&Intc, TX_INTR_ID);
	XScuGic_Enable(&Intc, RX_INTR_ID);

	//使能硬件中断

	Xil_ExceptionInit();
	Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT,
			(Xil_ExceptionHandler)INTC_HANDLER,
			(void *)&Intc);

	Xil_ExceptionEnable();


	/* Disable all interrupts before setup */

	XAxiDma_IntrDisable(&AxiDma, XAXIDMA_IRQ_ALL_MASK,
						XAXIDMA_DMA_TO_DEVICE);

	XAxiDma_IntrDisable(&AxiDma, XAXIDMA_IRQ_ALL_MASK,
				XAXIDMA_DEVICE_TO_DMA);


	/* Enable all interrupts */
	XAxiDma_IntrEnable(&AxiDma, XAXIDMA_IRQ_ALL_MASK,
							XAXIDMA_DMA_TO_DEVICE);


	XAxiDma_IntrEnable(&AxiDma, XAXIDMA_IRQ_ALL_MASK,
							XAXIDMA_DEVICE_TO_DMA);
}
//测试dma传输
void FFTDMA_test()
{

	int Status,Index,Value;

	//初始化标志位
	TxDone = 0;
	RxDone = 0;
	Error = 0;

	Value = 0;
	//初始化发送内容
	memcpy(stim_buf, sine_waves, sizeof(cplx_data_t)*FFT_MAX_NUM_PTS);

	// 在数据Cache使能情况下，传输前刷新buffer数据
	Xil_DCacheFlushRange((UINTPTR)stim_buf, MAX_BUF_LEN);
	Xil_DCacheFlushRange((UINTPTR)result_buf, MAX_BUF_LEN);


	//发送数据
	Status = XAxiDma_SimpleTransfer(&AxiDma,(UINTPTR) stim_buf,
				MAX_BUF_LEN, XAXIDMA_DMA_TO_DEVICE);
 	Status = XAxiDma_SimpleTransfer(&AxiDma,(UINTPTR) result_buf,
				MAX_BUF_LEN, XAXIDMA_DEVICE_TO_DMA);

	//等待传输完成
	while ( !(RxDone&&TxDone) ) {
			/* NOP */
	}

	if (Error) {
		//错误处理

	}

	xil_printf("测试 FFT \r\n");
	for(Index = 0; Index < FFT_MAX_NUM_PTS; Index++) {
		xil_printf("Index %d: Rx= %d i+%d \r\n",Index, result_buf[Index].data_im,result_buf[Index].data_re);

	}


}
void FFT_Transfer()
{
	int index;
	// 在数据Cache使能情况下，传输前刷新buffer数据
	for (index = 0; index < FFT_MAX_NUM_PTS ; index++)
	{
		stim_buf[index].data_re *= Hann[index];
	}
	Xil_DCacheFlushRange((UINTPTR)stim_buf, MAX_BUF_LEN);
	Xil_DCacheFlushRange((UINTPTR)result_buf, MAX_BUF_LEN);


	//发送数据
	XAxiDma_SimpleTransfer(&AxiDma,(UINTPTR) stim_buf,
				MAX_BUF_LEN, XAXIDMA_DMA_TO_DEVICE);
 	XAxiDma_SimpleTransfer(&AxiDma,(UINTPTR) result_buf,
				MAX_BUF_LEN, XAXIDMA_DEVICE_TO_DMA);

	//等待传输完成
	while ( !(RxDone&&TxDone) ) {
			/* NOP */
	}

}
void FFT_Config()
{
	int index;
	XGpio gpio_fftconfig;
	XGpio_Initialize(&gpio_fftconfig, XPAR_AXI_GPIO_0_DEVICE_ID);
	XGpio_SetDataDirection(&gpio_fftconfig,1,0);
	XGpio_DiscreteWrite(&gpio_fftconfig, 1, ((0x1AA)<<1)|0x1);

	for (index = 0; index < FFT_MAX_NUM_PTS ; index++)
	{
		stim_buf[index].data_im =0;//虚部为0
		//加窗
		//汉宁窗
		Hann[index] = 0.5f-0.5f*cos(6.28318f*index/(FFT_MAX_NUM_PTS-1));
		//布莱克曼窗
//		Blackman[index] = 0.42f - 0.5f*cosf(6.28318f*index/(FFT_MAX_NUM_PTS-1));
//		Blackman[index] += 0.08*cosf(6.28318f*2*index/(FFT_MAX_NUM_PTS-1));
	}

}
/*
 * dma发送中断函数
 * 从硬件获得中断状态并应答，如果发生错误复位硬件
 * 如果完成一次中断，设置TxDone标志位
 */
static void TxIntrHandler(void *Callback)
{

	u32 IrqStatus;
	int TimeOut;
	XAxiDma *AxiDmaInst = (XAxiDma *)Callback;

	//读取中断状态
	IrqStatus = XAxiDma_IntrGetIrq(AxiDmaInst, XAXIDMA_DMA_TO_DEVICE);

	//应答中断
	XAxiDma_IntrAckIrq(AxiDmaInst, IrqStatus, XAXIDMA_DMA_TO_DEVICE);

	//忽略未通过断言的中断事件
	if (!(IrqStatus & XAXIDMA_IRQ_ALL_MASK)) {

		return;
	}

	//错误处理，复位硬件
	if ((IrqStatus & XAXIDMA_IRQ_ERROR_MASK)) {

		Error = 1;

		/*
		 * Reset should never fail for transmit channel
		 */
		XAxiDma_Reset(AxiDmaInst);

		TimeOut = RESET_TIMEOUT_COUNTER;

		while (TimeOut) {
			if (XAxiDma_ResetIsDone(AxiDmaInst)) {
				break;
			}

			TimeOut -= 1;
		}

		return;
	}

	//正常处理，设置标志位
	if ((IrqStatus & XAXIDMA_IRQ_IOC_MASK)) {

		TxDone = 1;
	}
}

/*
 * dma接收中断函数
 * 从硬件获得中断状态并应答，如果发生错误复位硬件
 * 如果完成一次中断，设置RxDone标志位
 */
static void RxIntrHandler(void *Callback)
{
	u32 IrqStatus;
	int TimeOut;
	XAxiDma *AxiDmaInst = (XAxiDma *)Callback;

	/* Read pending interrupts */
	IrqStatus = XAxiDma_IntrGetIrq(AxiDmaInst, XAXIDMA_DEVICE_TO_DMA);

	/* Acknowledge pending interrupts */
	XAxiDma_IntrAckIrq(AxiDmaInst, IrqStatus, XAXIDMA_DEVICE_TO_DMA);

	/*
	 * If no interrupt is asserted, we do not do anything
	 */
	if (!(IrqStatus & XAXIDMA_IRQ_ALL_MASK)) {
		return;
	}

	/*
	 * If error interrupt is asserted, raise error flag, reset the
	 * hardware to recover from the error, and return with no further
	 * processing.
	 */
	if ((IrqStatus & XAXIDMA_IRQ_ERROR_MASK)) {

		Error = 1;

		/* Reset could fail and hang
		 * NEED a way to handle this or do not call it??
		 */
		XAxiDma_Reset(AxiDmaInst);

		TimeOut = RESET_TIMEOUT_COUNTER;

		while (TimeOut) {
			if(XAxiDma_ResetIsDone(AxiDmaInst)) {
				break;
			}

			TimeOut -= 1;
		}

		return;
	}

	/*
	 * If completion interrupt is asserted, then set RxDone flag
	 */
	if ((IrqStatus & XAXIDMA_IRQ_IOC_MASK)) {

		RxDone = 1;
	}
}
