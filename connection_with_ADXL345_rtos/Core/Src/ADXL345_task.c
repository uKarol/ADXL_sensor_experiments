/*
 * ADXL345_taks.c
 *
 *  Created on: Jul 19, 2026
 *      Author: Karol
 */

#include "FreeRTOS.h"

#include "task.h"
#include "queue.h"



void ADXL_Task(void *pvParameters)
{
	TickType_t xLastWakeTime = xTaskGetTickCount();
	while(1)
	{
		vTaskDelayUntil( &xLastWakeTime, pdMS_TO_TICKS( 250 ) );
	}
}
