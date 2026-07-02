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
}StreamStoppingCtxData_t;

static StreamStoppingCtxData_t StreamStoppingFsmData;
static fsm_context StreamStoppingFsmContext;

static FSM_ret StreamStopping_EntryStateHandler (fsm_context *ctx, FsmEvent_t *user_event);
static FSM_ret StreamStopping_ResettingPowerCTL (fsm_context *ctx, FsmEvent_t *user_event);
static FSM_ret StreamStopping_WaitingStateHandler (fsm_context *ctx, FsmEvent_t *user_event);


void Stream_StoppingSubFsmInit(fsm_set_event_callback event_cb)
{
	StreamStoppingFsmData.last_error = ADXL_ERR_NO_ERROR;
	StreamStoppingFsmData.evt_callback = event_cb;
	Fsm_Init(&StreamStoppingFsmContext, StreamStopping_EntryStateHandler , &StreamStoppingFsmData);
}

FSM_ret ADXL_FSMStopping_ProcessEvent(FsmEvent_t *user_event)
{
	return Fsm_ProcessEvent(&StreamStoppingFsmContext, user_event);
}

ADXL_Errors_t ADXL_FSMStopping_GetError()
{
	return StreamStoppingFsmData.last_error;
}

static uint8_t data_in;

static FSM_ret StreamStopping_EntryStateHandler(fsm_context *ctx, FsmEvent_t *user_event)
{
	FSM_ret ret_val = FSM_OK;
	ADXL_FSM_Events current_event = (ADXL_FSM_Events)user_event->user_event;
	StreamStoppingCtxData_t *context_data = (StreamStoppingCtxData_t*)ctx->user_data;

	switch (current_event)
	{
		case ADXL_EVT_STOP_STREAM:

			if(ADXLConn_GetCurrentOperation()!= ADXL_OP_NO_OPERATION)
			{
				ctx->current_state = StreamStopping_WaitingStateHandler;
			}
			else
			{
				data_in = 0;
				if(ADXL_WriteRegNonBlocking(POWER_CTL, &data_in) == ADXL_ERR_NO_ERROR)
				{
					ctx->current_state = StreamStopping_ResettingPowerCTL;
				}
				else
				{
					ret_val = FSM_ERROR;
					context_data->last_error = ADXL_ERR_COMMUNICATION_LOST;
					ctx->current_state = StreamStopping_EntryStateHandler;
				}
				break;
			}
		default:
			ret_val = FSM_ERROR;
			context_data->last_error = ADXL_ERR_UNEXPECTED_BEHAVIOUR;
			ctx->current_state = StreamStopping_EntryStateHandler;
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
		case ADXL_EVT_I2C_TX_COMPLETED: // fallthrough
		case ADXL_EVT_I2C_RX_COMPLETED:
			data_in = 0;
			if(ADXL_WriteRegNonBlocking(POWER_CTL, &data_in) == ADXL_ERR_NO_ERROR)
			{
				ctx->current_state = StreamStopping_ResettingPowerCTL;
			}
			else
			{
				ret_val = FSM_ERROR;
				context_data->last_error = ADXL_ERR_COMMUNICATION_LOST;
				ctx->current_state = StreamStopping_EntryStateHandler;
			}
			break;
		default:
			ret_val = FSM_ERROR;
			context_data->last_error = ADXL_ERR_UNEXPECTED_BEHAVIOUR;
			ctx->current_state = StreamStopping_EntryStateHandler;
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
		case ADXL_EVT_I2C_TX_COMPLETED:
			ctx->current_state = StreamStopping_EntryStateHandler;
			context_data->evt_callback(ADXL_EVT_STREAM_HALTED);
			break;
		default:
			ret_val = FSM_ERROR;
			context_data->last_error = ADXL_ERR_UNEXPECTED_BEHAVIOUR;
			ctx->current_state = StreamStopping_EntryStateHandler;
			break;
	}
	return ret_val;
}

