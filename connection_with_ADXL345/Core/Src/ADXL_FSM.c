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
#include "ADXL_SubFSM_UnexpectedIRQ.h"
#include "timer_evt.h"

// main stream fsm
static FSM_ret StreamCompleted_StateHandler (fsm_context *ctx, FsmEvent_t *user_event);
static FSM_ret StreamInProgress_StateHandler (fsm_context *ctx, FsmEvent_t *user_event);
static FSM_ret StreamError_StateHandler (fsm_context *ctx, FsmEvent_t *user_event);
static FSM_ret StreamWaiting_StateHandler (fsm_context *ctx, FsmEvent_t *user_event);
static FSM_ret StreamHalted_StateHandler (fsm_context *ctx, FsmEvent_t *user_event);
static FSM_ret StreamFlushing_StateHandler (fsm_context *ctx, FsmEvent_t *user_event);
static FSM_ret StreamStopping_StateHandler (fsm_context *ctx, FsmEvent_t *user_event);
static FSM_ret StreamUnexpectedIRQ_StateHandler (fsm_context *ctx, FsmEvent_t *user_event);

uint8_t ADXL_raw_data[MAX_READOUT_SIZE];

typedef struct
{
	ADXL_Errors_t error_code;
	ADXL_StreamStatus state; // state when error occured
	ADXL_FSM_Events evt; // event when error was generated

}ADXL_Err_Desc_t;

typedef struct
{
	uint8_t readout_num;
	uint8_t fifo_samples_num;
	ADXL_Err_Desc_t stream_errors;
	ADXL_StreamStatus current_state;
	fsm_error_callback error_callback;
	fsm_set_event_callback evt_callback;
}StreamCtxData_t;


static StreamCtxData_t StreamFsmData;
static fsm_context StreamFsmContext;

void ADXL_FSM_GetLastErrorInfo(ADXL_Err_Desc_t *ErrorInfo)
{
	ErrorInfo->error_code = StreamFsmData.stream_errors.error_code;
	ErrorInfo->evt = StreamFsmData.stream_errors.evt;
	ErrorInfo->state = StreamFsmData.stream_errors.state;
}

static void ADXL_SetError(ADXL_Errors_t error_code, ADXL_StreamStatus state, ADXL_FSM_Events evt)
{
	StreamFsmData.stream_errors.error_code = error_code;
	StreamFsmData.stream_errors.state = state;
	StreamFsmData.stream_errors.evt = evt;
}

static void ADXL_StreamReset()
{
	Stream_WaitingReset();
	Stream_FlushingReset();
	Stream_StoppingReset();
	Stream_HaltedReset();
	Stream_UnexpectedIRQReset();

	StreamFsmData.readout_num = 0;
	StreamFsmData.stream_errors.error_code = ADXL_ERR_NO_ERROR;
	Fsm_StateTransition(&StreamFsmContext, StreamStopping_StateHandler);

}

