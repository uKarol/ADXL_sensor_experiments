/*
 * MeasurementFSM.c
 *
 *  Created on: Jun 9, 2026
 *      Author: Karol
 */

#include "MeasurementFSM.h"
#include "ADXL_driver_interface.h"
#include "UART_Communication.h"
#include "MEASUREMENT_SubFSM_Processing.h"
#include "fsm.h"
#include <string.h>
#include <stdbool.h>
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "cmsis_gcc.h"

#define READOUT_NUM 100U

#define MAX_DIAG_READOUT_LENGTH 150U

typedef enum
{
    GET_SIZE_IDLE,
    GET_SIZE_WAIT_TX_COMPLETE,
    GET_SIZE_WAIT_RX_COMPLETE
} MeasurementGetSizePhase_t;

typedef struct
{
	measurement_error_t last_error;
	uint16_t measure_ctr;
	uint16_t expected_size;
	measurement_state_t current_state;
	uint8_t number_of_fifo_samples;
	MeasurementGetSizePhase_t get_size_phase;
	char diag_readout[MAX_DIAG_READOUT_LENGTH];
}MeasurementFSM_Data_t;


MeasurementFSM_Data_t MeasurementData;

#define EVT_BUFFER_CAPACITY (10U)


QueueHandle_t Measurement_EvtQueue;
TaskHandle_t measurement_task_handle;

static FSM_ret MeasurementWaiting_StateHandler (fsm_context *ctx, FsmEvent_t *user_event);
static FSM_ret MeasurementProcessing_StateHandler (fsm_context *ctx, FsmEvent_t *user_event);
static FSM_ret MeasurementGetSize_StateHandler (fsm_context *ctx, FsmEvent_t *user_event);
static FSM_ret MeasurementError_StateHandler (fsm_context *ctx, FsmEvent_t *user_event);
static FSM_ret MeasurementStarting_StateHandler (fsm_context *ctx, FsmEvent_t *user_event);
static FSM_ret MeasurementStopping_StateHandler (fsm_context *ctx, FsmEvent_t *user_event);
static FSM_ret MeasurementDiagReadout_StateHandler (fsm_context *ctx, FsmEvent_t *user_event);
void Measurement_SetEvent(MeasurementEvt_t evt);

void MeasurementADXL_Started();
void MeasurementADXL_Stopped();
void MeasurementADXL_DataReady();
void MeasurementADXL_ErrorDetected();

enum
{
	ADXL_INIT_FAILED,
	ADXL_INIT_SUCCESSFULL,
};

helper_external_callbacks adxl_callbacks =
{
		.adxl_started_callback = MeasurementADXL_Started,
		.adxl_stopped_callback = MeasurementADXL_Stopped,
		.adxl_completed_callback = MeasurementADXL_DataReady,
		.adxl_error_detected_callback = MeasurementADXL_ErrorDetected
};

static fsm_context MeasurementFsmContext;
static MeasurementFSM_Data_t MeasurementFsmData;



static void MeasurementErrorCallback()
{
	Fsm_StateTransition(&MeasurementFsmContext, MeasurementError_StateHandler);
}

void MeasurementADXL_ErrorDetected()
{
	Measurement_SetEvent(MEASUREMENT_EVT_ADXL_ERROR_DETECTED);
}

void MeasurementADXL_Started()
{
	Measurement_SetEvent(MEASUREMENT_EVT_STARTED);
}

void MeasurementADXL_Stopped()
{
	Measurement_SetEvent(MEASUREMENT_EVT_STOPPED);
}

void MeasurementADXL_DataReady()
{
	Measurement_SetEvent(MEASUREMENT_EVT_DATA_READY);
}

void MeasurementRxCompleted(){
	Measurement_SetEvent(MEASUREMENT_EVT_RX_COMPLETED);
}

void MeasurementTransmitCompleted(){
	Measurement_SetEvent(MEASUREMENT_EVT_TX_COMPLETED);
}

void MeasurementFSM_Init()
{
	Fsm_Init(&MeasurementFsmContext, MeasurementWaiting_StateHandler, &MeasurementFsmData, MeasurementErrorCallback);

}

#define MEASURMENT_STACK_SIZE 512U

measurement_error_t Measurement_Init(MeasurementInitStruct *init_data, measurement_err_callback error_cb)
{
	measurement_error_t ret_val = MEAS_INIT_FAILURE;

	ADXL_Init_t init_struct = {init_data->number_of_fifo_samples, &adxl_callbacks};

	if( ADXL_Driver_Init(&init_struct) == ADXL_SUCCESS )
	{
		MeasurementFsmData.number_of_fifo_samples = init_data->number_of_fifo_samples;
		MeasurementProcessingSubFsm_Init(Measurement_SetEvent, init_data->number_of_fifo_samples);
		MeasurementFSM_Init();
		ret_val = MEAS_NO_ERROR;

		Measurement_EvtQueue = xQueueCreate(EVT_BUFFER_CAPACITY, sizeof(FsmEvent_t));
		xTaskCreate(Measurement_task, "task_measurement", MEASURMENT_STACK_SIZE, NULL, 4, &measurement_task_handle);
		Measurement_SetEvent(FSM_INITIAL_EVENT);
	}
	return ret_val;

}

