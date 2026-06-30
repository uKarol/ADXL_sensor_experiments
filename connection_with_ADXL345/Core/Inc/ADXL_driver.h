/*
 * ADXL_driver.h
 *
 *  Created on: Jun 9, 2026
 *      Author: Karol
 */

#ifndef INC_ADXL_DRIVER_H_
#define INC_ADXL_DRIVER_H_

#include <stdint.h>

typedef enum
{
	ADXL_SUCCESS,
	ADXL_FAILURE,
	ADXL_NOT_INITIALIZED
}ADXL_status_t;

typedef enum
{
	ADXL_ERR_NO_ERROR,
	ADXL_ERR_OVERRUN,				// recoverable
	ADXL_ERR_READOUT_INCOMPLETE,	// recoverable
	ADXL_ERR_COMMUNICATION_LOST,	// not recoverable, reinit required
	ADXL_ERR_DMA_PROBLEM,			// not recoverable, reinit required
	ADXL_ERR_UNEXPECTED_BEHAVIOUR,	// not recoverable, reinit required
	ADXL_ERR_UNEXPECTED_REG_VAL		// not recoverable, reinit required
}ADXL_Errors_t;

typedef enum
{
	DRIVER_NOT_INITIALIZED,
	DRIVER_READY,
	DRIVER_HALTED,
	DRIVER_ERROR
}ADXL_DriverState_t;

typedef enum
{
	STREAM_NOT_INITIALIZED,
	STREAM_COMPLETED,
	STREAM_IN_PROGRESS,
	STREAM_ERROR,
	STREAM_IDLE
}ADXL_StreamStatus;

typedef struct
{
	uint8_t FifoSamples;
}ADXL_Init_t;

#define ONE_SAMPLE_SIZE 6U // sample size in bytes

ADXL_status_t ADXL_init_default();
ADXL_status_t ADXL_ReadData(int16_t *Xdata, int16_t *Ydata, int16_t *Zdata);
ADXL_status_t ADXL_GetConfig(char *readout, uint16_t max_size );
void ADXL_INT1InterruptHandler(void);
void ADXL_FIFO_Check();
ADXL_status_t ADXL_RegInitAlternative();
ADXL_StreamStatus ADXL_GetStreamStatus(void);
void ADXL_I2CTxComplete(void);
uint8_t* ADXL_GetStreamedData(void);
void ADXL_ReleaseDataBuffer(void);
void ADXL_DMAStreamComplete(void);

ADXL_Errors_t ADXL_GetLastError(void);
ADXL_status_t ADXL_ResetDriver(void);

ADXL_status_t ADXL_StartStreamMeasurements(void);
ADXL_status_t ADXL_StopStreamMeasurements(void);

void ADXL_task();


#endif /* INC_ADXL_DRIVER_H_ */
