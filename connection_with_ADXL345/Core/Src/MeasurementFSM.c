/*
 * MeasurementFSM.c
 *
 *  Created on: Jun 9, 2026
 *      Author: Karol
 */

#include "MeasurementFSM.h"
#include "ADXL_driver.h"
#include "usart.h"
#include <stdio.h>

#define READOUT_NUM 100

#define UART_RX_MAX_SIZE 20
#define UART_TX_MAX_SIZE 20


void MeasurementFSM_setup(MeasurementFSM_context_t *context)
{
	context->measure_ctr = 0;

	if( ADXL_init_default() == ADXL_SUCCESS )
	{
		context->current_state = MEASURE_WAITING;
		context->current_error = ADXL_NO_ERROR;
	}
	else
	{
		context->current_state = MEASURE_ERROR;
		context->current_error = ADXL_INIT_FAILURE;
	}
}

void MeasurementFSM_run(MeasurementFSM_context_t *context)
{
	int16_t Xdata;
	int16_t Ydata;
	int16_t Zdata;

	uint8_t uart_data_in[UART_RX_MAX_SIZE];
	uint8_t uart_data_out[UART_TX_MAX_SIZE];

	switch(context->current_state)
	{
		case MEASURE_PROCESSING:
			if( ADXL_ReadData(&Xdata, &Ydata, &Zdata) == ADXL_SUCCESS)
			{
				context->measure_ctr++;
				sprintf((char*)uart_data_out, "OK, %3d, %3d, %3d\n\r", Xdata, Ydata, Zdata);
				HAL_UART_Transmit(&hlpuart1, uart_data_out, 20, 100);
				if(context->measure_ctr >= READOUT_NUM)
				{
					context->measure_ctr = 0;
					context->current_state = MEASURE_WAITING;
				}
			}
			else
			{
				context->current_state = MEASURE_ERROR;
				context->current_error = ADXL_READ_FAILURE;
			}
			break;

		 case MEASURE_WAITING:
			if(HAL_UART_Receive(&hlpuart1, &uart_data_in, 1, 10) == HAL_OK )
			{
				if(uart_data_in[0] == 0x55)
				{
					context->current_state = MEASURE_PROCESSING;
				}
			}
			break;

		 case MEASURE_ERROR:
			sprintf((char*)uart_data_out, "ERROR, %d\n", context->current_error);
			HAL_UART_Transmit(&hlpuart1, uart_data_out, sizeof(uart_data_out), 100);
			break;

		default:
			/* shall never occure */
			break;
	  }
}
