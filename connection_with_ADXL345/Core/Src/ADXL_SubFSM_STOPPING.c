/*
 * ADXL_SubFSM_STOPPING.c
 *
 *  Created on: Jul 1, 2026
 *      Author: Karol
 */

#include "ADXL_SubFSM_HALTED.h"
#include "ADXL_driver.h"
#include "ADXL_FSM.h"
#include "ADXL_defs.h"
#include "fsm.h"
#include "timer_evt.h"
#include  "ADXL_i2c_conn.h"

typedef struct
{
	ADXL_Errors_t last_error;
	uint8_t dma_out_data;
	fsm_set_event_callback evt_callback;
	uint8_t data_in;
}StreamStoppingCtxData_t;

static StreamStoppingCtxData_t StreamStoppingFsmData;
static fsm_context StreamStoppingFsmContext;

static FSM_ret StreamStopping_EntryStateHandler (fsm_context *ctx, FsmEvent_t *user_event);
static FSM_ret StreamStopping_ResettingPowerCTL (fsm_context *ctx, FsmEvent_t *user_event);
static FSM_ret StreamStopping_WaitingStateHandler (fsm_context *ctx, FsmEvent_t *user_event);

void Stream_StoppingReset()
{
	StreamStoppingFsmData.last_error = ADXL_ERR_NO_ERROR;
	Fsm_StateTransition(&StreamStoppingFsmContext, StreamStopping_EntryStateHandler);
}

void FSM_StoppingErrorCallback(void)
{
	Fsm_StateTransition(&StreamStoppingFsmContext, StreamStopping_EntryStateHandler);
}

void Stream_StoppingSubFsmInit(fsm_set_event_callback event_cb)
{
	StreamStoppingFsmData.last_error = ADXL_ERR_NO_ERROR;
	StreamStoppingFsmData.evt_callback = event_cb;
	Fsm_Init(&StreamStoppingFsmContext, StreamStopping_EntryStateHandler , &StreamStoppingFsmData, FSM_StoppingErrorCallback);
}

FSM_ret ADXL_FSMStopping_ProcessEvent(FsmEvent_t *user_event)
{
	return Fsm_ProcessEvent(&StreamStoppingFsmContext, user_event);
}

ADXL_Errors_t ADXL_FSMStopping_GetError()
{
	return StreamStoppingFsmData.last_error;
}

static FSM_ret StreamStopping_EntryStateHandler(fsm_context *ctx, FsmEvent_t *user_event)
{
	FSM_ret ret_val = FSM_OK;
	ADXL_FSM_Events current_event = (ADXL_FSM_Events)user_event->user_event;
	StreamStoppingCtxData_t *context_data = (StreamStoppingCtxData_t*)ctx->user_data;

	switch (current_event)
	{
		case FSM_INITIAL_EVENT:
			break;
		case ADXL_EVT_STOP_STREAM:
			context_data->last_error = ADXL_ERR_NO_ERROR;
			if(ADXLConn_GetCurrentOperation()== ADXL_OP_NO_OPERATION)
			{
				Fsm_StateTransition(ctx, StreamStopping_ResettingPowerCTL);
			}
			else
			{
				Fsm_StateTransition(ctx, StreamStopping_WaitingStateHandler);
			}
			break;
		default:
			ret_val = FSM_ERROR;
			context_data->last_error = ADXL_ERR_UNEXPECTED_BEHAVIOUR;
			break;
	}

	return ret_val;
}

static FSM_ret StreamStopping_WaitingStateHandler(fsm_context *ctx, FsmEvent_t *user_event)
{
	FSM_ret ret_val = FSM_OK;
	ADXL_FSM_Events current_event = (ADXL_FSM_Events)user_event->user_event;
	StreamStoppingCtxData_t *context_data = (StreamStoppingCtxData_t*)ctx->user_data;

	switch (current_event)
	{
		case FSM_INITIAL_EVENT:
			break;
		case ADXL_EVT_I2C_TX_COMPLETED: // fallthrough
		case ADXL_EVT_I2C_RX_COMPLETED:
				Fsm_StateTransition(ctx, StreamStopping_ResettingPowerCTL);
			break;
		default:
			ret_val = FSM_ERROR;
			context_data->last_error = ADXL_ERR_UNEXPECTED_BEHAVIOUR;
			break;
	}
	return ret_val;
}

static FSM_ret StreamStopping_ResettingPowerCTL(fsm_context *ctx, FsmEvent_t *user_event)
{
	FSM_ret ret_val = FSM_OK;
	ADXL_FSM_Events current_event = (ADXL_FSM_Events)user_event->user_event;
	StreamStoppingCtxData_t *context_data = (StreamStoppingCtxData_t*)ctx->user_data;

	switch (current_event)
	{
		case FSM_INITIAL_EVENT:
			context_data->data_in = 0;
			if(ADXL_WriteRegNonBlocking(POWER_CTL, &(context_data->data_in)) != ADXL_ERR_NO_ERROR)
			{
				ret_val = FSM_ERROR;
				context_data->last_error = ADXL_ERR_COMMUNICATION_LOST;
				Fsm_StateTransition(ctx, StreamStopping_EntryStateHandler);
			}
			break;
		case ADXL_EVT_I2C_TX_COMPLETED:
			context_data->evt_callback(ADXL_EVT_STREAM_HALTED);
			Fsm_StateTransition(ctx, StreamStopping_EntryStateHandler);
			break;
		default:
			ret_val = FSM_ERROR;
			context_data->last_error = ADXL_ERR_UNEXPECTED_BEHAVIOUR;
			break;
	}
	return ret_val;
}

