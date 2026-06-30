/*
 * ADXL_SubFSM_WAITING.h
 *
 *  Created on: Jun 28, 2026
 *      Author: Karol
 */

#ifndef INC_ADXL_SUBFSM_WAITING_H_
#define INC_ADXL_SUBFSM_WAITING_H_

#include "fsm.h"
#include "ADXL_FSM.h"
#include "ADXL_defs.h"
#include "ADXL_driver.h"

void Stream_WaitingSubFsmInit(fsm_set_event_callback event_cb, uint8_t fifo_samples);
FSM_ret ADXL_FSMWaiting_ProcessEvent(FsmEvent_t *user_event);
ADXL_Errors_t ADXL_FSMWaiting_GetError();

#endif /* INC_ADXL_SUBFSM_WAITING_H_ */
