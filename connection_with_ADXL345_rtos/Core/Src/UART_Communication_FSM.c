/*
 * UART_Communication_FSM.c
 *
 *  Created on: Aug 1, 2026
 *      Author: Karol
 */

#include "fsm.h"
#include "string.h"
#include "UART_Communication.h"
#include "UART_Communication_FSM.h"
#include <stdbool.h>

typedef struct
{
	CommUart_frame_t current_frame;
	uint8_t raw_rx_data[MAX_RX_LENGTH];
}CommunicationRxFSM_Data_t;

typedef struct
{
	uint8_t raw_tx_data[MAX_TX_LENGTH];
	uint8_t tx_raw_length;
	uint8_t tx_total_size;
}CommunicationTxFSM_Data_t;

typedef uint16_t CommEvt_t;
bool transmissionOngoing = false;

static fsm_context CommunicationRxFsmContext;
static fsm_context CommunicationTxFsmContext;
static CommunicationTxFSM_Data_t CommunicationTxFSM_Data;
static CommunicationRxFSM_Data_t CommunicationRxFSM_Data;
static void UartComPrepareFrame(uint8_t frame_type, uint16_t length, uint8_t *payload, uint8_t *frame_out);

UartComUpperLayerNotify UpperLayerNot;

static FSM_ret CommRxWaiting_StateHandler (fsm_context *ctx, FsmEvent_t *user_event);
static FSM_ret CommGetType_StateHandler (fsm_context *ctx, FsmEvent_t *user_event);
static FSM_ret CommGetLength_StateHandler (fsm_context *ctx, FsmEvent_t *user_event);
static FSM_ret CommGetPayload_StateHandler (fsm_context *ctx, FsmEvent_t *user_event);

static FSM_ret CommTxWaiting_StateHandler (fsm_context *ctx, FsmEvent_t *user_event);
static FSM_ret CommTransmittingData_StateHandler (fsm_context *ctx, FsmEvent_t *user_event);

UartCommStatus_t GetReceivedFrame(CommUart_frame_t **frame_out)
{
	*frame_out = &(CommunicationRxFSM_Data.current_frame);
	return COMM_OK;
}

void UartComRx_FSM_ProcessEvent(FsmEvent_t *user_event)
{
	(void)Fsm_ProcessEvent(&CommunicationRxFsmContext, user_event);
}

void UartComTx_FSM_ProcessEvent(FsmEvent_t *user_event)
{
	(void)Fsm_ProcessEvent(&CommunicationTxFsmContext, user_event);
}

static void Comm_ClearRxFsmContext()
{
	CommunicationRxFSM_Data.current_frame.length = 0;
}

static void Comm_ClearTxFsmContext()
{
	CommunicationTxFSM_Data.tx_raw_length = 0;
	CommunicationTxFSM_Data.tx_total_size = 0;
}

void CommTxErrorCallback()
{
	Comm_ClearTxFsmContext();
	Fsm_StateTransition(&CommunicationTxFsmContext, CommTxWaiting_StateHandler);
}

void CommRxErrorCallback()
{
	Comm_ClearRxFsmContext();
	Fsm_StateTransition(&CommunicationRxFsmContext, CommRxWaiting_StateHandler);
}

void CommFsmInit(UartComUpperLayerNotify UserUpperLayerNot)
{
	UpperLayerNot = UserUpperLayerNot;
	Fsm_Init(&CommunicationRxFsmContext, CommRxWaiting_StateHandler, &CommunicationRxFSM_Data, CommRxErrorCallback);
	Fsm_Init(&CommunicationTxFsmContext, CommTxWaiting_StateHandler, &CommunicationTxFSM_Data, CommTxErrorCallback);
}

static FSM_ret CommTxWaiting_StateHandler (fsm_context *ctx, FsmEvent_t *user_event)
{
	FSM_ret ret_val = FSM_OK;
	CommEvt_t current_event = (CommEvt_t)user_event->user_event;
	CommunicationTxFSM_Data_t *context_data = (CommunicationTxFSM_Data_t*)ctx->user_data;

	switch(current_event)
	{
		case FSM_INITIAL_EVENT:
			transmissionOngoing = false;
			break;
		case COMM_EVT_TX_REQUEST:
			Fsm_StateTransition(ctx, CommTransmittingData_StateHandler);
			break;
		default:
			ret_val = FSM_ERROR;
			break;
	}

	return ret_val;
}

static FSM_ret CommRxWaiting_StateHandler (fsm_context *ctx, FsmEvent_t *user_event)
{
	FSM_ret ret_val = FSM_OK;
	CommEvt_t current_event = (CommEvt_t)user_event->user_event;
	CommunicationRxFSM_Data_t *context_data = (CommunicationRxFSM_Data_t*)ctx->user_data;

	switch(current_event)
	{
		case COMM_EVT_RX_TIMEOUT: // fallthrough
		case FSM_INITIAL_EVENT:
			UART_Com_ReceiveNonBlockingNoTimeout(context_data->raw_rx_data, START_FIELD_SIZE);
			break;

		case COMM_EVT_RX_COMPLETED:
			if(context_data->raw_rx_data[0] == START_SIGNAL)
			{
				Fsm_StateTransition(ctx, CommGetType_StateHandler);
			}
			else
			{
				UART_Com_ReceiveNonBlockingNoTimeout(context_data->raw_rx_data, START_FIELD_SIZE);
			}
			break;

		default:
			ret_val = FSM_ERROR;
			break;
	}
	return ret_val;
}

