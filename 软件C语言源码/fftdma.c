/*
 * fftdma.c
 *
 *  Created on: 2019��4��19��
 *      Author: guosheng
 *      �ο���xaxidma_example_simple_intr.c��ʵ��dma������Ƶ���ݵ�fft���ٴ�������Ĺ���
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
 * ȷ���ж�״̬�ı�־λ
 */
volatile int TxDone;
volatile int RxDone;
volatile int Error;
/*
 * �����ڴ�ָ��
 */

//��ʼ��dma����
void FFTDMA_Init()
{
	int Status;
	XAxiDma_Config *Config;

	//��ʼ��DMA����
	Config = XAxiDma_LookupConfig(DMA_DEV_ID);

	Status = XAxiDma_CfgInitialize(&AxiDma, Config);

	XAxiDma_Reset(&AxiDma);
	while (!XAxiDma_ResetIsDone(&AxiDma)) {}

	//�����ж�
	XScuGic_Config *IntcConfig;

	IntcConfig = XScuGic_LookupConfig(INTC_DEVICE_ID);

	Status = XScuGic_CfgInitialize(&Intc, IntcConfig,
					IntcConfig->CpuBaseAddress);



	XScuGic_SetPriorityTriggerType(&Intc, TX_INTR_ID, 0xA0, 0x3);

	XScuGic_SetPriorityTriggerType(&Intc, RX_INTR_ID, 0xA8, 0x3);

	/*
	 * �����豸�����жϺ������жϺ�������ʵ��������
	 */
	Status = XScuGic_Connect(&Intc, TX_INTR_ID,
				(Xil_InterruptHandler)TxIntrHandler,
				&AxiDma);


	Status = XScuGic_Connect(&Intc, RX_INTR_ID,
				(Xil_InterruptHandler)RxIntrHandler,
				&AxiDma);


	XScuGic_Enable(&Intc, TX_INTR_ID);
	XScuGic_Enable(&Intc, RX_INTR_ID);

	//ʹ��Ӳ���ж�

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
//����dma����
void FFTDMA_test()
{

	int Status,Index,Value;

	//��ʼ����־λ
	TxDone = 0;
	RxDone = 0;
	Error = 0;

	Value = 0;
	//��ʼ����������
	memcpy(stim_buf, sine_waves, sizeof(cplx_data_t)*FFT_MAX_NUM_PTS);

	// ������Cacheʹ������£�����ǰˢ��buffer����
	Xil_DCacheFlushRange((UINTPTR)stim_buf, MAX_BUF_LEN);
	Xil_DCacheFlushRange((UINTPTR)result_buf, MAX_BUF_LEN);


	//��������
	Status = XAxiDma_SimpleTransfer(&AxiDma,(UINTPTR) stim_buf,
				MAX_BUF_LEN, XAXIDMA_DMA_TO_DEVICE);
 	Status = XAxiDma_SimpleTransfer(&AxiDma,(UINTPTR) result_buf,
				MAX_BUF_LEN, XAXIDMA_DEVICE_TO_DMA);

	//�ȴ��������
	while ( !(RxDone&&TxDone) ) {
			/* NOP */
	}

	if (Error) {
		//������

	}

	xil_printf("���� FFT \r\n");
	for(Index = 0; Index < FFT_MAX_NUM_PTS; Index++) {
		xil_printf("Index %d: Rx= %d i+%d \r\n",Index, result_buf[Index].data_im,result_buf[Index].data_re);

	}


}
void FFT_Transfer()
{
	int index;
	// ������Cacheʹ������£�����ǰˢ��buffer����
	for (index = 0; index < FFT_MAX_NUM_PTS ; index++)
	{
		stim_buf[index].data_re *= Hann[index];
	}
	Xil_DCacheFlushRange((UINTPTR)stim_buf, MAX_BUF_LEN);
	Xil_DCacheFlushRange((UINTPTR)result_buf, MAX_BUF_LEN);


	//��������
	XAxiDma_SimpleTransfer(&AxiDma,(UINTPTR) stim_buf,
				MAX_BUF_LEN, XAXIDMA_DMA_TO_DEVICE);
 	XAxiDma_SimpleTransfer(&AxiDma,(UINTPTR) result_buf,
				MAX_BUF_LEN, XAXIDMA_DEVICE_TO_DMA);

	//�ȴ��������
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
		stim_buf[index].data_im =0;//�鲿Ϊ0
		//�Ӵ�
		//������
		Hann[index] = 0.5f-0.5f*cos(6.28318f*index/(FFT_MAX_NUM_PTS-1));
		//����������
//		Blackman[index] = 0.42f - 0.5f*cosf(6.28318f*index/(FFT_MAX_NUM_PTS-1));
//		Blackman[index] += 0.08*cosf(6.28318f*2*index/(FFT_MAX_NUM_PTS-1));
	}

}
/*
 * dma�����жϺ���
 * ��Ӳ������ж�״̬��Ӧ�������������λӲ��
 * ������һ���жϣ�����TxDone��־λ
 */
static void TxIntrHandler(void *Callback)
{

	u32 IrqStatus;
	int TimeOut;
	XAxiDma *AxiDmaInst = (XAxiDma *)Callback;

	//��ȡ�ж�״̬
	IrqStatus = XAxiDma_IntrGetIrq(AxiDmaInst, XAXIDMA_DMA_TO_DEVICE);

	//Ӧ���ж�
	XAxiDma_IntrAckIrq(AxiDmaInst, IrqStatus, XAXIDMA_DMA_TO_DEVICE);

	//����δͨ�����Ե��ж��¼�
	if (!(IrqStatus & XAXIDMA_IRQ_ALL_MASK)) {

		return;
	}

	//��������λӲ��
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

	//�����������ñ�־λ
	if ((IrqStatus & XAXIDMA_IRQ_IOC_MASK)) {

		TxDone = 1;
	}
}

/*
 * dma�����жϺ���
 * ��Ӳ������ж�״̬��Ӧ�������������λӲ��
 * ������һ���жϣ�����RxDone��־λ
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
