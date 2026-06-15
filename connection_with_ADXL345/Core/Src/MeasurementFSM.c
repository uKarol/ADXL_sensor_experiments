/*
 * MeasurementFSM.c
 *
 *  Created on: Jun 9, 2026
 *      Author: Karol
 */

#include "MeasurementFSM.h"
#include "ADXL_driver.h"
#include "UART_Communication.h"

#define READOUT_NUM 100

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

	switch(context->current_state)
	{
		case MEASURE_PROCESSING:
			if( ADXL_ReadData(&Xdata, &Ydata, &Zdata) == ADXL_SUCCESS)
			{
				context->measure_ctr++;
				UART_Com_TransmitData(Xdata, Ydata, Zdata);
				if(context->measure_ctr >= context->expected_size)
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
			 uint8_t sig;
			if( UART_Com_CheckStartSignal(&sig) == RECPETION_OK )
			{
				if(sig == START_SIGNAL)
				{
					context->current_state = MEASURE_GET_SIZE;
				}
				else if(sig == GET_CFG_SIGNAL)
				{
					char readout[150] = "";
					ADXL_GetConfig(readout, 150);
					UART_Com_TransmitString(readout);
				}
			}
			break;

		 case MEASURE_GET_SIZE:
			 uint16_t number_of_samples = 0;
			 UART_Com_TransmitString("START");
			 if( UART_Com_GetSize(&number_of_samples) == RECPETION_OK)
			 {
				 context->expected_size = number_of_samples;
			 }
			 else
			 {
				 context->expected_size = 100;
			 }
			 context->current_state = MEASURE_PROCESSING;
		break;

		 case MEASURE_ERROR:
			UART_Com_TransmitError(context->current_error);
			break;

		default:
			/* shall never occure */
			break;
	  }
}
