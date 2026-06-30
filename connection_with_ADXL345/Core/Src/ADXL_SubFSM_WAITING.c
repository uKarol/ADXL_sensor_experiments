/*
 * ADXL_SubFSM_WAITING.c
 *
 *  Created on: Jun 28, 2026
 *      Author: Karol
 */
#include "ADXL_SubFSM_WAITING.h"
#include "ADXL_driver.h"
#include "ADXL_FSM.h"
#include "ADXL_defs.h"
#include "ADXL_i2c_conn.h"

typedef struct
{
	ADXL_Errors_t last_error;
	uint8_t expected_size;
	uint8_t dma_out_data;
	fsm_set_event_callback evt_callback;
}StreamWaitingCtxData_t;

static StreamWaitingCtxData_t StreamWaitingFsmData;
static fsm_context StreamWaitingFsmContext;

// sub states for STREAM_WAITING state
static FSM_ret StreamWaiting_IdleStateHandler (fsm_context *ctx, FsmEvent_t *user_event);
static FSM_ret StreamWaiting_CheckIntStatusStateHandler (fsm_context *ctx, FsmEvent_t *user_event);
static FSM_ret StreamWaiting_CheckFifoStateHandler (fsm_context *ctx, FsmEvent_t *user_event);

ADXL_Errors_t ADXL_FSMWaiting_GetError()
{
	return StreamWaitingFsmData.last_error;
}

FSM_ret ADXL_FSMWaiting_ProcessEvent(FsmEvent_t *user_event)
{
	return Fsm_ProcessEvent(&StreamWaitingFsmContext, user_event);
}

void Stream_WaitingSubFsmInit(fsm_set_event_callback event_cb, uint8_t fifo_samples)
{
	StreamWaitingFsmData.expected_size = fifo_samples;
	StreamWaitingFsmData.evt_callback = event_cb;
	Fsm_Init(&StreamWaitingFsmContext, StreamWaiting_IdleStateHandler , &StreamWaitingFsmData);
}

static FSM_ret StreamWaiting_IdleStateHandler(fsm_context *ctx, FsmEvent_t *user_event)
{
	FSM_ret ret_val = FSM_OK;
	ADXL_FSM_Events current_event = (ADXL_FSM_Events)user_event->user_event;
	StreamWaitingCtxData_t *context_data = (StreamWaitingCtxData_t*)ctx->user_data;

	switch (current_event)
	{
	case ADXL_EVT_EXTI_IRQ:
		/* code */
		if(ADXL_ReadRegNonBlocking(INT_SOURCE, &(context_data->dma_out_data)) == ADXL_ERR_NO_ERROR )
		{
			ctx->current_state = StreamWaiting_CheckIntStatusStateHandler;
		}
		else
		{
			ret_val = FSM_ERROR;
			context_data->last_error = ADXL_ERR_DMA_PROBLEM;
		}
		break;
	default:
		break;
	}

	return ret_val;
}

static FSM_ret StreamWaiting_CheckIntStatusStateHandler (fsm_context *ctx, FsmEvent_t *user_event)
{
	FSM_ret ret_val = FSM_OK;
	ADXL_FSM_Events current_event = (ADXL_FSM_Events)user_event->user_event;
	StreamWaitingCtxData_t *context_data = (StreamWaitingCtxData_t*)ctx->user_data;


	switch (current_event)
	{
		case ADXL_EVT_I2C_RX_COMPLETED:
			/* code */
			if(context_data->dma_out_data & ADXL_INT_ENABLE_OVERRUN)
			{
				context_data->evt_callback(ADXL_EVT_FIFO_OVERRUN);
				ctx->current_state = StreamWaiting_IdleStateHandler;
				context_data->last_error = ADXL_ERR_OVERRUN;
			}
			else if(context_data->dma_out_data & ADXL_INT_ENABLE_WATERMARK)
			{
				if( ADXL_ReadRegNonBlocking(FIFO_STATUS, &(context_data->dma_out_data)) == ADXL_ERR_NO_ERROR )
				{
					ctx->current_state = StreamWaiting_CheckFifoStateHandler;
				}
				else
				{
					ret_val = FSM_ERROR;
					context_data->last_error = ADXL_ERR_DMA_PROBLEM;
					ctx->current_state = StreamWaiting_IdleStateHandler;
				}
			}
			break;
		default:
				ret_val = FSM_ERROR;
				context_data->last_error = ADXL_ERR_UNEXPECTED_BEHAVIOUR;
				ctx->current_state = StreamWaiting_IdleStateHandler;
			break;
	}


	return ret_val;
}

static FSM_ret StreamWaiting_CheckFifoStateHandler (fsm_context *ctx, FsmEvent_t *user_event)
{
	FSM_ret ret_val = FSM_OK;
	ADXL_FSM_Events current_event = (ADXL_FSM_Events)user_event->user_event;
	StreamWaitingCtxData_t *context_data = (StreamWaitingCtxData_t*)ctx->user_data;

	switch (current_event)
	{
		case ADXL_EVT_I2C_RX_COMPLETED:
			/* code */
			if((context_data->dma_out_data & FIFO_ENTRIES_BIT_MSK) >= context_data->expected_size)
			{
				context_data->evt_callback(ADXL_EVT_FIFO_READY);
				ctx->current_state = StreamWaiting_IdleStateHandler;
			}
			break;
		default:
			ret_val = FSM_ERROR;
			context_data->last_error = ADXL_ERR_UNEXPECTED_BEHAVIOUR;
			ctx->current_state = StreamWaiting_IdleStateHandler;
			break;
	}

	return ret_val;
}

