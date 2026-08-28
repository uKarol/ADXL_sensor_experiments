/*
 * UART_CommunicationTask.h
 *
 *  Created on: Aug 24, 2026
 *      Author: Karol
 */

#ifndef INC_UART_COMMUNICATIONTASK_H_
#define INC_UART_COMMUNICATIONTASK_H_

#include "UART_Communication.h"

void UART_ComTaskInit(UartComUpperLayerNotify UserUpperLayerNot);
void UART_ComTask(void *pvParameters);

#endif /* INC_UART_COMMUNICATIONTASK_H_ */
