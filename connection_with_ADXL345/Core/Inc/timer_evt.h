/*
 * timer_evt.h
 *
 *  Created on: Jun 28, 2026
 *      Author: Karol
 */

#ifndef INC_TIMER_EVT_H_
#define INC_TIMER_EVT_H_

#include "tim.h"
#include "stdint.h"

#define EVT_TIMER_INVALID_ID 0xFFU

typedef enum
{
	EVT_TIMER_OK,
	EVT_TIMER_ERROR,
}EvtTimerStatus_t;

typedef void (*timer_cb)(void);

uint8_t EvtTimerInit(timer_cb callback);
EvtTimerStatus_t EvtTimerStart(uint8_t timer_id, uint32_t period);
EvtTimerStatus_t EvtTimerStop(uint8_t timer_id);

#endif /* INC_TIMER_EVT_H_ */
