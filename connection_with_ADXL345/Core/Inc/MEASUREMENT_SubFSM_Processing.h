/*
 * MEASUREMENT_SubFSM_Processing.h
 *
 *  Created on: Jul 14, 2026
 *      Author: Karol
 */

#ifndef INC_MEASUREMENT_SUBFSM_PROCESSING_H_
#define INC_MEASUREMENT_SUBFSM_PROCESSING_H_

#include "fsm.h"
#include "MeasurementFSM.h"

typedef void(*measurement_set_event_callback)(MeasurementEvt_t evt); 

void MeasurementProcessingSubFsm_Reset();
measurement_error_t MeasurementProcessingSubFsm_GetError();
FSM_ret MeasurementProcessingSubFsm_ProcessEvent(FsmEvent_t *user_event);
void MeasurementProcessingSubFsm_Init(measurement_set_event_callback event_cb, uint8_t number_of_fifo_samples);

#endif /* INC_MEASUREMENT_SUBFSM_PROCESSING_H_ */
