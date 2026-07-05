/*
 * ADXL_FSM.c
 *
 *  Created on: Jun 26, 2026
 *      Author: Karol
 */

#include "fsm.h"
#include "ADXL_driver.h"
#include "ADXL_FSM.h"
#include "ADXL_i2c_conn.h"
#include "ADXL_defs.h"
#include "ADXL_SubFSM_WAITING.h"
#include "ADXL_SubFSM_FLUSHING.h"
#include "ADXL_SubFSM_HALTED.h"
#include "ADXL_SubFSM_STOPPING.h"
#include "timer_evt.h"

// main stream fsm
static FSM_ret StreamCompleted_StateHandler (fsm_context *ctx, FsmEvent_t *user_event);
static FSM_ret StreamInProgress_StateHandler (fsm_context *ctx, FsmEvent_t *user_event);
static FSM_ret StreamError_StateHandler (fsm_context *ctx, FsmEvent_t *user_event);
static FSM_ret StreamWaiting_StateHandler (fsm_context *ctx, FsmEvent_t *user_event);
static FSM_ret StreamHalted_StateHandler (fsm_context *ctx, FsmEvent_t *user_event);
static FSM_ret StreamFlushing_StateHandler (fsm_context *ctx, FsmEvent_t *user_event);
static FSM_ret StreamStopping_StateHandler (fsm_context *ctx, FsmEvent_t *user_event);

uint8_t ADXL_raw_data[MAX_READOUT_SIZE];

typedef struct
{
	uint8_t readout_num;
	uint8_t fifo_samples_num;
	ADXL_Errors_t stream_errors;
	ADXL_StreamStatus current_state;
	fsm_error_callback error_callback;
	fsm_set_event_callback evt_callback;
}StreamCtxData_t;

typedef struct
{
	ADXL_Errors_t last_error;
	uint8_t samples_num;
	uint8_t readout_ctr;
}StreamFlushingCtxData_t;

static StreamCtxData_t StreamFsmData;
static fsm_context StreamFsmContext;

static void ADXL_FSM_ErrorCallback()
{
	StreamFsmData.current_state = STREAM_ERROR;
	Fsm_StateTransition(&StreamFsmContext, StreamError_StateHandler);
}

void ADXL_TimeoutEvent()
{
	StreamFsmData.evt_callback(ADXL_EVT_TIMEOUT);
}

void ADXL_FSM_Init(uint8_t fifo_samples, fsm_error_callback error_cb, fsm_set_event_callback event_cb)
{
	StreamFsmData.evt_callback = event_cb;
	StreamFsmData.error_callback = error_cb;
	StreamFsmData.fifo_samples_num = fifo_samples;

	Stream_WaitingSubFsmInit(event_cb, fifo_samples);
	Stream_FlushingSubFsmInit(event_cb);
	Stream_HaltedSubFsmInit(event_cb);
	Stream_StoppingSubFsmInit(event_cb);
	Fsm_Init(&StreamFsmContext, StreamHalted_StateHandler , &StreamFsmData, ADXL_FSM_ErrorCallback);
	EvtTimerInit(ADXL_TimeoutEvent);
}

static FSM_ret StreamStopping_StateHandler (fsm_context *ctx, FsmEvent_t *user_event)
{
	FSM_ret ret_val = FSM_ERROR;

	ADXL_FSM_Events current_event = (ADXL_FSM_Events)user_event->user_event;
	StreamCtxData_t *context_data = (StreamCtxData_t*)ctx->user_data;

	switch(current_event)
	{
		case FSM_INITIAL_EVENT:
			user_event->user_event = ADXL_EVT_STOP_STREAM; // fallthrough
		case ADXL_EVT_STOP_STREAM: // fallthrough
		case ADXL_EVT_I2C_TX_COMPLETED: // fallthrough
		case ADXL_EVT_I2C_RX_COMPLETED:

			if(ADXL_FSMStopping_ProcessEvent(user_event) != FSM_OK )
			{
				context_data->stream_errors = ADXL_FSMStopping_GetError();
			}
			else
			{
				ret_val = FSM_OK;
			}
			break;
		case ADXL_EVT_STREAM_HALTED:
			Fsm_StateTransition(ctx, StreamHalted_StateHandler);
			ret_val = FSM_OK;
			break;
		default:
			context_data->stream_errors = ADXL_ERR_UNEXPECTED_BEHAVIOUR;
			break;

	}
	return ret_val;
}

