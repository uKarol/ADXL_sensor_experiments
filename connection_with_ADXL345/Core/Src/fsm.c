/*
 * fsm.c
 *
 *  Created on: Jun 25, 2026
 *      Author: Karol
 */

#include "fsm.h"

FSM_ret Fsm_ProcessEvent(fsm_context *ctx, FsmEvent_t *user_event)
{
    return ctx->current_state(ctx, user_event);
}

void Fsm_Init(fsm_context *ctx, StateHandler initial_state , void *user_data)
{
    ctx->current_state = initial_state;
    ctx->user_data = user_data;
}
