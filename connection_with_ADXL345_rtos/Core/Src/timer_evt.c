/*
 * timer_evt.c
 *
 *  Created on: Jul 19, 2026
 *      Author: Karol
 */

#include "timer_evt.h"

#include <stdlib.h>

timer_cb time_elapsed_cb = NULL;
uint8_t timer_period;
uint8_t period_ctr;


void EvtTimerInit(timer_cb callback)
{
	time_elapsed_cb = callback;
}

void EvtTimerStart(uint8_t period)
{
	timer_period = period;
	period_ctr = 0;
}