static void ADXL_FSM_ErrorCallback()
{
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
	Stream_UnexpectedIRQSubFsmInit(event_cb);
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
			context_data->current_state = STREAM_STOPPING;
			user_event->user_event = ADXL_EVT_STOP_STREAM; // fallthrough
		case ADXL_EVT_STOP_STREAM: // fallthrough
		case ADXL_EVT_I2C_TX_COMPLETED: // fallthrough
		case ADXL_EVT_I2C_RX_COMPLETED:

			if(ADXL_FSMStopping_ProcessEvent(user_event) != FSM_OK )
			{
				ADXL_SetError(ADXL_FSMStopping_GetError(), context_data->current_state, current_event);
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
		case ADXL_EVT_EXTI_IRQ:
			Fsm_StateTransition(ctx, StreamUnexpectedIRQ_StateHandler);
			break;
		default:
			ADXL_SetError(ADXL_ERR_UNEXPECTED_BEHAVIOUR, context_data->current_state, current_event);
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
			context_data->current_state = STREAM_HALTED;
			break;
		case ADXL_EVT_START_STREAM: // fallthrough
		case ADXL_EVT_I2C_TX_COMPLETED: // fallthrough
		case ADXL_EVT_I2C_RX_COMPLETED: // fallthrough
		case ADXL_EVT_TIMEOUT:
			if(ADXL_FSMHalted_ProcessEvent(user_event) != FSM_OK )
			{
				ADXL_SetError(ADXL_FSMHalted_GetError(), context_data->current_state, current_event);
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
		case ADXL_EVT_EXTI_IRQ:
			Fsm_StateTransition(ctx, StreamUnexpectedIRQ_StateHandler);
			break;
		default:
			ADXL_SetError(ADXL_ERR_UNEXPECTED_BEHAVIOUR, context_data->current_state, current_event);
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
			context_data->current_state = STREAM_FLUSHING;
			user_event->user_event = ADXL_EVT_FIFO_FLUSH_REQ; // fallthough
		case ADXL_EVT_FIFO_FLUSH_REQ: // fallthrough
		case ADXL_EVT_I2C_RX_COMPLETED:
			if(ADXL_FSMFlushing_ProcessEvent(user_event) != FSM_OK )
			{
				ADXL_SetError(ADXL_FSMFlushing_GetError(), context_data->current_state, current_event);
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
		case ADXL_EVT_EXTI_IRQ:
			Fsm_StateTransition(ctx, StreamUnexpectedIRQ_StateHandler);
			break;
		default:
			ADXL_SetError(ADXL_ERR_UNEXPECTED_BEHAVIOUR, context_data->current_state, current_event);
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
			context_data->current_state = STREAM_WAITING;
			break;
		case ADXL_EVT_EXTI_IRQ: // fallthrough
		case ADXL_EVT_I2C_RX_COMPLETED:
			if(ADXL_FSMWaiting_ProcessEvent(user_event) != FSM_OK )
			{
				ADXL_SetError(ADXL_FSMWaiting_GetError(), context_data->current_state, current_event);
			}
			else
			{
				ret_val = FSM_OK;
			}
			break;
		case ADXL_EVT_FIFO_READY:

			if( ADXL_ReadMultipleRegsNonBlocking(DATAX0_REG, ADXL_raw_data, ONE_SAMPLE_SIZE) == ADXL_ERR_NO_ERROR)
			{
				ret_val = FSM_OK;
				Fsm_StateTransition(ctx, StreamInProgress_StateHandler);
			}
			else
			{
				ADXL_SetError(ADXL_ERR_COMMUNICATION_LOST, context_data->current_state, current_event);
			}
			break;
		case ADXL_EVT_FIFO_OVERRUN:
			ADXL_SetError(ADXL_ERR_OVERRUN, context_data->current_state, current_event);
			break;
		case ADXL_EVT_STOP_REQUEST:
			Fsm_StateTransition(ctx,StreamStopping_StateHandler);
			ret_val = FSM_OK;
			break;
		default:
			ADXL_SetError(ADXL_ERR_UNEXPECTED_BEHAVIOUR, context_data->current_state, current_event);
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
			context_data->current_state = STREAM_IN_PROGRESS;
			break;
		case ADXL_EVT_I2C_RX_COMPLETED:

			if(context_data->readout_num == (context_data->fifo_samples_num-1))
			{
				context_data->readout_num = 0;
				Fsm_StateTransition(ctx, StreamCompleted_StateHandler);

			}
			else
			{
				context_data->readout_num++;

				if( ADXL_ReadMultipleRegsNonBlocking(DATAX0_REG, &(ADXL_raw_data[context_data->readout_num * ONE_SAMPLE_SIZE]), ONE_SAMPLE_SIZE) != ADXL_ERR_NO_ERROR)
				{
					ret_val = FSM_ERROR;
					ADXL_SetError(ADXL_ERR_DMA_PROBLEM, context_data->current_state, current_event);
				}
			}
			break;
		case ADXL_EVT_STOP_REQUEST:
			Fsm_StateTransition(ctx, StreamStopping_StateHandler);
			break;
		case ADXL_EVT_EXTI_IRQ:
			Fsm_StateTransition(ctx, StreamUnexpectedIRQ_StateHandler);
			break;
		default:
			ret_val = FSM_ERROR;
			ADXL_SetError(ADXL_ERR_UNEXPECTED_BEHAVIOUR, context_data->current_state, current_event);
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
			context_data->current_state = STREAM_COMPLETED;
			break;
		case ADXL_EVT_STREAM_FINISHED:
			break;
		case ADXL_EVT_BUFFER_RELEASE_REQ:
			Fsm_StateTransition(ctx, StreamWaiting_StateHandler);
			break;
		case ADXL_EVT_STOP_REQUEST:
			Fsm_StateTransition(ctx, StreamStopping_StateHandler);
			break;
		case ADXL_EVT_EXTI_IRQ:
			Fsm_StateTransition(ctx, StreamUnexpectedIRQ_StateHandler);
			break;
		default:
			ret_val = FSM_ERROR;
			ADXL_SetError(ADXL_ERR_UNEXPECTED_BEHAVIOUR, context_data->current_state, current_event);
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
			context_data->current_state = STREAM_ERROR;
			context_data->error_callback(context_data->stream_errors.error_code);
			break;
		case ADXL_EVT_RESET_ERROR_REQUEST:
			ADXL_StreamReset();
			break;
		default:
			break;
	}
	return ret_val;
}

static FSM_ret StreamUnexpectedIRQ_StateHandler (fsm_context *ctx, FsmEvent_t *user_event)
{
	FSM_ret ret_val = FSM_OK;
	ADXL_FSM_Events current_event = (ADXL_FSM_Events)user_event->user_event;
	StreamCtxData_t *context_data = (StreamCtxData_t*)ctx->user_data;
	switch(current_event)
	{
		case FSM_INITIAL_EVENT:
			context_data->current_state = STREAM_UNEXPECTED_IRQ;
			user_event->user_event = ADXL_EVT_UNEXEPECTED_IRQ;
		case ADXL_EVT_I2C_TX_COMPLETED:
		case ADXL_EVT_I2C_RX_COMPLETED:
			if(ADXL_FSMUnexpectedIRQ_ProcessEvent(user_event)!= FSM_OK)
			{
				ADXL_SetError(ADXL_FSMUnexpectedIRQ_GetError(), context_data->current_state, current_event);
				ret_val = FSM_ERROR;
			}
			break;
		case ADXL_EVT_BUFFER_RELEASE_REQ:
			break; // ignore this
		case ADXL_EVT_FIFO_OVERRUN:
			ADXL_SetError(ADXL_ERR_OVERRUN, context_data->current_state, current_event);
			ret_val = FSM_ERROR;
			break;
		case ADXL_EVT_UNEXPCETED_WATERMARK:
			ADXL_SetError(ADXL_ERR_UNEXPECTED_WATERMARK, context_data->current_state, current_event);
			ret_val = FSM_ERROR;
			break;
		case ADXL_EVT_UNKNOWN_IRQ:
		default:
			ret_val = FSM_ERROR;
			ADXL_SetError(ADXL_ERR_UNEXPECTED_BEHAVIOUR, context_data->current_state, current_event);
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

