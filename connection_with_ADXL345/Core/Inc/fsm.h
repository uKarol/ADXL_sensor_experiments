/*
 * fsm.h
 *
 *  Created on: Jun 25, 2026
 *      Author: Karol
 */

#ifndef INC_FSM_H_
#define INC_FSM_H_

#include <stdint.h>
#include <stdlib.h>
typedef enum
{
    FSM_OK,
    FSM_ERROR,
}FSM_ret;

typedef enum
{
    FSM_INITIAL_EVENT = 0,
    FSM_BASIC_EVENT_NUM,
}FSM_Basic_Event;

typedef struct
{
	uint8_t user_event;
	void *user_data;
}FsmEvent_t;

typedef struct fsm_context fsm_context;

typedef void (*state_error_callback)(void);

typedef FSM_ret (*StateHandler) (fsm_context *ctx, FsmEvent_t *user_event);

struct fsm_context
{
    StateHandler current_state;
    state_error_callback error_cb;
    void *user_data;
};

void Fsm_StateTransition(fsm_context *ctx, StateHandler target_state);

void Fsm_Init(fsm_context *ctx, StateHandler initial_state , void *user_data, void *error_callback);
FSM_ret Fsm_ProcessEvent(fsm_context *ctx, FsmEvent_t *user_event);

#endif /* INC_FSM_H_ */
