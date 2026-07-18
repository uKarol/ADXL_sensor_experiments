/*
 * MEASUREMENT_SubFSM_Processing.c
 *
 *  Created on: Jul 14, 2026
 *      Author: Karol
 */


#include "MeasurementFSM.h"
#include "MEASUREMENT_SubFSM_Processing.h"
#include "ADXL_driver.h"
#include "UART_Communication.h"
#include "fsm.h"



typedef struct
{
	measurement_error_t last_error;
	uint16_t expected_size;
	measurement_set_event_callback evt_callback;
	uint8_t number_of_fifo_samples;
	uint16_t readout_ctr;
	uint8_t *captured_data;
}MeasurementProcessingCtxData_t;

static MeasurementProcessingCtxData_t MeasurementProcessingFsmData;
static fsm_context MeasurementProcessingFsmContext;

static FSM_ret MeasurementProcessingIdle_StateHandler (fsm_context *ctx, FsmEvent_t *user_event);
static FSM_ret MeasurementProcessingHeaderSending_StateHandler (fsm_context *ctx, FsmEvent_t *user_event);
static FSM_ret MeasurementProcessingDataSending_StateHandler (fsm_context *ctx, FsmEvent_t *user_event);


void MeasurementProcessingSubFsm_Reset()
{
	MeasurementProcessingFsmData.last_error = MEAS_NO_ERROR;
	MeasurementProcessingFsmData.readout_ctr = 0;
	MeasurementProcessingFsmData.expected_size = 0;
	Fsm_StateTransition(&MeasurementProcessingFsmContext, MeasurementProcessingIdle_StateHandler);
}

static void MeasurementProcessingSubFsmErrorCallback()
{
	Fsm_StateTransition(&MeasurementProcessingFsmContext, MeasurementProcessingIdle_StateHandler);
}

measurement_error_t MeasurementProcessingSubFsm_GetError()
{
	return MeasurementProcessingFsmData.last_error;
}

FSM_ret MeasurementProcessingSubFsm_ProcessEvent(FsmEvent_t *user_event)
{
	return Fsm_ProcessEvent(&MeasurementProcessingFsmContext, user_event);
}

void MeasurementProcessingSubFsm_Init(measurement_set_event_callback event_cb, uint8_t number_of_fifo_samples)
{
	MeasurementProcessingFsmData.number_of_fifo_samples = number_of_fifo_samples;
	MeasurementProcessingFsmData.evt_callback = event_cb;
	MeasurementProcessingFsmData.readout_ctr = 0;
	MeasurementProcessingFsmData.last_error = MEAS_NO_ERROR;
	Fsm_Init(&MeasurementProcessingFsmContext, MeasurementProcessingIdle_StateHandler , &MeasurementProcessingFsmData, MeasurementProcessingSubFsmErrorCallback);
}

static FSM_ret MeasurementProcessingIdle_StateHandler (fsm_context *ctx, FsmEvent_t *user_event)
{
	FSM_ret ret_val = FSM_OK;
	MeasurementEvt_t current_event = (MeasurementEvt_t)user_event->user_event;
	MeasurementProcessingCtxData_t *context_data = (MeasurementProcessingCtxData_t*)ctx->user_data;
	uint16_t *event_data = (uint16_t*)(user_event->user_data);

	switch(current_event)
	{ 
		case FSM_INITIAL_EVENT:
		break;

		case MEASUREMENT_EVT_CONFIG_SET:
			context_data->expected_size = *event_data;
		break;

		case MEASUREMENT_EVT_DATA_READY:
			Fsm_StateTransition(ctx, MeasurementProcessingHeaderSending_StateHandler);
		break;

		default:
			ret_val = FSM_ERROR;
			context_data->last_error = MEAS_UNEXPECTED_EVENT;
			break;

	}
	return ret_val;
}

static FSM_ret MeasurementProcessingHeaderSending_StateHandler (fsm_context *ctx, FsmEvent_t *user_event)
{
	FSM_ret ret_val = FSM_OK;
	MeasurementEvt_t current_event = (MeasurementEvt_t)user_event->user_event;
	MeasurementProcessingCtxData_t *context_data = (MeasurementProcessingCtxData_t*)ctx->user_data;

	switch(current_event)
	{
		case FSM_INITIAL_EVENT:
			if(UART_Com_TransmitStringNonBlocking("OK\n") != TRANSMIT_OK )
			{
				ret_val = FSM_ERROR;
				context_data->last_error = MEAS_TX_FAILURE;
				break;
			}
			break;

		case MEASUREMENT_EVT_TX_COMPLETED:
			Fsm_StateTransition(ctx, MeasurementProcessingDataSending_StateHandler);
			break;
		default:
			ret_val = FSM_ERROR;
			context_data->last_error = MEAS_UNEXPECTED_EVENT;
			break;
	}

	return ret_val;
}

static FSM_ret MeasurementProcessingDataSending_StateHandler (fsm_context *ctx, FsmEvent_t *user_event)
{
	FSM_ret ret_val = FSM_OK;
	MeasurementEvt_t current_event = (MeasurementEvt_t)user_event->user_event;
	MeasurementProcessingCtxData_t *context_data = (MeasurementProcessingCtxData_t*)ctx->user_data;

	switch(current_event)
	{ 
		case FSM_INITIAL_EVENT:
			context_data->captured_data = ADXL_GetStreamedData();
			if(context_data->captured_data != NULL)
			{
				if( UART_Com_TransmitRawDataNonBLocking(context_data->captured_data, (context_data->number_of_fifo_samples*ONE_SAMPLE_SIZE)) != TRANSMIT_OK )
				{
					ret_val = FSM_ERROR;
					context_data->last_error = MEAS_TX_FAILURE;
					break;
				}
			}
			else
			{
				ret_val = FSM_ERROR;
				context_data->last_error = MEAS_READ_FAILURE;
			}
			break;

		case MEASUREMENT_EVT_TX_COMPLETED:
			ADXL_ReleaseDataBuffer();
			context_data->readout_ctr++;
			if(context_data->readout_ctr == context_data->expected_size)
			{
				context_data->readout_ctr = 0;
				context_data->evt_callback(MEASUREMENT_EVT_READOUT_COMPLETED);
			}
			Fsm_StateTransition(ctx, MeasurementProcessingIdle_StateHandler);
			break;
		default:
			ret_val = FSM_ERROR;
			context_data->last_error = MEAS_UNEXPECTED_EVENT;
			break;
	}
	return ret_val;
}
