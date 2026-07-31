/*
 * ADXL_SubFSM_UnexpectedIRQ.c
 *
 *  Created on: Jul 5, 2026
 *      Author: Karol
 */

#include "ADXL_SubFSM_UnexpectedIRQ.h"
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
}StreamUnexpectedIRQCtxData_t;

static StreamUnexpectedIRQCtxData_t StreamUnexpectedIRQFsmData;
static fsm_context StreamUnexpectedIRQFsmContext;

static FSM_ret StreamUnexpectedIRQ_EntryStateHandler (fsm_context *ctx, FsmEvent_t *user_event);
static FSM_ret StreamUnexpectedIRQ_CheckingIntStatus (fsm_context *ctx, FsmEvent_t *user_event);
static FSM_ret StreamUnexpectedIRQ_WaitingStateHandler (fsm_context *ctx, FsmEvent_t *user_event);

void Stream_UnexpectedIRQReset()
{
	StreamUnexpectedIRQFsmData.last_error = ADXL_ERR_NO_ERROR;
	Fsm_StateTransition(&StreamUnexpectedIRQFsmContext, StreamUnexpectedIRQ_EntryStateHandler);
}

void FSM_UnexpectedIRQErrorCallback(void)
{
	Fsm_StateTransition(&StreamUnexpectedIRQFsmContext, StreamUnexpectedIRQ_EntryStateHandler);
}

void Stream_UnexpectedIRQSubFsmInit(fsm_set_event_callback event_cb)
{
	StreamUnexpectedIRQFsmData.last_error = ADXL_ERR_NO_ERROR;
	StreamUnexpectedIRQFsmData.evt_callback = event_cb;
	Fsm_Init(&StreamUnexpectedIRQFsmContext, StreamUnexpectedIRQ_EntryStateHandler , &StreamUnexpectedIRQFsmData, FSM_UnexpectedIRQErrorCallback);
}

FSM_ret ADXL_FSMUnexpectedIRQ_ProcessEvent(FsmEvent_t *user_event)
{
	return Fsm_ProcessEvent(&StreamUnexpectedIRQFsmContext, user_event);
}

ADXL_Errors_t ADXL_FSMUnexpectedIRQ_GetError()
{
	return StreamUnexpectedIRQFsmData.last_error;
}

static uint8_t data_out;

static FSM_ret StreamUnexpectedIRQ_EntryStateHandler(fsm_context *ctx, FsmEvent_t *user_event)
{
	FSM_ret ret_val = FSM_OK;
	ADXL_FSM_Events current_event = (ADXL_FSM_Events)user_event->user_event;
	StreamUnexpectedIRQCtxData_t *context_data = (StreamUnexpectedIRQCtxData_t*)ctx->user_data;

	switch (current_event)
	{
		case FSM_INITIAL_EVENT:
			break;
		case ADXL_EVT_UNEXEPECTED_IRQ:
			context_data->last_error = ADXL_ERR_NO_ERROR;
			if(ADXLConn_GetCurrentOperation()== ADXL_OP_NO_OPERATION)
			{
				Fsm_StateTransition(ctx, StreamUnexpectedIRQ_CheckingIntStatus);
			}
			else
			{
				Fsm_StateTransition(ctx, StreamUnexpectedIRQ_WaitingStateHandler);
			}
			break;
		default:
			ret_val = FSM_ERROR;
			context_data->last_error = ADXL_ERR_UNEXPECTED_BEHAVIOUR;
			break;
	}

	return ret_val;
}

static FSM_ret StreamUnexpectedIRQ_WaitingStateHandler(fsm_context *ctx, FsmEvent_t *user_event)
{
	FSM_ret ret_val = FSM_OK;
	ADXL_FSM_Events current_event = (ADXL_FSM_Events)user_event->user_event;
	StreamUnexpectedIRQCtxData_t *context_data = (StreamUnexpectedIRQCtxData_t*)ctx->user_data;

	switch (current_event)
	{
		case FSM_INITIAL_EVENT:
			break;
		case ADXL_EVT_I2C_TX_COMPLETED: // fallthrough
		case ADXL_EVT_I2C_RX_COMPLETED:
				Fsm_StateTransition(ctx, StreamUnexpectedIRQ_CheckingIntStatus);
			break;
		default:
			ret_val = FSM_ERROR;
			context_data->last_error = ADXL_ERR_UNEXPECTED_BEHAVIOUR;
			break;
	}
	return ret_val;
}

static FSM_ret StreamUnexpectedIRQ_CheckingIntStatus(fsm_context *ctx, FsmEvent_t *user_event)
{
	FSM_ret ret_val = FSM_OK;
	ADXL_FSM_Events current_event = (ADXL_FSM_Events)user_event->user_event;
	StreamUnexpectedIRQCtxData_t *context_data = (StreamUnexpectedIRQCtxData_t*)ctx->user_data;

	switch (current_event)
	{
		case FSM_INITIAL_EVENT:
			data_out = 0;
			if(ADXL_ReadRegNonBlocking(INT_SOURCE, &data_out) != ADXL_ERR_NO_ERROR)
			{
				ret_val = FSM_ERROR;
				context_data->last_error = ADXL_ERR_COMMUNICATION_LOST;
				Fsm_StateTransition(ctx, StreamUnexpectedIRQ_EntryStateHandler);
			}
			break;
		case ADXL_EVT_I2C_RX_COMPLETED:
			if(data_out & ADXL_INT_SOURCE_OVERRUN)
			{
				if( context_data->evt_callback(ADXL_EVT_FIFO_OVERRUN) != ADXL_SUCCESS)
				{
					ret_val = FSM_ERROR;
					context_data->last_error = ADXL_ERR_QUEUE_FAILURE;
				}
			}
			else if(data_out & ADXL_INT_SOURCE_WATERMARK)
			{
				if(context_data->evt_callback(ADXL_EVT_UNEXPCETED_WATERMARK) != ADXL_SUCCESS)
				{
					ret_val = FSM_ERROR;
					context_data->last_error = ADXL_ERR_QUEUE_FAILURE;
				}
			}
			else
			{
				if(context_data->evt_callback(ADXL_EVT_UNKNOWN_IRQ) != ADXL_SUCCESS)
				{
					ret_val = FSM_ERROR;
					context_data->last_error = ADXL_ERR_QUEUE_FAILURE;
				}
			}
			Fsm_StateTransition(ctx, StreamUnexpectedIRQ_EntryStateHandler);
			break;
		default:
			ret_val = FSM_ERROR;
			context_data->last_error = ADXL_ERR_UNEXPECTED_BEHAVIOUR;
			break;
	}
	return ret_val;
}

