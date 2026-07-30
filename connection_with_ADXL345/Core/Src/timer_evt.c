/*
 * timer_evt.c
 *
 *  Created on: Jun 28, 2026
 *      Author: Karol
 */
#include "timer_evt.h"

#define MAX_TMR_NUM 10

typedef enum
{
	TMR_NOT_ACTIVE,
	TMR_ACTIVE,
}tmr_state;

typedef struct
{
	timer_cb time_elapsed_cb;
	uint8_t timer_period;
	uint8_t period_ctr;
	tmr_state state;
}internal_timer;

static internal_timer internal_timers[MAX_TMR_NUM];

uint8_t used_tmr_cnt = 0;

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	if(htim == &htim6)
	{
		for(uint8_t i = 0; i < used_tmr_cnt; i++)
		{
			if(internal_timers[i].state == TMR_ACTIVE)
			{
				internal_timers[i].period_ctr++;
				if(internal_timers[i].period_ctr == internal_timers[i].timer_period)
				{
					internal_timers[i].time_elapsed_cb();
					internal_timers[i].state = TMR_NOT_ACTIVE;
				}
			}
		}
	}
}

uint8_t EvtTimerInit(timer_cb callback)
{
	uint8_t ret_val = used_tmr_cnt;
	if(used_tmr_cnt < MAX_TMR_NUM)
	{
		internal_timer* current_tmr = &(internal_timers[used_tmr_cnt]);
		current_tmr->time_elapsed_cb = callback;
		used_tmr_cnt++;
	}
	return ret_val;
}

EvtTimerStatus_t EvtTimerStart(uint8_t timer_id, uint32_t period)
{
	EvtTimerStatus_t ret_val = EVT_TIMER_ERROR;
	if(timer_id < MAX_TMR_NUM)
	{
		internal_timer* current_tmr = &(internal_timers[timer_id]);
		current_tmr->timer_period = period;
		current_tmr->period_ctr = 0;
		current_tmr->state = TMR_ACTIVE;
		ret_val = EVT_TIMER_OK;
	}
	return ret_val;
}

EvtTimerStatus_t EvtTimerStop(uint8_t timer_id)
{
	EvtTimerStatus_t ret_val = EVT_TIMER_ERROR;
	if(timer_id < MAX_TMR_NUM)
	{
		internal_timer* current_tmr = &(internal_timers[timer_id]);
		current_tmr->period_ctr = 0;
		current_tmr->state = TMR_NOT_ACTIVE;
		ret_val = EVT_TIMER_OK;
	}
	return ret_val;
}
