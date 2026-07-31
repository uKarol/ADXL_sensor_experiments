/*
 * ADXL_driver_2.c
 *
 *  Created on: Jun 25, 2026
 *      Author: Karol
 */

#include "adxl_user_cfg.h"

#ifndef USE_OS

#include "fsm.h"
#include "i2c.h"
#include "ADXL_driver_task.h"
#include "ADXL_defs.h"
#include "stdio.h"
#include "stdbool.h"
#include "ADXL_FSM.h"
#include "ADXL_i2c_conn.h"
#include "simple_queue.h"


uint8_t adxl_queue_buffer[EVT_BUFFER_CAPACITY];
SimpleQueue_t ADXL_queue;


ADXL_status_t ADXL_Task_Init()
{
	ADXL_status_t ret_val = ADXL_FAILURE;
	if(SimpleQueueInit(&ADXL_queue, adxl_queue_buffer, EVT_BUFFER_CAPACITY)== QUEUE_OK)
	{
		ret_val = ADXL_SUCCESS;
	}
	return ret_val;
}

void ADXL_task()
{
	FsmEvent_t current_event;
	if( SimpleQueueGet(&ADXL_queue, (void*)(&current_event), sizeof(current_event)) == QUEUE_OK)
	{
		ADXL_FSM_ProcessEvent(&current_event);
	}
	else
	{
			// no event in queue
	}
}

ADXL_status_t ADXL_SetEvent(ADXL_FSM_Events evt)
{
	ADXL_status_t ret_val = ADXL_FAILURE;
	FsmEvent_t user_event = {evt, NULL};
	if(QUEUE_OK == SimpleQueuePut(&ADXL_queue, (void*)(&user_event), sizeof(user_event)) )
	{
		ret_val = ADXL_SUCCESS;
	}
	return ret_val;
}

#endif /* USE_OS */
