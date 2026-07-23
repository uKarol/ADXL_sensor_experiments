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

#define MAX_TMR_NUM 10

typedef struct
{
	timer_cb time_elapsed_cb;
	uint8_t timer_period;
	uint8_t period_ctr;
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
		current_tmr->period_ctr++;
		if(current_tmr->period_ctr == current_tmr->timer_period)
		{
			xTimerStop(current_tmr->internal_ostmr, 0);
			current_tmr->time_elapsed_cb();
		}
	}
}

// return value is id of timer
uint8_t EvtTimerInit(timer_cb callback)
{
	uint8_t ret_val = used_tmr_cnt;
	if(used_tmr_cnt < MAX_TMR_NUM)
	{
		internal_timer* current_tmr = &(internal_timers[used_tmr_cnt]);

		current_tmr->internal_ostmr = xTimerCreate( "my_evt_timer",
				pdMS_TO_TICKS(1),
				pdTRUE,
				NULL,
		evt_timer_callback );
		current_tmr->time_elapsed_cb = callback;
		used_tmr_cnt++;
	}
	return ret_val;
}

void EvtTimerStart(uint8_t timer_id, uint8_t period)
{
	if(timer_id < MAX_TMR_NUM)
	{
		internal_timer* current_tmr = &(internal_timers[timer_id]);
		current_tmr->timer_period = period;
		current_tmr->period_ctr = 0;
		xTimerStart(current_tmr->internal_ostmr, 0);
	}
}

void EvtTimerStop(uint8_t timer_id)
{
	if(timer_id < MAX_TMR_NUM)
	{
		internal_timer* current_tmr = &(internal_timers[timer_id]);
		current_tmr->period_ctr = 0;
		xTimerStop(current_tmr->internal_ostmr, 0);
	}
}
