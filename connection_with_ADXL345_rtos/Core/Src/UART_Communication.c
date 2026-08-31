/*
 * UartTransmission.c
 *
 *  Created on: Jun 9, 2026
 *      Author: Karol
 */
#include "UART_Communication.h"
#include "usart.h"
#include <stdio.h>
#include <string.h>
#include "timer_evt.h"

#define UART_RX_MAX_SIZE 64
#define UART_TX_MAX_SIZE 64

#define DEFAULT_RX_TIMEOUT 100
#define DEFAULT_TX_TIMEOUT 100

typedef enum
{
	UART_COM_TX_UNINIT,
	UART_COM_TX_IDLE,
	UART_COM_TX_PROCESSING,
}UART_TxComState_t;

typedef enum
{
	UART_COM_RX_UNINIT,
	UART_COM_RX_IDLE,
	UART_COM_RX_PROCESSING,
}UART_RxComState_t;

volatile UART_TxComState_t UART_TxComState = UART_COM_TX_UNINIT;
volatile UART_RxComState_t UART_RxComState = UART_COM_RX_UNINIT;

static UartComSetEvent setEv;

uint8_t tx_timer_id;
uint8_t rx_timer_id;


static void UART_ComRxTimeoutCallback(void);
static void UART_ComTxTimeoutCallback(void);

UartCommStatus_t UART_ComInit(UartComSetEvent UserSetEv)
{
	UartCommStatus_t ret_val = COMM_ERROR;
	if(UserSetEv != NULL)
	{
		tx_timer_id = EvtTimerInit(UART_ComTxTimeoutCallback);
		rx_timer_id = EvtTimerInit(UART_ComRxTimeoutCallback);
		if((rx_timer_id != EVT_TIMER_INVALID_ID) && (tx_timer_id != EVT_TIMER_INVALID_ID))
		{
			ret_val = COMM_OK;
			setEv = UserSetEv;
			UART_TxComState = UART_COM_RX_IDLE;
			UART_RxComState = UART_COM_RX_IDLE;
		}
	}
	return ret_val;
}

static void UART_ComTxTimeoutCallback(void)
{
	if(UART_TxComState == UART_COM_TX_PROCESSING)
	{
		UART_TxComState = UART_COM_TX_IDLE;
		setEv(COMM_EVT_TX_TIMEOUT);
	}
}


static void UART_ComRxTimeoutCallback(void)
{
	if(UART_RxComState == UART_COM_RX_PROCESSING)
	{
		UART_RxComState = UART_COM_RX_IDLE;
		setEv(COMM_EVT_RX_TIMEOUT);
	}
}

void UART_ComRxCallback(void)
{
	if(UART_RxComState == UART_COM_RX_PROCESSING)
	{
		if(EvtTimerStop(rx_timer_id) == EVT_TIMER_OK)
		{
			UART_RxComState = UART_COM_RX_IDLE;
			setEv(COMM_EVT_RX_COMPLETED);
		}
	}
}

void UART_ComTxCallback(void)
{
	if(UART_TxComState == UART_COM_TX_PROCESSING)
	{
		if(EvtTimerStop(tx_timer_id) == EVT_TIMER_OK)
		{
			UART_TxComState = UART_COM_TX_IDLE;
			setEv(COMM_EVT_TX_CONFIRMATION);
		}
	}
}

UartCommStatus_t UART_Com_TransmitRawData(uint8_t *data, uint32_t size)
{
	UartCommStatus_t ret_val = COMM_ERROR;
	if(UART_TxComState == UART_COM_TX_IDLE)
	{
		if(HAL_UART_Transmit(&hlpuart1, data, size, 100) == HAL_OK)
		{
			ret_val = COMM_OK;
		}
	}
	return ret_val;
}

UartCommStatus_t UART_Com_TransmitRawDataNonBLocking(uint8_t *data, uint32_t size)
{
	UartCommStatus_t ret_val = COMM_ERROR;
	if(UART_TxComState == UART_COM_TX_IDLE)
	{
		if(HAL_UART_Transmit_IT(&hlpuart1, data, size) == HAL_OK)
		{
			if(EvtTimerStart(tx_timer_id, DEFAULT_TX_TIMEOUT) == EVT_TIMER_OK)
			{
				UART_TxComState = UART_COM_TX_PROCESSING;
				ret_val = COMM_OK;
			}
		}
	}
	return ret_val;
}

UartCommStatus_t UART_Com_ReceiveNonBlockingNoTimeout(uint8_t *data_out, uint8_t size)
{
	UartCommStatus_t ret_val = COMM_ERROR;
	if(UART_RxComState == UART_COM_RX_IDLE)
	{
		if(HAL_UART_Receive_IT(&hlpuart1, data_out, size) == HAL_OK)
		{
			UART_RxComState = UART_COM_RX_PROCESSING;
			ret_val = COMM_OK;
		}
	}
	return ret_val;
}

UartCommStatus_t UART_Com_ReceiveNonBlocking(uint8_t *data_out, uint8_t size)
{
	UartCommStatus_t ret_val = COMM_ERROR;
	if(UART_RxComState == UART_COM_RX_IDLE)
	{
		if(HAL_UART_Receive_IT(&hlpuart1, data_out, size) == HAL_OK)
		{
			if(EvtTimerStart(rx_timer_id, DEFAULT_TX_TIMEOUT) == EVT_TIMER_OK)
			{
				UART_RxComState = UART_COM_RX_PROCESSING;
				ret_val = COMM_OK;
			}
		}
	}
	return ret_val;
}

uint8_t uart_read_byte(UART_HandleTypeDef *huart)
{
	while(!(huart->Instance->ISR & USART_ISR_RXNE));
	return huart->Instance->RDR;
}

