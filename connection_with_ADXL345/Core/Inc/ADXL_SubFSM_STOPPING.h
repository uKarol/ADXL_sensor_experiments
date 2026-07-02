/*
 * ADXL_SubFSM_STOPPING.h
 *
 *  Created on: Jul 1, 2026
 *      Author: Karol
 */

#ifndef INC_ADXL_SUBFSM_STOPPING_H_
#define INC_ADXL_SUBFSM_STOPPING_H_

#include "fsm.h"
#include "ADXL_FSM.h"
#include "ADXL_defs.h"
#include "ADXL_driver.h"

void Stream_StoppingSubFsmInit(fsm_set_event_callback event_cb);
FSM_ret ADXL_FSMStopping_ProcessEvent(FsmEvent_t *user_event);
ADXL_Errors_t ADXL_FSMStopping_GetError();

#endif /* INC_ADXL_SUBFSM_STOPPING_H_ */
