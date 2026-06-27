/*
 * ADXL_FSM.c
 *
 *  Created on: Jun 26, 2026
 *      Author: Karol
 */

#include "fsm.h"
#include "ADXL_driver.h"
#include "ADXL_FSM.h"
#include "i2c.h"
#include "ADXL_defs.h"

// main stream fsm
static FSM_ret StreamCompleted_StateHandler (fsm_context *ctx, FsmEvent_t *user_event);
static FSM_ret StreamInProgress_StateHandler (fsm_context *ctx, FsmEvent_t *user_event);
static FSM_ret StreamError_StateHandler (fsm_context *ctx, FsmEvent_t *user_event);
static FSM_ret StreamWaiting_StateHandler (fsm_context *ctx, FsmEvent_t *user_event);

// sub states for STREAM_WAITING state
static FSM_ret StreamWaiting_IdleStateHandler (fsm_context *ctx, FsmEvent_t *user_event);
static FSM_ret StreamWaiting_CheckIntStatusStateHandler (fsm_context *ctx, FsmEvent_t *user_event);
static FSM_ret StreamWaiting_CheckFifoStateHandler (fsm_context *ctx, FsmEvent_t *user_event);

static void Stream_WaitingSubFsmInit();

uint8_t ADXL_raw_data[MAX_READOUT_SIZE];

typedef struct
{
	ADXL_Errors_t last_error;
	uint8_t expected_size;
	uint8_t dma_out_data;
}StreamWaitingCtxData_t;

typedef struct
{
	uint8_t readout_num;
	uint8_t fifo_samples_num;
	ADXL_Errors_t stream_errors;
	ADXL_StreamStatus current_state;
	fsm_error_callback error_callback;
}StreamCtxData_t;

StreamCtxData_t StreamFsmData;
StreamWaitingCtxData_t StreamWaitingFsmData;
fsm_context StreamWaitingFsmContext;
fsm_context StreamFsmContext;

void ADXL_FSM_Init(uint8_t fifo_samples, fsm_error_callback error_cb)
{
	StreamFsmData.error_callback = error_cb;
	StreamFsmData.fifo_samples_num = fifo_samples;
	StreamWaitingFsmData.expected_size = fifo_samples;
	Stream_WaitingSubFsmInit();
	Fsm_Init(&StreamFsmContext, StreamWaiting_StateHandler , &StreamFsmData);
}

