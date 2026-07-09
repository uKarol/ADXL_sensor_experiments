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

typedef void (*timer_cb)(void);

void EvtTimerInit(timer_cb callback);
void EvtTimerStart(uint8_t period);

#endif /* INC_TIMER_EVT_H_ */
