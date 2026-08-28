/*
 * UART_Communication_FSM.h
 *
 *  Created on: Aug 1, 2026
 *      Author: Karol
 */

#ifndef INC_UART_COMMUNICATION_FSM_H_
#define INC_UART_COMMUNICATION_FSM_H_

#include "fsm.h"

UartCommStatus_t UartComSetFrame(uint8_t frame_type, uint16_t length, uint8_t *payload);
void CommFsmInit(UartComUpperLayerNotify UserUpperLayerNot);
void UartComTx_FSM_ProcessEvent(FsmEvent_t *user_event);
void UartComRx_FSM_ProcessEvent(FsmEvent_t *user_event);

#endif /* INC_UART_COMMUNICATION_FSM_H_ */
