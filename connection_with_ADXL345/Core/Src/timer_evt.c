/*
 * timer_evt.c
 *
 *  Created on: Jun 28, 2026
 *      Author: Karol
 */
#include "timer_evt.h"


timer_cb time_elapsed_cb = NULL;
uint8_t timer_period;
uint8_t period_ctr;

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	if(htim == &htim6)
	{
		period_ctr++;
		if(period_ctr == timer_period)
		{
			time_elapsed_cb();
			HAL_TIM_Base_Stop_IT(htim);
		}
	}
}

void EvtTimerInit(timer_cb callback)
{
	time_elapsed_cb = callback;
}

void EvtTimerStart(uint8_t period)
{
	timer_period = period;
	period_ctr = 0;
	HAL_TIM_Base_Start_IT(&htim6);
}
