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

typedef enum
{
	TRANSMIT_OK,
	TRANSMIT_FAILURE
}TransmissionStatus_t;

typedef enum
{
	RECPETION_OK,
	RECEPTION_FAILURE
}ReceptionStatus_t;

TransmissionStatus_t UART_Com_TransmitData(int16_t Xdata, int16_t Ydata, int16_t Zdata);
TransmissionStatus_t UART_Com_TransmitError(uint16_t ErrorCode);
ReceptionStatus_t UART_Com_CheckStartSignal(uint8_t *sig_out);
ReceptionStatus_t UART_Com_GetSize(uint16_t *size);

#endif /* INC_UART_COMMUNICATION_H_ */
