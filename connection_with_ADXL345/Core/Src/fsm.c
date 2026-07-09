/*
 * fsm.c
 *
 *  Created on: Jun 25, 2026
 *      Author: Karol
 */

#include "fsm.h"


FSM_ret Fsm_ProcessEvent(fsm_context *ctx, FsmEvent_t *user_event)
{
    FSM_ret ret_val = ctx->current_state(ctx, user_event);
    if((ret_val == FSM_ERROR) && (ctx->error_cb != NULL))
    {
        ctx->error_cb();
    }
    return ret_val;
}

void Fsm_StateTransition(fsm_context *ctx, StateHandler target_state)
{
    FsmEvent_t entry_event = {.user_event = FSM_INITIAL_EVENT, .user_data = NULL};
    ctx->current_state = target_state;
    ctx->current_state(ctx, &entry_event);
}

void Fsm_Init(fsm_context *ctx, StateHandler initial_state , void *user_data, void *error_callback)
{
    ctx->current_state = initial_state;
    ctx->user_data = user_data;
    ctx->error_cb = error_callback;
}
