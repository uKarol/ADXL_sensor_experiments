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
#include "helper_functions.h"
#include "UART_CommunicationTask.h"

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

typedef enum
{
	COMMAND_MESSAGE = 0,
	TEXT_MESSAGE,
	DATA_MESSAGE,
	ERROR_MESSAGE,
	UNKNOWN_MESSAGE,
}MeasMsgTypes;

typedef enum
{
	CMD_START_MEASUREMENT, // 1 byte
	CMD_STOP_MEASUREMENT,  // 1 byte
	CMD_GET_SAMPLES,       // 1 byte + 2 bytes
	CMD_GET_DIAG,		   // 1 byte
	CMD_RESET,             // 1 byte
	UNKNOWN_CMD,           // 1 byte
}MeasCommandTypes;

static void MeasurementMsgProcessCommand(uint8_t *command, uint8_t length)
{
	uint8_t bkp;
	switch(command[0])
	{
		case CMD_START_MEASUREMENT:
			Measurement_SetEvent(MEASUREMENT_EVT_START_REQUEST);
			break;
		case CMD_STOP_MEASUREMENT:
			Measurement_SetEvent(MEASUREMENT_EVT_STOP_REQUEST);
			break;
		case CMD_GET_SAMPLES:
			Measurement_SetEvent(MEASUREMENT_EVT_SAMPLES_REQUEST);
			break;
		case CMD_GET_DIAG:
			Measurement_SetEvent(MEASUREMENT_EVT_CONFIG_SET);
			break;
		default:
			// unexpeced / unsupported frame - ignore
			break;

	}
}

static void MeasurementMsgProcessing(CommUart_frame_t *msg_frame)
{
	switch(msg_frame->frame_type)
	{
		case COMMAND_MESSAGE:
			MeasurementMsgProcessCommand(msg_frame->payload, msg_frame->length);
			break;

		default:
			break;
	}
}

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


void MeasurementFSM_Init()
{
	Fsm_Init(&MeasurementFsmContext, MeasurementWaiting_StateHandler, &MeasurementFsmData, MeasurementErrorCallback);
}

void MeasurementCommEvt(UartComNotificationType evt_type)
{
	switch(evt_type)
	{
		case UART_COM_MESSAGE_RECEIVED:
			CommUart_frame_t *frame_out;
			GetReceivedFrame(&frame_out);
			MeasurementMsgProcessing(frame_out);
			break;
		case UART_COM_MESSAGE_TRANSMITTED:
			Measurement_SetEvent(MEASUREMENT_EVT_TX_COMPLETED);
			break;

		case UART_COM_ERROR_DURING_TX:
			Measurement_SetEvent(MEASUREMENT_EVT_TX_ERROR);
			break;
	}
}

#define MEASURMENT_STACK_SIZE 512U

measurement_error_t Measurement_Init(MeasurementInitStruct *init_data, measurement_err_callback error_cb)
{
	measurement_error_t ret_val = MEAS_INIT_FAILURE;
	UART_ComTaskInit(MeasurementCommEvt);
	ADXL_Init_t init_struct = {init_data->number_of_fifo_samples, &adxl_callbacks};

	if( ADXL_Driver_Init(&init_struct) == ADXL_SUCCESS )
	{
		MeasurementFsmData.number_of_fifo_samples = init_data->number_of_fifo_samples;
		MeasurementFSM_Init();
		ret_val = MEAS_NO_ERROR;
//		size_t heap_before = xPortGetFreeHeapSize();

		Measurement_EvtQueue = xQueueCreate(EVT_BUFFER_CAPACITY, sizeof(FsmEvent_t));
		xTaskCreate(Measurement_task, "task_measurement", MEASURMENT_STACK_SIZE, NULL, 4, &measurement_task_handle);
		Measurement_SetEvent(FSM_INITIAL_EVENT);
	}
	return ret_val;
}

uint8_t errors;

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
		if(pdPASS != xQueueSend(Measurement_EvtQueue, &user_event, 0))
		{
			errors++;
		}

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
			break;

		case MEASUREMENT_EVT_START_REQUEST:
			UartComSendFrame(TEXT_MESSAGE, 8, "STARTING");
			break;

		case MEASUREMENT_EVT_TX_COMPLETED:
			Fsm_StateTransition(ctx, MeasurementStarting_StateHandler);
			break;

		case MEASUREMENT_EVT_STOP_REQUEST:
			// ignore in this state
			break;

		case MEASUREMENT_EVT_SAMPLES_REQUEST:
			// not implemented yet
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
			break;

		case MEASUREMENT_EVT_DATA_READY:
			uint8_t *raw_adxl_data = ADXL_GetStreamedData();
			UartComSendFrame(DATA_MESSAGE, 96, raw_adxl_data);
			break;

		case MEASUREMENT_EVT_TX_COMPLETED:
			ADXL_ReleaseDataBuffer();
			break;

		case MEASUREMENT_EVT_TX_ERROR:
			ret_val = FSM_ERROR;
			break;

		case MEASUREMENT_EVT_STOP_REQUEST:
			Fsm_StateTransition(ctx, MeasurementStopping_StateHandler);
			// ignore in this state
			break;


		default:
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

			break;
		default:
			break;
	}

	return ret_val;
}


