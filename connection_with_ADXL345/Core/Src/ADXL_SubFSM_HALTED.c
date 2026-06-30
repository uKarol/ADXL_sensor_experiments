/*
 * ADXL_SubFSM_HALTED.c
 *
 *  Created on: Jun 28, 2026
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
}StreamHaltedCtxData_t;

static StreamHaltedCtxData_t StreamHaltedFsmData;
static fsm_context StreamHaltedFsmContext;

static FSM_ret StreamHalted_IdleStateHandler (fsm_context *ctx, FsmEvent_t *user_event);
static FSM_ret StreamHalted_SettingPowerCTL (fsm_context *ctx, FsmEvent_t *user_event);
static FSM_ret StreamHalted_Waiting (fsm_context *ctx, FsmEvent_t *user_event);


void Stream_HaltedSubFsmInit(fsm_set_event_callback event_cb)
{
	StreamHaltedFsmData.evt_callback = event_cb;
	Fsm_Init(&StreamHaltedFsmContext, StreamHalted_IdleStateHandler , &StreamHaltedFsmData);
}

FSM_ret ADXL_FSMHalted_ProcessEvent(FsmEvent_t *user_event)
{
	return Fsm_ProcessEvent(&StreamHaltedFsmContext, user_event);
}

ADXL_Errors_t ADXL_FSMHalted_GetError()
{
	return StreamHaltedFsmData.last_error;
}

static FSM_ret StreamHalted_IdleStateHandler (fsm_context *ctx, FsmEvent_t *user_event)
{
	FSM_ret ret_val = FSM_OK;
	ADXL_FSM_Events current_event = (ADXL_FSM_Events)user_event->user_event;
	StreamHaltedCtxData_t *context_data = (StreamHaltedCtxData_t*)ctx->user_data;

	switch (current_event)
	{
		case ADXL_EVT_START_STREAM:
			if(ADXL_WriteRegNonBlocking(POWER_CTL, POWER_CTL_MEASURE) == ADXL_ERR_NO_ERROR)
			{
				ctx->current_state = StreamHalted_SettingPowerCTL;
			}
			else
			{
				ret_val = FSM_ERROR;
				context_data->last_error = ADXL_ERR_COMMUNICATION_LOST;
			}
			break;
		default:
			break;
	}

	return ret_val;
}
uint8_t helper_data;
static FSM_ret StreamHalted_SettingPowerCTL (fsm_context *ctx, FsmEvent_t *user_event)
{
	FSM_ret ret_val = FSM_OK;
	ADXL_FSM_Events current_event = (ADXL_FSM_Events)user_event->user_event;
	StreamHaltedCtxData_t *context_data = (StreamHaltedCtxData_t*)ctx->user_data;

	switch (current_event)
	{
		case ADXL_EVT_I2C_TX_COMPLETED:


			ADXL_ReadRegNonBlocking(POWER_CTL, &helper_data);

			break;
		case ADXL_EVT_I2C_RX_COMPLETED:
			EvtTimerStart(100);
			ctx->current_state = StreamHalted_Waiting;
			break;
		default:
			ret_val = FSM_ERROR;
			context_data->last_error = ADXL_ERR_UNEXPECTED_BEHAVIOUR;
			ctx->current_state = StreamHalted_IdleStateHandler;
			break;
	}

	return ret_val;
}

static FSM_ret StreamHalted_Waiting (fsm_context *ctx, FsmEvent_t *user_event)
{
	FSM_ret ret_val = FSM_OK;
	ADXL_FSM_Events current_event = (ADXL_FSM_Events)user_event->user_event;
	StreamHaltedCtxData_t *context_data = (StreamHaltedCtxData_t*)ctx->user_data;

	switch (current_event)
	{
		case ADXL_EVT_TIMEOUT:
			ctx->current_state = StreamHalted_IdleStateHandler;
			context_data->evt_callback(ADXL_EVT_SENSOR_ENABLED);
			break;
		default:
			ret_val = FSM_ERROR;
			context_data->last_error = ADXL_ERR_UNEXPECTED_BEHAVIOUR;
			ctx->current_state = StreamHalted_IdleStateHandler;
		break;
	}

	return ret_val;
}
