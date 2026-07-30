/*
 * timer_evt.c
 *
 *  Created on: Jul 19, 2026
 *      Author: Karol
 */

#include "timer_evt.h"

#include <stdlib.h>

#include "FreeRTOS.h"
#include "timers.h"
#include "stdbool.h"
#include "cmsis_gcc.h"

#define MAX_TMR_NUM 10

typedef struct
{
	timer_cb time_elapsed_cb;
	TimerHandle_t internal_ostmr;
}internal_timer;

internal_timer internal_timers[MAX_TMR_NUM];

uint8_t used_tmr_cnt = 0;

internal_timer* get_timer_by_os_handle(TimerHandle_t xTimer)
{
	for(uint8_t i = 0; i < MAX_TMR_NUM; i++)
	{
		if(internal_timers[i].internal_ostmr == xTimer)
		{
			return &internal_timers[i];
		}
	}
	return NULL;
}

void evt_timer_callback( TimerHandle_t xTimer )
{
	internal_timer* current_tmr = get_timer_by_os_handle(xTimer);

	if(current_tmr != NULL)
	{
		current_tmr->time_elapsed_cb();
	}
}

// return value is id of timer
uint8_t EvtTimerInit(timer_cb callback)
{
	uint8_t ret_val = EVT_TIMER_INVALID_ID;
	if((used_tmr_cnt < MAX_TMR_NUM) && (callback != NULL))
	{
		internal_timer* current_tmr = &(internal_timers[used_tmr_cnt]);

		current_tmr->internal_ostmr = xTimerCreate( "my_evt_timer",
				pdMS_TO_TICKS(1),
				pdFALSE,
				NULL,
		evt_timer_callback );

		if(current_tmr->internal_ostmr != NULL)
		{
			current_tmr->time_elapsed_cb = callback;
			ret_val = used_tmr_cnt;
			used_tmr_cnt++;
		}
	}
	return ret_val;
}

EvtTimerStatus_t EvtTimerStart(uint8_t timer_id, uint32_t period)
{
	EvtTimerStatus_t ret_val = EVT_TIMER_ERROR;
	if(timer_id < MAX_TMR_NUM)
	{
		internal_timer* current_tmr = &(internal_timers[timer_id]);
		if(xTimerChangePeriod(current_tmr->internal_ostmr, pdMS_TO_TICKS(period), 0) == pdPASS)
		{
			if(xTimerStart(current_tmr->internal_ostmr, 0) == pdPASS)
			{
				ret_val = EVT_TIMER_OK;
			}
		}
	}
	return ret_val;
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

EvtTimerStatus_t EvtTimerStop(uint8_t timer_id)
{
	EvtTimerStatus_t ret_val = EVT_TIMER_ERROR;
	if(timer_id < MAX_TMR_NUM)
	{
		internal_timer* current_tmr = &(internal_timers[timer_id]);
		if(IsIsr())
		{
			if(xTimerStopFromISR(current_tmr->internal_ostmr, 0) == pdPASS)
			{
				ret_val = EVT_TIMER_OK;
			}
		}
		else
		{
			if(xTimerStop(current_tmr->internal_ostmr, 0) == pdPASS)
			{
				ret_val = EVT_TIMER_OK;
			}
		}
	}
	return ret_val;
}
