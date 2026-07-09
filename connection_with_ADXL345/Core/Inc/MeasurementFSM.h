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
	ADXL_MEAS_NO_ERROR,
	ADXL_INIT_FAILURE,
	ADXL_READ_FAILURE,
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

void MeasurementFSM_setup(MeasurementFSM_context_t *context, MeasurementInitStruct *init_data);
void MeasurementFSM_run(MeasurementFSM_context_t *context);

#endif /* INC_MEASUREMENTFSM_H_ */
