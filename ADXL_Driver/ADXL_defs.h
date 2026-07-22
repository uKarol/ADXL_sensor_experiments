/*
 * ADXL_defs.h
 *
 *  Created on: Jun 9, 2026
 *      Author: Karol
 */

#ifndef INC_ADXL_DEFS_H_
#define INC_ADXL_DEFS_H_

#include <stdint.h>

#define ADEXL_ID (0x53U<<1U)

#define DATAX0_REG 0x32U
#define DATAX1_REG 0x33U
#define DATAY0_REG 0x34U
#define DATAY1_REG 0x35U
#define DATAZ0_REG 0x36U
#define DATAZ1_REG 0x37U
#define OFSX_REG 0x1E
#define OFSY_REG 0x1F
#define OFSZ_REG 0x20
#define DATA_FORMAT_REG 0x31U
#define BW_RATE_REG 0x2CU

#define INT_ENABLE_REG 0x2E
#define INT_MAP_REG 0x2F
#define INT_SOURCE 0x30

#define FIFO_CTL 0x38
#define FIFO_STATUS 0x39

#define FIFO_CTL_SAMPLES_MASK 0x1F
// adxl registers

#define POWER_CTL 0x2DU
#define FIFO_ENTRIES_BIT_MSK 0x3FU
// adxl bits
#define POWER_CTL_MEASURE (1U<<3U)

#define ADXL_INT_ENABLE_WATERMARK (1U<<1)
#define ADXL_INT_ENABLE_OVERRUN (1U<<0)

#define ADXL_INT_SOURCE_WATERMARK (1U<<1)
#define ADXL_INT_SOURCE_OVERRUN (1U<<0)

#define ADXL_FIFO_CTL_STREAM (1U<<7)
#define ADFL_FIFO_16_SAMPLES (1U<<4)
#define ADFL_FIFO_8_SAMPLES (1U<<3)

#define MAX_NUMBER_OF_SAMPLES 32U // number of samples per watermark
#define MAX_READOUT_SIZE (ONE_SAMPLE_SIZE * MAX_NUMBER_OF_SAMPLES)


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
	ADXL_ERR_UNEXPECTED_REG_VAL,	// not recoverable, reinit required
	ADXL_ERR_UNEXPECTED_WATERMARK,	// recoverable
	ADXL_ERR_QUEUE_FAILURE,			// not recoverable
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
	STREAM_HALTED,
	STREAM_WAITING,
	STREAM_STOPPING,
	STREAM_FLUSHING,
	STREAM_UNEXPECTED_IRQ,
}ADXL_StreamStatus;

typedef void (*adxl_external_callback)(void);

typedef struct
{
	adxl_external_callback adxl_started_callback;
	adxl_external_callback adxl_stopped_callback;
	adxl_external_callback adxl_completed_callback;
	adxl_external_callback adxl_error_detected_callback;
}helper_external_callbacks;

typedef struct
{
	uint8_t FifoSamples;
	helper_external_callbacks *helper_callbacks;
}ADXL_Init_t;

#define ONE_SAMPLE_SIZE 6U // sample size in bytes

#endif /* INC_ADXL_DEFS_H_ */
