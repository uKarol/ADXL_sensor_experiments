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


#define UART_RX_MAX_SIZE 64
#define UART_TX_MAX_SIZE 64

TransmissionStatus_t UART_Com_TransmitData(int16_t Xdata, int16_t Ydata, int16_t Zdata)
{
	TransmissionStatus_t ret_val = TRANSMIT_FAILURE;
	uint8_t uart_data_out[UART_TX_MAX_SIZE];
	int len_str = snprintf((char*)uart_data_out, UART_TX_MAX_SIZE, "OK, %3d, %3d, %3d\n\r", Xdata, Ydata, Zdata);
	if( HAL_UART_Transmit(&hlpuart1, uart_data_out, len_str, 100) == HAL_OK)
	{
		ret_val = TRANSMIT_OK;
	}
	return ret_val;
}

TransmissionStatus_t UART_Com_TransmitString(uint8_t *str)
{
	TransmissionStatus_t ret_val = TRANSMIT_FAILURE;
	if( HAL_UART_Transmit(&hlpuart1, str, strlen(str), 100) != HAL_OK )
	{
		ret_val = TRANSMIT_OK;
	}
	return ret_val;
}

TransmissionStatus_t UART_Com_TransmitError(uint16_t ErrorCode)
{
	TransmissionStatus_t ret_val = TRANSMIT_FAILURE;
	uint8_t uart_data_out[UART_TX_MAX_SIZE];
	int len_str = snprintf((char*)uart_data_out, UART_TX_MAX_SIZE, "ERROR, %d\n", ErrorCode);
	if(HAL_UART_Transmit(&hlpuart1, uart_data_out, len_str, 100) == HAL_OK)
	{
		ret_val = TRANSMIT_OK;
	}
	return ret_val;
}

uint8_t uart_read_byte(UART_HandleTypeDef *huart)
{
	while(!(huart->Instance->ISR & USART_ISR_RXNE));
	return huart->Instance->RDR;
}

ReceptionStatus_t UART_Com_GetSize(uint16_t *size)
{
	uint8_t uart_data_in[2];
	ReceptionStatus_t ret_val = RECEPTION_FAILURE;

	HAL_StatusTypeDef hal_ret = HAL_UART_Receive(&hlpuart1, uart_data_in, 2, 2000);
	if( hal_ret == HAL_OK )
	{
		*size = ((uint16_t)uart_data_in[0])<<8 | uart_data_in[1];
		ret_val = RECPETION_OK;
	}
	return ret_val;
}

ReceptionStatus_t UART_Com_CheckStartSignal(uint8_t *sig_out)
{
	uint8_t uart_data_in;
	ReceptionStatus_t ret_val = RECEPTION_FAILURE;
	if(HAL_UART_Receive(&hlpuart1, &uart_data_in, 1, 10) == HAL_OK )
	{
		if(uart_data_in == START_SIGNAL)
		{
			*sig_out = START_SIGNAL;
			ret_val = RECPETION_OK;
		}
		else if(uart_data_in == GET_CFG_SIGNAL)
		{
			*sig_out = GET_CFG_SIGNAL;
			ret_val = RECPETION_OK;
		}
		else
		{

		}
	}
	return ret_val;
}