static FSM_ret StreamHalted_StateHandler (fsm_context *ctx, FsmEvent_t *user_event)
{
	FSM_ret ret_val = FSM_ERROR;

	ADXL_FSM_Events current_event = (ADXL_FSM_Events)user_event->user_event;
	StreamCtxData_t *context_data = (StreamCtxData_t*)ctx->user_data;

	switch(current_event)
	{
		case FSM_INITIAL_EVENT:
			break;
		case ADXL_EVT_START_STREAM: // fallthrough
		case ADXL_EVT_I2C_TX_COMPLETED: // fallthrough
		case ADXL_EVT_I2C_RX_COMPLETED: // fallthrough
		case ADXL_EVT_TIMEOUT:
			if(ADXL_FSMHalted_ProcessEvent(user_event) != FSM_OK )
			{
				context_data->stream_errors = ADXL_FSMHalted_GetError();
				context_data->current_state = STREAM_ERROR;
				Fsm_StateTransition(ctx, StreamError_StateHandler);
			}
			else
			{
				ret_val = FSM_OK;
			}
			break;
		case ADXL_EVT_SENSOR_ENABLED:
			Fsm_StateTransition(ctx, StreamFlushing_StateHandler);
			ret_val = FSM_OK;
			break;
		default:
			context_data->stream_errors = ADXL_ERR_UNEXPECTED_BEHAVIOUR;
			break;

	}
	return ret_val;
}

static FSM_ret StreamFlushing_StateHandler (fsm_context *ctx, FsmEvent_t *user_event)
{
	FSM_ret ret_val = FSM_ERROR;

	ADXL_FSM_Events current_event = (ADXL_FSM_Events)user_event->user_event;
	StreamCtxData_t *context_data = (StreamCtxData_t*)ctx->user_data;

	switch(current_event)
	{
		case FSM_INITIAL_EVENT:
			user_event->user_event = ADXL_EVT_FIFO_FLUSH_REQ; // fallthough
		case ADXL_EVT_FIFO_FLUSH_REQ: // fallthrough
		case ADXL_EVT_I2C_RX_COMPLETED:
			if(ADXL_FSMFlushing_ProcessEvent(user_event) != FSM_OK )
			{
				context_data->stream_errors = ADXL_FSMFlushing_GetError();
				context_data->current_state = STREAM_ERROR;
				Fsm_StateTransition(ctx, StreamError_StateHandler);
			}
			else
			{
				ret_val = FSM_OK;
			}
			break;
		case ADXL_EVT_FIFO_CLEARED:
			Fsm_StateTransition(ctx,StreamWaiting_StateHandler);
			ret_val = FSM_OK;
			break;
		case ADXL_EVT_STOP_REQUEST:
			Fsm_StateTransition(ctx,StreamStopping_StateHandler);
			ret_val = FSM_OK;
			break;
		default:
			context_data->stream_errors = ADXL_ERR_UNEXPECTED_BEHAVIOUR;
			break;

	}
	return ret_val;
}

FSM_ret StreamWaiting_StateHandler (fsm_context *ctx, FsmEvent_t *user_event)
{
	FSM_ret ret_val = FSM_ERROR;

	ADXL_FSM_Events current_event = (ADXL_FSM_Events)user_event->user_event;
	StreamCtxData_t *context_data = (StreamCtxData_t*)ctx->user_data;

	switch(current_event)
	{
		case FSM_INITIAL_EVENT:
			break;
		case ADXL_EVT_EXTI_IRQ: // fallthrough
		case ADXL_EVT_I2C_RX_COMPLETED:
			if(ADXL_FSMWaiting_ProcessEvent(user_event) != FSM_OK )
			{
				context_data->stream_errors = ADXL_FSMWaiting_GetError();
			}
			else
			{
				ret_val = FSM_OK;
			}
			break;
		case ADXL_EVT_FIFO_READY:

			if( ADXL_ReadMultipleRegsNonBlocking(DATAX0_REG, ADXL_raw_data, ONE_SAMPLE_SIZE) == ADXL_ERR_NO_ERROR)
			{
				context_data->current_state = STREAM_IN_PROGRESS;
				ret_val = FSM_OK;
				Fsm_StateTransition(ctx, StreamInProgress_StateHandler);
			}
			else
			{
				context_data->stream_errors = ADXL_ERR_COMMUNICATION_LOST;
			}
			break;
		case ADXL_EVT_STOP_REQUEST:
			Fsm_StateTransition(ctx,StreamStopping_StateHandler);
			ret_val = FSM_OK;
			break;
		default:
			context_data->stream_errors = ADXL_ERR_UNEXPECTED_BEHAVIOUR;
			break;

	}
	return ret_val;
}


