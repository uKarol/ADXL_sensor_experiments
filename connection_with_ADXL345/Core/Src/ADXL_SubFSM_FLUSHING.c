/*
 * ADXL_SubFSM_FLUSHING.c
 *
 *  Created on: Jun 28, 2026
 *      Author: Karol
 */

#include "ADXL_SubFSM_WAITING.h"
#include "ADXL_driver.h"
#include "ADXL_FSM.h"
#include "ADXL_i2c_conn.h"
#include "ADXL_defs.h"
#include "fsm.h"

typedef struct
{
	ADXL_Errors_t last_error;
	uint8_t expected_size;
	uint8_t dma_out_data;
	fsm_set_event_callback evt_callback;
	uint8_t readout_num;
}StreamFlushingCtxData_t;

static StreamFlushingCtxData_t StreamFlushingFsmData;
static fsm_context StreamFlushingFsmContext;

static FSM_ret StreamFlushingIdle_StateHandler (fsm_context *ctx, FsmEvent_t *user_event);
static FSM_ret StreamFlushingCheckFifo_StateHandler (fsm_context *ctx, FsmEvent_t *user_event);
static FSM_ret StreamFlushingDataReadout_StateHandler (fsm_context *ctx, FsmEvent_t *user_event);


ADXL_Errors_t ADXL_FSMFlushing_GetError()
{
	return StreamFlushingFsmData.last_error;
}

FSM_ret ADXL_FSMFlushing_ProcessEvent(FsmEvent_t *user_event)
{
	return Fsm_ProcessEvent(&StreamFlushingFsmContext, user_event);
}

void Stream_FlushingSubFsmInit(fsm_set_event_callback event_cb)
{
	StreamFlushingFsmData.evt_callback = event_cb;
	Fsm_Init(&StreamFlushingFsmContext, StreamFlushingIdle_StateHandler , &StreamFlushingFsmData);
}

static FSM_ret StreamFlushingIdle_StateHandler (fsm_context *ctx, FsmEvent_t *user_event)
{
	FSM_ret ret_val = FSM_OK;
	ADXL_FSM_Events current_event = (ADXL_FSM_Events)user_event->user_event;
	StreamFlushingCtxData_t *context_data = (StreamFlushingCtxData_t*)ctx->user_data;

	switch(current_event)
	{
		case ADXL_EVT_FIFO_FLUSH_REQ:
			if(ADXL_ReadRegNonBlocking(FIFO_STATUS, &(context_data->dma_out_data)) == ADXL_ERR_NO_ERROR )
			{
				context_data->last_error = ADXL_ERR_NO_ERROR;
				context_data->expected_size = 0;
				context_data->readout_num = 0;
				ctx->current_state = StreamFlushingCheckFifo_StateHandler;
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

static uint8_t dma_readout[6];

static FSM_ret StreamFlushingCheckFifo_StateHandler (fsm_context *ctx, FsmEvent_t *user_event)
{
	FSM_ret ret_val = FSM_OK;
	ADXL_FSM_Events current_event = (ADXL_FSM_Events)user_event->user_event;
	StreamFlushingCtxData_t *context_data = (StreamFlushingCtxData_t*)ctx->user_data;

	switch(current_event)
	{
		case ADXL_EVT_I2C_RX_COMPLETED:
			context_data->expected_size = (context_data->dma_out_data & FIFO_ENTRIES_BIT_MSK);
			if( ADXL_ReadMultipleRegsNonBlocking(DATAX0_REG, dma_readout, ONE_SAMPLE_SIZE) == ADXL_ERR_NO_ERROR)
			{
				ctx->current_state = StreamFlushingDataReadout_StateHandler;
			}
			else
			{
				ret_val = FSM_ERROR;
				context_data->last_error = ADXL_ERR_DMA_PROBLEM;
				ctx->current_state = StreamFlushingIdle_StateHandler;
			}
			break;
		default:
			ret_val = FSM_ERROR;
			context_data->last_error = ADXL_ERR_UNEXPECTED_BEHAVIOUR;
			ctx->current_state = StreamFlushingIdle_StateHandler;
	}
	return ret_val;
}
static FSM_ret StreamFlushingDataReadout_StateHandler (fsm_context *ctx, FsmEvent_t *user_event)
{
	FSM_ret ret_val = FSM_OK;
	ADXL_FSM_Events current_event = (ADXL_FSM_Events)user_event->user_event;
	StreamFlushingCtxData_t *context_data = (StreamFlushingCtxData_t*)ctx->user_data;

	switch(current_event)
	{
		case ADXL_EVT_I2C_RX_COMPLETED:
			context_data->readout_num ++;

			if(context_data->readout_num == context_data->expected_size)
			{
				ctx->current_state = StreamFlushingIdle_StateHandler;
				context_data->evt_callback(ADXL_EVT_FIFO_CLEARED);
			}
			else
			{
				if( ADXL_ReadMultipleRegsNonBlocking(DATAX0_REG, dma_readout, ONE_SAMPLE_SIZE) != ADXL_ERR_NO_ERROR)
				{
					ret_val = FSM_ERROR;
					context_data->last_error = ADXL_ERR_DMA_PROBLEM;
					ctx->current_state = StreamFlushingIdle_StateHandler;
				}
			}
			break;

		default:
			ret_val = FSM_ERROR;
			context_data->last_error = ADXL_ERR_UNEXPECTED_BEHAVIOUR;
			ctx->current_state = StreamFlushingIdle_StateHandler;
	}

	return ret_val;
}

