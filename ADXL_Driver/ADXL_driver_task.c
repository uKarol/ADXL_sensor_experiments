/*
 * ADXL_driver_task.c
 *
 *  Created on: Jul 19, 2026
 *      Author: Karol
 */

#include "adxl_user_cfg.h"

#ifdef USE_OS

#include "fsm.h"
#include "i2c.h"
#include "ADXL_driver_task.h"
#include "ADXL_defs.h"
#include "stdio.h"
#include "stdbool.h"
#include "ADXL_FSM.h"
#include "ADXL_i2c_conn.h"

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#define ADXL_STACK_SIZE 512U

QueueHandle_t ADXL_EvtQueue;
TaskHandle_t adxl_task_handle;

#define EVT_QUEUE_CAPACITY 10U

void ADXL_task(void *pvParameters);

ADXL_status_t ADXL_Task_Init()
{
	ADXL_status_t ret_val = ADXL_FAILURE;
	BaseType_t task_result;
	ADXL_EvtQueue = xQueueCreate(EVT_QUEUE_CAPACITY, sizeof(FsmEvent_t));
	if(ADXL_EvtQueue != NULL)
	{
	task_result = xTaskCreate(ADXL_task, "task_ADXL", ADXL_STACK_SIZE, NULL, 4, &adxl_task_handle);
		if(task_result == pdPASS)
		{
			ret_val = ADXL_SUCCESS;
		}
	}
	
	return ret_val;
}

void ADXL_task(void *pvParameters)
{
	FsmEvent_t current_event;

	while(1)
	{
		if( xQueueReceive(ADXL_EvtQueue, &current_event, portMAX_DELAY) == pdPASS )
		{
			ADXL_FSM_ProcessEvent(&current_event);
		}
		else
		{
				// no event in queue
		}
	}
}

static bool IsIsr()
{
	if(__get_IPSR() == 0)
	{
		return false;
	}
	else
	{
		return true;
	}
}

ADXL_status_t ADXL_SetEvent(ADXL_FSM_Events evt)
{
	ADXL_status_t ret_val = ADXL_FAILURE;
	FsmEvent_t user_event = {evt, NULL};
	if(IsIsr())
	{
		BaseType_t xHigherPriorityTaskWoken = pdFALSE;
		if( xQueueSendFromISR(ADXL_EvtQueue, &user_event, &xHigherPriorityTaskWoken) == pdPASS )
		{
			portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
			ret_val = ADXL_SUCCESS;
		}
	}
	else
	{
		if(xQueueSend(ADXL_EvtQueue, &user_event, 0) == pdPASS)
		{
			ret_val = ADXL_SUCCESS;
		}
	}
	return ret_val;
}

#endif // USE_RTOS