static bool IsIsr()
{
	if(__get_IPSR() == 0)
	{
		return false;
	}
	else
	{
		return true;
	}
}

void Measurement_SetEvent(MeasurementEvt_t evt)
{
	FsmEvent_t user_event = {evt, NULL};

	if(IsIsr())
	{
		BaseType_t xHigherPriorityTaskWoken;
		xQueueSendFromISR(Measurement_EvtQueue, &user_event, &xHigherPriorityTaskWoken);
		portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
	}
	else
	{
		xQueueSend(Measurement_EvtQueue, &user_event, 0);
	}
}


void Measurement_FSM_ProcessEvent(FsmEvent_t *user_event)
{
	(void)Fsm_ProcessEvent(&MeasurementFsmContext, user_event);
}

void Measurement_task(void *pvParameters)
{
	FsmEvent_t current_event;

	while(1)
	{
		if( xQueueReceive(Measurement_EvtQueue, &current_event, portMAX_DELAY) == pdPASS )
		{
			Measurement_FSM_ProcessEvent(&current_event);
		}
		else
		{
			// no event in queue
		}
	}
}


uint8_t data_in;

static FSM_ret MeasurementDiagReadout_StateHandler (fsm_context *ctx, FsmEvent_t *user_event)
{
	FSM_ret ret_val = FSM_OK;
	MeasurementEvt_t current_event = (MeasurementEvt_t)user_event->user_event;
	MeasurementFSM_Data_t *context_data = (MeasurementFSM_Data_t*)ctx->user_data;

	switch(current_event)
	{
		case FSM_INITIAL_EVENT:
			if(ADXL_GetConfig(context_data->diag_readout, MAX_DIAG_READOUT_LENGTH) == ADXL_SUCCESS)
			{
				if(UART_Com_TransmitStringNonBlocking(context_data->diag_readout) != TRANSMIT_OK )
				{
					ret_val = FSM_ERROR;
					context_data->last_error = MEAS_TX_FAILURE;
				}
			}
			else
			{
				ret_val = FSM_ERROR;
				context_data->last_error = MEAS_ADXL_FAILURE;
			}
			break;

		case MEASUREMENT_EVT_TX_COMPLETED:
			Fsm_StateTransition(ctx, MeasurementWaiting_StateHandler);
			break;

	}

	return ret_val;
}


static FSM_ret MeasurementWaiting_StateHandler (fsm_context *ctx, FsmEvent_t *user_event)
{

	FSM_ret ret_val = FSM_OK;
	MeasurementEvt_t current_event = (MeasurementEvt_t)user_event->user_event;
	MeasurementFSM_Data_t *context_data = (MeasurementFSM_Data_t*)ctx->user_data;

	switch(current_event)
	{
		case FSM_INITIAL_EVENT:
			if(UART_Com_ReceiveNonBlocking(&data_in, 1) != RECPETION_OK)
			{
				context_data->last_error = MEAS_RX_FAILURE;
				ret_val = FSM_ERROR;
			}
			break;

		case MEASUREMENT_EVT_RX_COMPLETED:
			if(data_in == START_SIGNAL)
			{
				context_data->measure_ctr = 0;
				Fsm_StateTransition(ctx, MeasurementGetSize_StateHandler);
			}
			else if(data_in == GET_CFG_SIGNAL) // use only for diagnostic purposes when stream measurements are stopped
			{
				Fsm_StateTransition(ctx, MeasurementDiagReadout_StateHandler);
			}
			break;

		default:
			context_data->last_error = MEAS_UNEXPECTED_EVENT;
			ret_val = FSM_ERROR;
			break;
	}
	return ret_val;
}

char helper_out_str[100];
char helper_in_str[100];

