/*
 * MeasurementFSM.c
 *
 *  Created on: Jun 9, 2026
 *      Author: Karol
 */

#include "MeasurementFSM.h"
#include "ADXL_driver.h"
#include "UART_Communication.h"
#include "fsm.h"
#include "simple_queue.h"


#define READOUT_NUM 100

typedef struct
{
	measurement_error_t last_error;
	uint16_t measure_ctr;
	uint16_t expected_size;
	measurement_state_t current_state;
	uint8_t number_of_fifo_samples;
}MeasurementFSM_Data_t;

typedef uint16_t MeasurementEvt_t;

MeasurementFSM_Data_t MeasurementData;

#define EVT_BUFFER_CAPACITY (10U * sizeof(FsmEvent_t))

enum
{
	MEASUREMENT_EVT_TX_COMPLETED = FSM_BASIC_EVENT_NUM,
	MEASUREMENT_EVT_DATA_READY,
	MEASUREMENT_EVT_RX_COMPLETED,
};

uint8_t measurement_queue_buffer[EVT_BUFFER_CAPACITY];
SimpleQueue_t MeasurementQueue;

static FSM_ret MeasurementWaiting_StateHandler (fsm_context *ctx, FsmEvent_t *user_event);
static FSM_ret MeasurementProcessing_StateHandler (fsm_context *ctx, FsmEvent_t *user_event);
static FSM_ret MeasurementGetSize_StateHandler (fsm_context *ctx, FsmEvent_t *user_event);
static FSM_ret MeasurementError_StateHandler (fsm_context *ctx, FsmEvent_t *user_event);
void Measurement_SetEvent(MeasurementEvt_t evt);

enum
{
	ADXL_INIT_FAILED,
	ADXL_INIT_SUCCESSFULL,
};

static fsm_context MeasurementFsmContext;
static MeasurementFSM_Data_t MeasurementFsmData;



static void MeasurementErrorCallback()
{
	Fsm_StateTransition(&MeasurementFsmContext, MeasurementError_StateHandler);
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
	Measurement_SetEvent(FSM_INITIAL_EVENT);
}


measurement_error_t Measurement_Init(MeasurementInitStruct *init_data, measurement_err_callback error_cb)
{
	measurement_error_t ret_val = MEAS_INIT_FAILURE;
	SimpleQueueInit(&MeasurementQueue, measurement_queue_buffer, EVT_BUFFER_CAPACITY);

	ADXL_Init_t init_struct = {init_data->number_of_fifo_samples};

	if( ADXL_RegInitAlternative(&init_struct) == ADXL_SUCCESS )
	{
		MeasurementFsmData.number_of_fifo_samples = init_data->number_of_fifo_samples;
		MeasurementFSM_Init();
		ret_val = MEAS_NO_ERROR;
	}
	return ret_val;

}

void Measurement_SetEvent(MeasurementEvt_t evt)
{
	FsmEvent_t user_event = {evt, NULL};
	SimpleQueuePut(&MeasurementQueue, (void*)(&user_event), sizeof(user_event));
}


void Measurement_FSM_ProcessEvent(FsmEvent_t *user_event)
{
	(void)Fsm_ProcessEvent(&MeasurementFsmContext, user_event);
}

void Measurement_task()
{
	FsmEvent_t current_event;

	if( SimpleQueueGet(&MeasurementQueue, (void*)(&current_event), sizeof(current_event)) == QUEUE_OK)
	{
		Measurement_FSM_ProcessEvent(&current_event);
	}
	else
	{
		// no event in queue
	}
}


uint8_t data_in;

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
				ret_val = FSM_ERROR;
			}
			break;

		case MEASUREMENT_EVT_RX_COMPLETED:
			if(data_in == START_SIGNAL)
			{
				context_data->measure_ctr = 0;
				Fsm_StateTransition(ctx, MeasurementGetSize_StateHandler);
			}
			else if(data_in == GET_CFG_SIGNAL)
			{
				char readout[150] = "";
				ADXL_GetConfig(readout, 150);
				UART_Com_TransmitString(readout);
				if(UART_Com_ReceiveNonBlocking(&data_in, 1) != RECPETION_OK)
				{
					ret_val = FSM_ERROR;
				}
			}
			break;

		default:
			ret_val = FSM_ERROR;
			break;
	}
	return ret_val;
}

