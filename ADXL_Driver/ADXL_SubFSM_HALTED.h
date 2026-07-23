/*
 * ADXL_SunFSM_HALTED.h
 *
 *  Created on: Jun 28, 2026
 *      Author: Karol
 */

#ifndef INC_ADXL_SUBFSM_HALTED_H_
#define INC_ADXL_SUBFSM_HALTED_H_


#include "fsm.h"
#include "ADXL_FSM.h"
#include "ADXL_defs.h"

void Stream_HaltedReset();
void Stream_HaltedSubFsmInit(fsm_set_event_callback event_cb, uint8_t evt_tmr_id);
FSM_ret ADXL_FSMHalted_ProcessEvent(FsmEvent_t *user_event);
ADXL_Errors_t ADXL_FSMHalted_GetError();


#endif /* INC_ADXL_SUBFSM_HALTED_H_ */
