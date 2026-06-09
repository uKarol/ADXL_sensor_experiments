/*
 * UartTransmission.c
 *
 *  Created on: Jun 9, 2026
 *      Author: Karol
 */
#include "UART_Communication.h"
#include "usart.h"
#include <stdio.h>

#define START_SIGNAL 0x55
#define UART_RX_MAX_SIZE 20
#define UART_TX_MAX_SIZE 20

TransmissionStatus_t UART_Com_TransmitData(int16_t Xdata, int16_t Ydata, int16_t Zdata)
{
	TransmissionStatus_t ret_val = TRANSMIT_FAILURE;
	uint8_t uart_data_out[UART_TX_MAX_SIZE];
	sprintf((char*)uart_data_out, "OK, %3d, %3d, %3d\n\r", Xdata, Ydata, Zdata);
	if( HAL_UART_Transmit(&hlpuart1, uart_data_out, 20, 100) == HAL_OK)
	{
		ret_val = TRANSMIT_OK;
	}
	return ret_val;
}

TransmissionStatus_t UART_Com_TransmitError(uint16_t ErrorCode)
{
	TransmissionStatus_t ret_val = TRANSMIT_FAILURE;
	uint8_t uart_data_out[UART_TX_MAX_SIZE];
	sprintf((char*)uart_data_out, "ERROR, %d\n", ErrorCode);
	if(HAL_UART_Transmit(&hlpuart1, uart_data_out, sizeof(uart_data_out), 100) == HAL_OK)
	{
		ret_val = TRANSMIT_OK;
	}
	return ret_val;
}

ReceptionStatus_t UART_Com_CheckStartSignal(void)
{
	uint8_t uart_data_in[UART_RX_MAX_SIZE];
	ReceptionStatus_t ret_val = RECEPTION_FAILURE;
	if(HAL_UART_Receive(&hlpuart1, &uart_data_in, 1, 10) == HAL_OK )
	{
		if(uart_data_in[0] == START_SIGNAL)
		{
			ret_val = RECPETION_OK;
		}
	}
	return ret_val;
}