FSM_ret StreamInProgress_StateHandler (fsm_context *ctx, FsmEvent_t *user_event)
{
	FSM_ret ret_val = FSM_OK;
	ADXL_FSM_Events current_event = (ADXL_FSM_Events)user_event->user_event;
	StreamCtxData_t *context_data = (StreamCtxData_t*)ctx->user_data;
	switch(current_event)
	{
		case FSM_INITIAL_EVENT:
			break;
		case ADXL_EVT_I2C_RX_COMPLETED:

			if(context_data->readout_num == (context_data->fifo_samples_num-1))
			{
				context_data->readout_num = 0;
				context_data->current_state = STREAM_COMPLETED;
				Fsm_StateTransition(ctx, StreamCompleted_StateHandler);

			}
			else
			{
				context_data->readout_num++;

				if( ADXL_ReadMultipleRegsNonBlocking(DATAX0_REG, &(ADXL_raw_data[context_data->readout_num * ONE_SAMPLE_SIZE]), ONE_SAMPLE_SIZE) != ADXL_ERR_NO_ERROR)
				{
					ret_val = FSM_ERROR;
					context_data->current_state = STREAM_ERROR;
					context_data->stream_errors = ADXL_ERR_DMA_PROBLEM;
					Fsm_StateTransition(ctx, StreamError_StateHandler);
				}
			}
			break;
		case ADXL_EVT_STOP_REQUEST:
			Fsm_StateTransition(ctx, StreamStopping_StateHandler);
			break;
		default:
			ret_val = FSM_ERROR;
			context_data->stream_errors = ADXL_ERR_UNEXPECTED_BEHAVIOUR;
			break;

	}
	return ret_val;
}

FSM_ret StreamCompleted_StateHandler (fsm_context *ctx, FsmEvent_t *user_event)
{
	FSM_ret ret_val = FSM_OK;
	ADXL_FSM_Events current_event = (ADXL_FSM_Events)user_event->user_event;
	StreamCtxData_t *context_data = (StreamCtxData_t*)ctx->user_data;
	switch(current_event)
	{
		case FSM_INITIAL_EVENT:
			break;
		case ADXL_EVT_STREAM_FINISHED:
			break;
		case ADXL_EVT_BUFFER_RELEASE_REQ:
			context_data->current_state = STREAM_IDLE;
			Fsm_StateTransition(ctx, StreamWaiting_StateHandler);
			break;
		case ADXL_EVT_STOP_REQUEST:
			Fsm_StateTransition(ctx, StreamStopping_StateHandler);
		default:
			ret_val = FSM_ERROR;
			context_data->stream_errors = ADXL_ERR_UNEXPECTED_BEHAVIOUR;
			break;

	}
	return ret_val;
}

FSM_ret StreamError_StateHandler (fsm_context *ctx, FsmEvent_t *user_event)
{
	FSM_ret ret_val = FSM_OK;
	ADXL_FSM_Events current_event = (ADXL_FSM_Events)user_event->user_event;
	StreamCtxData_t *context_data = (StreamCtxData_t*)ctx->user_data;
	switch(current_event)
	{
		case FSM_INITIAL_EVENT:
			ret_val = FSM_ERROR;
			context_data->error_callback(context_data->stream_errors);
			break;
		case ADXL_EVT_RESET_ERROR_REQUEST:
			ret_val = FSM_OK;
			context_data->current_state = STREAM_IDLE;
			context_data->readout_num = 0;
			context_data->stream_errors = ADXL_ERR_NO_ERROR;
			Fsm_StateTransition(ctx, StreamStopping_StateHandler);
			break;

		default:
			ret_val = FSM_ERROR;
			break;
	}
	return ret_val;
}

ADXL_StreamStatus ADXL_FSM_GetStatus(void)
{
	return StreamFsmData.current_state;
}

uint8_t* ADXL_FSM_GetDataBuffer(void)
{
	uint8_t *ret_val = NULL;
	if(STREAM_COMPLETED == StreamFsmData.current_state)
	{
		ret_val = ADXL_raw_data;
	}
	return ret_val;
}

void ADXL_FSM_ProcessEvent(FsmEvent_t *user_event)
{
	(void)Fsm_ProcessEvent(&StreamFsmContext, user_event);
}

