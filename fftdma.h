/*
 * fftdma.h
 *
 *  Created on: 2019年5月24日
 *      Author: dell
 */

#ifndef SRC_FFTDMA_H_
#define SRC_FFTDMA_H_

#define DMA_DEV_ID		XPAR_AXIDMA_0_DEVICE_ID

//配置存储地址空间
#define MEM_BASE_ADDR		(XPAR_PS7_DDR_0_S_AXI_BASEADDR + 0x1000000)


#define RX_INTR_ID		XPAR_FABRIC_AXIDMA_0_S2MM_INTROUT_VEC_ID
#define TX_INTR_ID		XPAR_FABRIC_AXIDMA_0_MM2S_INTROUT_VEC_ID

#define TX_BUFFER_BASE		(MEM_BASE_ADDR + 0x00100000)
#define RX_BUFFER_BASE		(MEM_BASE_ADDR + 0x00300000)
#define RX_BUFFER_HIGH		(MEM_BASE_ADDR + 0x004FFFFF)

#define INTC_DEVICE_ID          XPAR_SCUGIC_SINGLE_DEVICE_ID

#define INTC		XScuGic
#define INTC_HANDLER	XScuGic_InterruptHandler


/* Timeout loop counter for reset
 */
#define RESET_TIMEOUT_COUNTER	10000

#define TEST_START_VALUE	0xC
/*
 * Buffer and Buffer Descriptor related constant definition
 */
#define MAX_BUF_LEN		512*4
#define FFT_MAX_NUM_PTS 512
typedef struct cplx_data
{
	short data_re;
	short data_im;
} cplx_data_t;

/************************** Variable Definitions *****************************/
/*
 * Device instance definitions
 */

static void TxIntrHandler(void *Callback);
static void RxIntrHandler(void *Callback);

void FFTDMA_Init();
void FFTDMA_test();
void FFT_Transfer();
void FFT_Config();



#endif /* SRC_FFTDMA_H_ */
