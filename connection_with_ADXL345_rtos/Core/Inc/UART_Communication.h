/*
 * UartTransmission.h
 *
 *  Created on: Jun 9, 2026
 *      Author: Karol
 */

#ifndef INC_UART_COMMUNICATION_H_
#define INC_UART_COMMUNICATION_H_

#include "stdint.h"

#define GET_CFG_SIGNAL 0x56
#define START_SIGNAL 0x55

#include "fsm.h"

#define MAX_MSG_LENGTH 20
#define START_FIELD_SIZE 1
#define LENGTH_FIELD_SIZE 2
#define TYPE_FIELD_SIZE 1
#define MAX_RX_LENGTH 20
#define MAX_TX_LENGTH 120


typedef enum
{
	COMM_EVT_RX_COMPLETED = FSM_BASIC_EVENT_NUM,
	COMM_EVT_RX_TIMEOUT,
	COMM_EVT_TX_REQUEST,
	COMM_EVT_TX_CONFIRMATION,
	COMM_EVT_TX_TIMEOUT,
};

typedef struct
{
	uint16_t length;
	uint8_t frame_type;
	uint8_t payload[MAX_MSG_LENGTH];
}CommUart_frame_t;

typedef enum
{
	COMM_OK,
	COMM_ERROR,
}UartCommStatus_t;

typedef enum
{
	UART_COM_MESSAGE_RECEIVED,
	UART_COM_MESSAGE_TRANSMITTED,
	UART_COM_ERROR_DURING_TX,
}UartComNotificationType;

typedef void (*UartComSetEvent)(uint8_t evt);
typedef void (*UartComUpperLayerNotify)(UartComNotificationType evt_type);


void UART_ComRxCallback(void);
void UART_ComTxCallback(void);


UartCommStatus_t UART_ComInit(UartComSetEvent UserSetEv);
UartCommStatus_t UART_Com_TransmitRawData(uint8_t *data, uint32_t size);
UartCommStatus_t UART_Com_ReceiveNonBlocking(uint8_t *data_out, uint8_t size);
UartCommStatus_t UART_Com_TransmitRawDataNonBLocking(uint8_t *data, uint32_t size);
UartCommStatus_t UART_Com_ReceiveNonBlockingNoTimeout(uint8_t *data_out, uint8_t size);

#endif /* INC_UART_COMMUNICATION_H_ */