static FSM_ret MeasurementGetSize_StateHandler (fsm_context *ctx, FsmEvent_t *user_event)
{
	FSM_ret ret_val = FSM_OK;
	MeasurementEvt_t current_event = (MeasurementEvt_t)user_event->user_event;
	MeasurementFSM_Data_t *context_data = (MeasurementFSM_Data_t*)ctx->user_data;

	switch(current_event)
	{
		case FSM_INITIAL_EVENT:
			 uint16_t number_of_samples = 0;
			 UART_Com_TransmitString("START");
			 if( UART_Com_GetSize(&number_of_samples) == RECPETION_OK)
			 {
				 context_data->expected_size = number_of_samples;
			 }
			 else
			 {
				 context_data->expected_size = 1;
			 }

			 Fsm_StateTransition(ctx, MeasurementProcessing_StateHandler);
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
			ADXL_StartStreamMeasurements();
			break;

		case MEASUREMENT_EVT_DATA_READY:
			uint8_t *captured_data = ADXL_GetStreamedData();
			if(UART_Com_TransmitString("OK\n") != TRANSMIT_OK )
			{
				ret_val = FSM_ERROR;
				break;
			}
			if( UART_Com_TransmitRawData(captured_data, (context_data->number_of_fifo_samples*ONE_SAMPLE_SIZE)) != TRANSMIT_OK )
			{
				ret_val = FSM_ERROR;
				break;
			}
			ADXL_ReleaseDataBuffer();
			context_data->measure_ctr++;
			if(context_data->measure_ctr >= context_data->expected_size)
			{
				context_data->measure_ctr = 0;

				ADXL_StopStreamMeasurements();
				Fsm_StateTransition(ctx, MeasurementWaiting_StateHandler);
			}
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

//void MeasurementFSM_run(MeasurementFSM_context_t *context)
//{
//	uint8_t *captured_data;
//
//	switch(context->current_state)
//	{
//		case MEASURE_PROCESSING:
//			ADXL_StreamStatus stream_status= ADXL_GetStreamStatus();
//			if(stream_status == STREAM_COMPLETED)
//			{
//				captured_data = ADXL_GetStreamedData();
//				UART_Com_TransmitString("OK\n");
//				UART_Com_TransmitRawData(captured_data, (context->number_of_fifo_samples*ONE_SAMPLE_SIZE));
//				ADXL_ReleaseDataBuffer();
//				context->measure_ctr++;
//				if(context->measure_ctr >= context->expected_size)
//				{
//					context->measure_ctr = 0;
//					context->current_state = MEASURE_WAITING;
//					ADXL_StopStreamMeasurements();
//				}
//			}
//			else if(stream_status == STREAM_ERROR)
//			{
//				context->current_state = MEASURE_ERROR;
//				context->current_error = ADXL_READ_FAILURE;
//			}
//			break;
//
//		 case MEASURE_WAITING:
//			 uint8_t sig;
////			 ADXL_FIFO_Check();
//			if( UART_Com_CheckStartSignal(&sig) == RECPETION_OK )
//			{
//				if(sig == START_SIGNAL)
//				{
//					context->current_state = MEASURE_GET_SIZE;
//				}
//				else if(sig == GET_CFG_SIGNAL)
//				{
//					char readout[150] = "";
//					ADXL_GetConfig(readout, 150);
//					UART_Com_TransmitString(readout);
//				}
//			}
//			break;
//
//		 case MEASURE_GET_SIZE:
//			 uint16_t number_of_samples = 0;
//			 UART_Com_TransmitString("START");
//			 if( UART_Com_GetSize(&number_of_samples) == RECPETION_OK)
//			 {
//				 context->expected_size = number_of_samples;
//			 }
//			 else
//			 {
//				 context->expected_size = 1;
//			 }
//			  ADXL_StartStreamMeasurements();
//			 context->current_state = MEASURE_PROCESSING;
//		break;
//
//		 case MEASURE_ERROR:
//			UART_Com_TransmitError(context->current_error);
//			break;
//
//		default:
//			/* shall never occure */
//			break;
//	  }
//}
