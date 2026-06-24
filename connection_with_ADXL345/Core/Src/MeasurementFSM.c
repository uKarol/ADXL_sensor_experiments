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

void MeasurementFSM_setup(MeasurementFSM_context_t *context, MeasurementInitStruct *init_data)
{
	context->measure_ctr = 0;
	context->number_of_fifo_samples = init_data->number_of_fifo_samples;

	ADXL_Init_t init_struct = {init_data->number_of_fifo_samples};

	if( ADXL_RegInitAlternative(&init_struct) == ADXL_SUCCESS )
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
	uint8_t *captured_data;

	switch(context->current_state)
	{
		case MEASURE_PROCESSING:
			ADXL_StreamStatus stream_status= ADXL_GetStreamStatus();
			if(stream_status == STREAM_COMPLETED)
			{
				captured_data = ADXL_GetStreamedData();
				UART_Com_TransmitString("OK\n");
				UART_Com_TransmitRawData(captured_data, (context->number_of_fifo_samples*ONE_SAMPLE_SIZE));
				ADXL_ReleaseDataBuffer();
				context->measure_ctr++;
				if(context->measure_ctr >= context->expected_size)
				{
					context->measure_ctr = 0;
					context->current_state = MEASURE_WAITING;
					ADXL_StopStreamMeasurements();
				}
			}
			else if(stream_status == STREAM_ERROR)
			{
				context->current_state = MEASURE_ERROR;
				context->current_error = ADXL_READ_FAILURE;
			}
			break;

		 case MEASURE_WAITING:
			 uint8_t sig;
//			 ADXL_FIFO_Check();
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
				 context->expected_size = 5;
			 }
			  ADXL_StartStreamMeasurements();
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
