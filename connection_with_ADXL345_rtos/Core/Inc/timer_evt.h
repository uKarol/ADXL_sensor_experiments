/*
 * timer_evt.h
 *
 *  Created on: Jul 19, 2026
 *      Author: Karol
 */

#ifndef INC_TIMER_EVT_H_
#define INC_TIMER_EVT_H_

#include "stdint.h"

typedef void (*timer_cb)(void);

uint8_t EvtTimerInit(timer_cb callback);
void EvtTimerStart(uint8_t timer_id, uint8_t period);
void EvtTimerStop(uint8_t timer_id);

#endif /* INC_TIMER_EVT_H_ */
