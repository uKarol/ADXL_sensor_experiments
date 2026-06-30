/*
 * ADXL_SubFSM_FLUSHING.h
 *
 *  Created on: Jun 28, 2026
 *      Author: Karol
 */

#ifndef INC_ADXL_SUBFSM_FLUSHING_H_
#define INC_ADXL_SUBFSM_FLUSHING_H_

void Stream_FlushingSubFsmInit(fsm_set_event_callback event_cb);
FSM_ret ADXL_FSMFlushing_ProcessEvent(FsmEvent_t *user_event);
ADXL_Errors_t ADXL_FSMFlushing_GetError();

#endif /* INC_ADXL_SUBFSM_FLUSHING_H_ */
