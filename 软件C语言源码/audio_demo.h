#ifndef AUDIO_DEMO_H_
#define AUDIO_DEMO_H_

/* ------------------------------------------------------------ */
/*				Include File Definitions						*/
/* ------------------------------------------------------------ */

#include "xil_types.h"
#include "xiicps.h"

/* ------------------------------------------------------------ */
/*					Miscellaneous Declarations					*/
/* ------------------------------------------------------------ */

/* I2S Register offsets */
#define I2S_RESET_REG 		0x00
#define I2S_CTRL_REG 		0x04
#define I2S_CLK_CTRL_REG 	0x08
#define I2S_FIFO_STS_REG 	0x20
#define I2S_RX_FIFO_REG 	0x28
#define I2S_TX_FIFO_REG 	0x2C

/* IIC address of the SSM2603 device and the desired IIC clock speed */
#define IIC_SLAVE_ADDR		0b0011010
#define IIC_SCLK_RATE		100000

#define REC_TIME_SEC 5
#define REC_SAMPLES (REC_TIME_SEC * 48000) /* Sample rate of 48000Hz */

/* ------------------------------------------------------------ */
/*					Procedure Declarations						*/
/* ------------------------------------------------------------ */

int AudioInitialize(u16 timerID,  u16 iicID, u32 i2sAddr);
int AudioRegSet(XIicPs *IIcPtr, u8 regAddr, u16 regData);
void I2SFifoWrite (u32 i2sBaseAddr, u32 audioData);
u32 I2SFifoRead (u32 i2sBaseAddr);
void AudioRecIn();

/* ------------------------------------------------------------ */

/************************************************************************/

#endif /* AUDIO_DEMO_H_ */
