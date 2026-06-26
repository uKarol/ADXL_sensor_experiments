/*
 * fsm.h
 *
 *  Created on: Jun 25, 2026
 *      Author: Karol
 */

#ifndef INC_FSM_H_
#define INC_FSM_H_

#include <stdint.h>

typedef enum
{
    FSM_OK,
    FSM_ERROR,
}FSM_ret;

typedef struct
{
	uint8_t user_event;
	void *user_data;
}FsmEvent_t;

typedef struct fsm_context fsm_context;

typedef FSM_ret (*StateHandler) (fsm_context *ctx, FsmEvent_t *user_event);

struct fsm_context
{
    StateHandler current_state;
    void *user_data;
};



void Fsm_Init(fsm_context *ctx, StateHandler initial_state , void *user_data);
FSM_ret Fsm_ProcessEvent(fsm_context *ctx, FsmEvent_t *user_event);

#endif /* INC_FSM_H_ */