static FSM_ret MeasurementGetSize_StateHandler (fsm_context *ctx, FsmEvent_t *user_event)
{
	FSM_ret ret_val = FSM_OK;
	MeasurementEvt_t current_event = (MeasurementEvt_t)user_event->user_event;
	MeasurementFSM_Data_t *context_data = (MeasurementFSM_Data_t*)ctx->user_data;

	switch(current_event)
	{
		case FSM_INITIAL_EVENT:
			strcpy(helper_out_str, "START");
			if(UART_Com_TransmitStringNonBlocking(helper_out_str) == TRANSMIT_OK )
			{
				context_data->get_size_phase = GET_SIZE_WAIT_TX_COMPLETE;
			}
			else
			{
				context_data->last_error = MEAS_TX_FAILURE;
				ret_val = FSM_ERROR;
			}
			break;
		case MEASUREMENT_EVT_TX_COMPLETED:

			if(context_data->get_size_phase != GET_SIZE_WAIT_TX_COMPLETE)
			{
				context_data->last_error = MEAS_UNEXPECTED_EVENT;
				ret_val = FSM_ERROR;
				break;
			}

			if(UART_Com_GetBytesNonBlocking(helper_in_str, 2) == RECPETION_OK )
			{
				context_data->get_size_phase = GET_SIZE_WAIT_RX_COMPLETE;
			}
			else
			{
				context_data->last_error = MEAS_RX_FAILURE;
				ret_val = FSM_ERROR;
			}
			break;

		case MEASUREMENT_EVT_RX_COMPLETED:
			if(context_data->get_size_phase == GET_SIZE_WAIT_RX_COMPLETE)
			{
				uint16_t number_of_samples = ((uint16_t)helper_in_str[0])<<8 | helper_in_str[1];
				context_data->expected_size = number_of_samples;
				Fsm_StateTransition(ctx, MeasurementStarting_StateHandler);
				context_data->get_size_phase = GET_SIZE_IDLE;
			}
			else
			{
				context_data->last_error = MEAS_UNEXPECTED_EVENT;
				ret_val = FSM_ERROR;
			}
			break;
		default:
			context_data->last_error = MEAS_UNEXPECTED_EVENT;
			ret_val = FSM_ERROR;
			break;
	}
	return ret_val;
}

static FSM_ret MeasurementStarting_StateHandler (fsm_context *ctx, FsmEvent_t *user_event)
{
	FSM_ret ret_val = FSM_OK;
	MeasurementEvt_t current_event = (MeasurementEvt_t)user_event->user_event;
	MeasurementFSM_Data_t *context_data = (MeasurementFSM_Data_t*)ctx->user_data;

	switch(current_event)
	{
		case FSM_INITIAL_EVENT:
			ADXL_StartStreamMeasurements();
			break;
		case MEASUREMENT_EVT_STARTED:
			Fsm_StateTransition(ctx, MeasurementProcessing_StateHandler);
			break;
		default:
			context_data->last_error = MEAS_UNEXPECTED_EVENT;
			ret_val = FSM_ERROR;
			break;
	}
	return ret_val;
}

static FSM_ret MeasurementStopping_StateHandler (fsm_context *ctx, FsmEvent_t *user_event)
{
	FSM_ret ret_val = FSM_OK;
	MeasurementEvt_t current_event = (MeasurementEvt_t)user_event->user_event;
	MeasurementFSM_Data_t *context_data = (MeasurementFSM_Data_t*)ctx->user_data;

	switch(current_event)
	{
		case FSM_INITIAL_EVENT:
			ADXL_StopStreamMeasurements();
			break;
		case MEASUREMENT_EVT_STOPPED:
			Fsm_StateTransition(ctx, MeasurementWaiting_StateHandler);
			break;
		default:
			context_data->last_error = MEAS_UNEXPECTED_EVENT;
			ret_val = FSM_ERROR;
			break;
	}
	return ret_val;
}

static FSM_ret MeasurementProcessing_StateHandler (fsm_context *ctx, FsmEvent_t *user_event)
{
	FSM_ret ret_val = FSM_OK;
	MeasurementEvt_t current_event = (MeasurementEvt_t)user_event->user_event;
	MeasurementFSM_Data_t *context_data = (MeasurementFSM_Data_t*)ctx->user_data;

	switch(current_event)
	{
		case FSM_INITIAL_EVENT:
			user_event->user_event = MEASUREMENT_EVT_CONFIG_SET;
			user_event->user_data = (void*)(&(context_data->expected_size)); // fallthrough
		case MEASUREMENT_EVT_DATA_READY:
		case MEASUREMENT_EVT_TX_COMPLETED:
			if(MeasurementProcessingSubFsm_ProcessEvent(user_event) != FSM_OK)
			{
				context_data->last_error = MeasurementProcessingSubFsm_GetError();
				ret_val = FSM_ERROR;
			}
			break;

		case MEASUREMENT_EVT_READOUT_COMPLETED:
			Fsm_StateTransition(ctx, MeasurementStopping_StateHandler);
			break;
		default:
			context_data->last_error = MEAS_UNEXPECTED_EVENT;
			ret_val = FSM_ERROR;
			break;

	}
	return ret_val;
}

static FSM_ret MeasurementError_StateHandler (fsm_context *ctx, FsmEvent_t *user_event)
{
	FSM_ret ret_val = FSM_OK;
	MeasurementEvt_t current_event = (MeasurementEvt_t)user_event->user_event;
	MeasurementFSM_Data_t *context_data = (MeasurementFSM_Data_t*)ctx->user_data;
	switch(current_event)
	{
		case FSM_INITIAL_EVENT:
			UART_Com_TransmitError(context_data->last_error);
			break;
		default:
			break;
	}

	return ret_val;
}


