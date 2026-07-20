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

timer_cb time_elapsed_cb = NULL;
uint8_t timer_period;
uint8_t period_ctr;

TimerHandle_t internal_tmr;

void evt_timer_callback( TimerHandle_t xTimer )
{
	period_ctr++;
	if(period_ctr == timer_period)
	{
		xTimerStop(internal_tmr, 0);
		time_elapsed_cb();
	}
}

void EvtTimerInit(timer_cb callback)
{

	internal_tmr = xTimerCreate( "my_evt_timer",
			pdMS_TO_TICKS(1),
			pdTRUE,
			NULL,
	evt_timer_callback );

	time_elapsed_cb = callback;
}

void EvtTimerStart(uint8_t period)
{
	timer_period = period;
	period_ctr = 0;
	xTimerStart(internal_tmr, 0);
}

