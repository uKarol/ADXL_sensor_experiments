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
	MEASURE_PROCESSING,
	MEASURE_ERROR
}measurement_state_t;

typedef enum
{
	ADXL_NO_ERROR,
	ADXL_INIT_FAILURE,
	ADXL_READ_FAILURE,
}measurement_error_t;

typedef struct
{
	measurement_error_t current_error;
	uint8_t measure_ctr;
	measurement_state_t current_state;
}MeasurementFSM_context_t;

void MeasurementFSM_setup(MeasurementFSM_context_t *context);
void MeasurementFSM_run(MeasurementFSM_context_t *context);

#endif /* INC_MEASUREMENTFSM_H_ */