static FSM_ret CommGetType_StateHandler (fsm_context *ctx, FsmEvent_t *user_event)
{
	FSM_ret ret_val = FSM_OK;
	CommEvt_t current_event = (CommEvt_t)user_event->user_event;
	CommunicationRxFSM_Data_t *context_data = (CommunicationRxFSM_Data_t*)ctx->user_data;

	switch(current_event)
	{
		case FSM_INITIAL_EVENT:
			UART_Com_ReceiveNonBlocking(context_data->raw_rx_data, TYPE_FIELD_SIZE);
			break;

		case COMM_EVT_RX_COMPLETED:
			context_data->current_frame.frame_type = context_data->raw_rx_data[0];
			Fsm_StateTransition(ctx, CommGetLength_StateHandler);
			break;

		case COMM_EVT_RX_TIMEOUT:
			ret_val = FSM_ERROR;
			break;

		default:
			ret_val = FSM_ERROR;
			break;

	}
	return ret_val;
}

static FSM_ret CommGetLength_StateHandler (fsm_context *ctx, FsmEvent_t *user_event)
{
	FSM_ret ret_val = FSM_OK;
	CommEvt_t current_event = (CommEvt_t)user_event->user_event;
	CommunicationRxFSM_Data_t *context_data = (CommunicationRxFSM_Data_t*)ctx->user_data;

	switch(current_event)
	{
		case FSM_INITIAL_EVENT:
			UART_Com_ReceiveNonBlocking(context_data->raw_rx_data, LENGTH_FIELD_SIZE);
			break;

		case COMM_EVT_RX_COMPLETED:
			context_data->current_frame.length = (uint16_t)context_data->raw_rx_data[0] | (uint16_t)(context_data->raw_rx_data[1]) << 8U;

			if((context_data->current_frame.length < MAX_RX_LENGTH) && (context_data->current_frame.length > 0))
			{
				Fsm_StateTransition(ctx, CommGetPayload_StateHandler);
			}
			else
			{
				ret_val = FSM_ERROR;
			}
			break;

		case COMM_EVT_RX_TIMEOUT:
			ret_val = FSM_ERROR;
			break;

		default:
			ret_val = FSM_ERROR;
			break;
	}
	return ret_val;
}

static FSM_ret CommGetPayload_StateHandler (fsm_context *ctx, FsmEvent_t *user_event)
{
	FSM_ret ret_val = FSM_OK;
	CommEvt_t current_event = (CommEvt_t)user_event->user_event;
	CommunicationRxFSM_Data_t *context_data = (CommunicationRxFSM_Data_t*)ctx->user_data;

	switch(current_event)
	{
		case FSM_INITIAL_EVENT:
			UART_Com_ReceiveNonBlocking(context_data->raw_rx_data, context_data->current_frame.length);
			break;

		case COMM_EVT_RX_COMPLETED:
			memcpy(context_data->current_frame.payload, context_data->raw_rx_data, context_data->current_frame.length);
			UpperLayerNot(UART_COM_MESSAGE_RECEIVED);
			Fsm_StateTransition(ctx, CommRxWaiting_StateHandler);
			break;

		case COMM_EVT_RX_TIMEOUT:
			ret_val = FSM_ERROR;
			break;

		default:
			ret_val = FSM_ERROR;
			break;

	}
	return ret_val;
}

static FSM_ret CommTransmittingData_StateHandler (fsm_context *ctx, FsmEvent_t *user_event)
{
	FSM_ret ret_val = FSM_OK;
	CommEvt_t current_event = (CommEvt_t)user_event->user_event;
	CommunicationTxFSM_Data_t *context_data = (CommunicationTxFSM_Data_t*)ctx->user_data;

	switch(current_event)
	{
		case FSM_INITIAL_EVENT:
			if( UART_Com_TransmitRawDataNonBLocking(context_data->raw_tx_data, context_data->tx_raw_length) != COMM_OK)
			{
				UpperLayerNot(UART_COM_ERROR_DURING_TX);
				ret_val = FSM_ERROR;
			}
			break;

		case COMM_EVT_TX_CONFIRMATION:
			UpperLayerNot(UART_COM_MESSAGE_TRANSMITTED);
			Fsm_StateTransition(ctx, CommTxWaiting_StateHandler);
			break;

		case COMM_EVT_TX_TIMEOUT:
			UpperLayerNot(UART_COM_ERROR_DURING_TX);
			ret_val = FSM_ERROR;
			break;

		default:
			ret_val = FSM_ERROR;
			break;
	}
	return ret_val;
}

#define DEFAULT_FRAME_OVERHEAD (START_FIELD_SIZE+TYPE_FIELD_SIZE+LENGTH_FIELD_SIZE)

UartCommStatus_t UartComSetFrame(uint8_t frame_type, uint16_t length, uint8_t *payload)
{
	UartCommStatus_t ret_val = COMM_ERROR;
	if((length < MAX_TX_LENGTH - DEFAULT_FRAME_OVERHEAD) && (!transmissionOngoing))
	{
		transmissionOngoing = true;
		CommunicationTxFSM_Data.tx_raw_length = DEFAULT_FRAME_OVERHEAD + length;
		UartComPrepareFrame(frame_type, length, payload, CommunicationTxFSM_Data.raw_tx_data);
		ret_val = COMM_OK;
	}
	return ret_val;
}

static void UartComPrepareFrame(uint8_t frame_type, uint16_t length, uint8_t *payload, uint8_t *frame_out)
{
	frame_out[0] = START_SIGNAL;
	frame_out[1] = frame_type;
	frame_out[2] = (length & 0xFFU);
	frame_out[3] = (uint8_t)(length >> 8);

	memcpy(&frame_out[4], payload, length);
}