static FSM_ret StreamWaiting_IdleStateHandler(fsm_context *ctx, FsmEvent_t *user_event)
{
	FSM_ret ret_val = FSM_OK;
	ADXL_FSM_Events current_event = (ADXL_FSM_Events)user_event->user_event;
	StreamWaitingCtxData_t *context_data = (StreamWaitingCtxData_t*)ctx->user_data;

	switch (current_event)
	{
	case ADXL_EXTI_IRQ:
		/* code */
		if( HAL_I2C_Mem_Read_DMA(&hi2c1, ADEXL_ID, INT_SOURCE, 1, &(context_data->dma_out_data), 1) == HAL_OK)
		{
			ctx->current_state = StreamWaiting_CheckIntStatusStateHandler;
		}
		else
		{
			ret_val = FSM_ERROR;
			context_data->last_error = ADXL_DMA_PROBLEM;
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
		case DMA_COMPLETED:
			/* code */
			if(context_data->dma_out_data & ADXL_INT_ENABLE_OVERRUN)
			{
				ADXL_SetEvent(ADXL_OVERRUN);
				ctx->current_state = StreamWaiting_IdleStateHandler;
				context_data->last_error = ADXL_OVERRUN;
			}
			else if(context_data->dma_out_data & ADXL_INT_ENABLE_WATERMARK)
			{

				if( HAL_I2C_Mem_Read_DMA(&hi2c1, ADEXL_ID, FIFO_STATUS, 1, &(context_data->dma_out_data), 1) == HAL_OK)
				{
					ctx->current_state = StreamWaiting_CheckFifoStateHandler;
				}
				else
				{
					ret_val = FSM_ERROR;
					context_data->last_error = ADXL_DMA_PROBLEM;
					ctx->current_state = StreamWaiting_IdleStateHandler;
				}
			}
			break;
		default:
				ret_val = FSM_ERROR;
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
		case DMA_COMPLETED:
			/* code */
			if((context_data->dma_out_data & FIFO_ENTRIES_BIT_MSK) >= context_data->expected_size)
			{
				ADXL_SetEvent(ADXL_FIFO_READY);
				ctx->current_state = StreamWaiting_IdleStateHandler;
			}
			break;
		default:
			ret_val = FSM_ERROR;
			ctx->current_state = StreamWaiting_IdleStateHandler;
			break;
	}

	return ret_val;
}


static void Stream_WaitingSubFsmInit()
{
	Fsm_Init(&StreamWaitingFsmContext, StreamWaiting_IdleStateHandler , &StreamWaitingFsmData);
}

FSM_ret StreamWaiting_StateHandler (fsm_context *ctx, FsmEvent_t *user_event)
{
	FSM_ret ret_val = FSM_ERROR;

	ADXL_FSM_Events current_event = (ADXL_FSM_Events)user_event->user_event;
	StreamCtxData_t *context_data = (StreamCtxData_t*)ctx->user_data;

	switch(current_event)
	{
		case ADXL_EXTI_IRQ: // fallthrough
		case DMA_COMPLETED:
			if(Fsm_ProcessEvent(&StreamWaitingFsmContext, user_event) != FSM_OK )
			{
				StreamWaitingCtxData_t *subcontext_data = (StreamWaitingCtxData_t*)(StreamWaitingFsmContext.user_data);
				ctx->current_state = StreamError_StateHandler;
				context_data->stream_errors = subcontext_data->last_error;
				context_data->current_state = STREAM_ERROR;
				ADXL_SetEvent(ERROR_OCCURED);
			}
			else
			{
				ret_val = FSM_OK;
			}
			break;
		case ADXL_FIFO_READY:

			if( HAL_I2C_Mem_Read_DMA(&hi2c1, ADEXL_ID, DATAX0_REG, 1, ADXL_raw_data, ONE_SAMPLE_SIZE) == HAL_OK)
			{
				ctx->current_state = StreamInProgress_StateHandler;
				context_data->current_state = STREAM_IN_PROGRESS;
				ret_val = FSM_OK;
			}
			else
			{
				ctx->current_state = StreamError_StateHandler;
				context_data->stream_errors = ADXL_COMMUNICATION_LOST;
				context_data->current_state = STREAM_ERROR;
				ADXL_SetEvent(ERROR_OCCURED);
			}
			break;
		default:
			ctx->current_state = StreamError_StateHandler;
			context_data->stream_errors = ADXL_UNEXPECTED_BEHAVIOUR;
			context_data->current_state = STREAM_ERROR;
			ADXL_SetEvent(ERROR_OCCURED);
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
		case DMA_COMPLETED:

			if(context_data->readout_num == (context_data->fifo_samples_num-1))
			{
				context_data->readout_num = 0;
				context_data->current_state = STREAM_COMPLETED;
				ctx->current_state = StreamCompleted_StateHandler;

			}
			else
			{
				context_data->readout_num++;
				if( HAL_I2C_Mem_Read_DMA(&hi2c1, ADEXL_ID, DATAX0_REG, 1, &(ADXL_raw_data[context_data->readout_num * ONE_SAMPLE_SIZE]), ONE_SAMPLE_SIZE) != HAL_OK)
				{
					ret_val = FSM_ERROR;
					context_data->current_state = STREAM_ERROR;
					ctx->current_state = StreamError_StateHandler;
					context_data->stream_errors = ADXL_DMA_PROBLEM;
					ADXL_SetEvent(ERROR_OCCURED);
				}
			}
			break;

		default:
			ret_val = FSM_ERROR;
			context_data->current_state = STREAM_ERROR;
			ctx->current_state = StreamError_StateHandler;
			context_data->stream_errors = ADXL_UNEXPECTED_BEHAVIOUR;
			ADXL_SetEvent(ERROR_OCCURED);
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
		case STREAM_FINISHED:
			break;
		case BUFFER_RELEASE_REQUEST:
			ctx->current_state = StreamWaiting_StateHandler;
			context_data->current_state = STREAM_IDLE;
			break;

		default:
			ret_val = FSM_ERROR;
			ctx->current_state = StreamError_StateHandler;
			context_data->current_state = STREAM_ERROR;
			context_data->stream_errors = ADXL_UNEXPECTED_BEHAVIOUR;
			ADXL_SetEvent(ERROR_OCCURED);
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
		case ERROR_OCCURED:
			ret_val = FSM_ERROR;
			context_data->error_callback(context_data->stream_errors);
			break;
		case RESET_ERROR_REQUEST:
			ret_val = FSM_OK;
			context_data->current_state = STREAM_IDLE;
			context_data->readout_num = 0;
			context_data->stream_errors = ADXL_NO_ERROR;
			ctx->current_state = StreamWaiting_StateHandler;
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

