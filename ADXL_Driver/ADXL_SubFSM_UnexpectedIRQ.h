/*
 * ADXL_SubFSM_UnexpectedIRQ.h
 *
 *  Created on: Jul 6, 2026
 *      Author: Karol
 */

#ifndef INC_ADXL_SUBFSM_UNEXPECTEDIRQ_H_
#define INC_ADXL_SUBFSM_UNEXPECTEDIRQ_H_

#include "ADXL_FSM.h"

void Stream_UnexpectedIRQReset();
void Stream_UnexpectedIRQSubFsmInit(fsm_set_event_callback event_cb);
FSM_ret ADXL_FSMUnexpectedIRQ_ProcessEvent(FsmEvent_t *user_event);
ADXL_Errors_t ADXL_FSMUnexpectedIRQ_GetError();

#endif /* INC_ADXL_SUBFSM_UNEXPECTEDIRQ_H_ */
