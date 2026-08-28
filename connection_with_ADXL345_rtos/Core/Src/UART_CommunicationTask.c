/*
 * UART_CommunicationTask.c
 *
 *  Created on: Aug 24, 2026
 *      Author: Karol
 */

#include "UART_Communication.h"
#include "UART_CommunicationTask.h"
#include "UART_Communication_FSM.h"
#include "fsm.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "helper_functions.h"

QueueHandle_t UartCom_EvtQueue;
TaskHandle_t UartCom_task_handle;

#define EVT_BUFFER_CAPACITY (10U)
#define COM_STACK_SIZE 512U

void UART_ComSetEvent(uint8_t evt)
{
	FsmEvent_t user_event = {evt, NULL};

	if(IsIsr())
	{
		BaseType_t xHigherPriorityTaskWoken;
		xQueueSendFromISR(UartCom_EvtQueue, &user_event, &xHigherPriorityTaskWoken);
		portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
	}
	else
	{
		xQueueSend(UartCom_EvtQueue, &user_event, 0);
	}
}

UartCommStatus_t UartComSendFrame(uint8_t frame_type, uint16_t length, uint8_t *payload)
{
	UartCommStatus_t ret_val = COMM_ERROR;
	if(UartComSetFrame(frame_type, length, payload) == COMM_OK)
	{
		UART_ComSetEvent(COMM_EVT_TX_REQUEST);
		ret_val = COMM_OK;
	}
	return ret_val;
}

void UART_ComTaskInit(UartComUpperLayerNotify UserUpperLayerNot)
{
	CommFsmInit(UserUpperLayerNot);
	UART_ComInit(UART_ComSetEvent);
	UartCom_EvtQueue = xQueueCreate(EVT_BUFFER_CAPACITY, sizeof(FsmEvent_t));
	xTaskCreate(UART_ComTask, "task_uartCom", COM_STACK_SIZE, NULL, 4, &UartCom_task_handle);
	UART_ComSetEvent(FSM_INITIAL_EVENT);
}

void UART_ComTask(void *pvParameters)
{
	FsmEvent_t current_event;

	while(1)
	{
		if( xQueueReceive(UartCom_EvtQueue, &current_event, portMAX_DELAY) == pdPASS )
		{
			if(current_event.user_event < COMM_EVT_TX_REQUEST)
			{
				UartComRx_FSM_ProcessEvent(&current_event);
			}
			else
			{
				UartComTx_FSM_ProcessEvent(&current_event);
			}
		}
		else
		{
			// no event in queue
		}
	}
}
