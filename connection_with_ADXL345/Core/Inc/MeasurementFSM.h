/*
 * MeasurementFSM.h
 *
 *  Created on: Jun 9, 2026
 *      Author: Karol
 */

#ifndef INC_MEASUREMENTFSM_H_
#define INC_MEASUREMENTFSM_H_

#include <stdint.h>

typedef enum
{
	MEASURE_WAITING,
	MEASURE_GET_SIZE,
	MEASURE_PROCESSING,
	MEASURE_ERROR
}measurement_state_t;

typedef enum
{
	MEAS_NO_ERROR,
	MEAS_INIT_FAILURE,
	MEAS_READ_FAILURE,
}measurement_error_t;

typedef struct
{
	measurement_error_t current_error;
	uint16_t measure_ctr;
	uint16_t expected_size;
	measurement_state_t current_state;
	uint8_t number_of_fifo_samples;
}MeasurementFSM_context_t;

typedef struct
{
	uint8_t number_of_fifo_samples;
}MeasurementInitStruct;

typedef void (*measurement_err_callback)(void);

void MeasurementFSM_setup(MeasurementFSM_context_t *context, MeasurementInitStruct *init_data);
void Measurement_task();
measurement_error_t Measurement_Init(MeasurementInitStruct *init_data, measurement_err_callback error_cb);
void MeasurementTransmitCompleted();
void MeasurementRxCompleted();
void MeasurementADXL_DataReady();
#endif /* INC_MEASUREMENTFSM_H_ */
