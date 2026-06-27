/*
 * ADXL_FSM.h
 *
 *  Created on: Jun 26, 2026
 *      Author: Karol
 */

#ifndef INC_ADXL_FSM_H_
#define INC_ADXL_FSM_H_

typedef enum
{
	ADXL_EXTI_IRQ,
	DMA_COMPLETED,
	ADXL_FIFO_OVERRUN,
	ADXL_FIFO_READY,
	BUFFER_RELEASE_REQUEST,
	FSM_RESET,
	ERROR_OCCURED,
	STREAM_FINISHED,
	RESET_ERROR_REQUEST,
}ADXL_FSM_Events;

typedef void (*fsm_error_callback)(ADXL_Errors_t CurrentError);

#define EVT_BUFFER_CAPACITY (10U * sizeof(FsmEvent_t))

uint8_t* ADXL_FSM_GetDataBuffer(void);
void ADXL_FSM_Init(uint8_t fifo_samples, fsm_error_callback error_cb);
void ADXL_SetEvent(ADXL_FSM_Events evt);
void ADXL_FSM_ProcessEvent(FsmEvent_t *user_event);
ADXL_StreamStatus ADXL_FSM_GetStatus(void);

#endif /* INC_ADXL_FSM_H_ */
