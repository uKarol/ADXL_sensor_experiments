/*
 * MeasurementFSM.h
 *
 *  Created on: Jun 9, 2026
 *      Author: Karol
 */

#ifndef INC_MEASUREMENTFSM_H_
#define INC_MEASUREMENTFSM_H_

#include <stdint.h>
#include "fsm.h"

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
	MEAS_ADXL_FAILURE,
	MEAS_UNEXPECTED_EVENT,
	MEAS_RX_FAILURE,
	MEAS_TX_FAILURE,
}measurement_error_t;

enum
{
	MEASUREMENT_EVT_TX_COMPLETED = FSM_BASIC_EVENT_NUM,
	MEASUREMENT_EVT_DATA_READY,
	MEASUREMENT_EVT_RX_COMPLETED,
	MEASUREMENT_EVT_STARTED,
	MEASUREMENT_EVT_STOPPED,
	MEASUREMENT_EVT_ADXL_ERROR_DETECTED,
	MEASUREMENT_EVT_READOUT_COMPLETED,
};

typedef uint16_t MeasurementEvt_t;

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
