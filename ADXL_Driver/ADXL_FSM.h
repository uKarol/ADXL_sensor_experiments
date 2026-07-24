/*
 * ADXL_FSM.h
 *
 *  Created on: Jun 26, 2026
 *      Author: Karol
 */

#ifndef INC_ADXL_FSM_H_
#define INC_ADXL_FSM_H_

#include "ADXL_driver_interface.h"
#include "fsm.h"

enum
{
	ADXL_EVT_EXTI_IRQ = FSM_BASIC_EVENT_NUM,
	ADXL_EVT_FIFO_OVERRUN,
	ADXL_EVT_UNEXPCETED_WATERMARK,
	ADXL_EVT_UNEXEPECTED_IRQ,
	ADXL_EVT_UNKNOWN_IRQ,
	ADXL_EVT_FIFO_READY,
	ADXL_EVT_SENSOR_ENABLED,
	ADXL_EVT_FIFO_FLUSH_REQ,
	ADXL_EVT_BUFFER_RELEASE_REQ,
	ADXL_EVT_FSM_RESET,
	ADXL_EVT_ERROR_OCCURED,
	ADXL_EVT_STREAM_FINISHED,
	ADXL_EVT_RESET_ERROR_REQUEST,
	ADXL_EVT_TIMEOUT,
	ADXL_EVT_START_STREAM,
	ADXL_EVT_STOP_STREAM,
	ADXL_EVT_STOP_REQUEST,
	ADXL_EVT_STREAM_HALTED,
	ADXL_EVT_FIFO_CLEARED,
	ADXL_EVT_I2C_TX_COMPLETED, 
	ADXL_EVT_I2C_RX_COMPLETED, 
	ADXL_EVT_COMM_TIMEOUT,
};



typedef uint16_t ADXL_FSM_Events;

typedef void (*fsm_error_callback)(ADXL_Errors_t CurrentError);
typedef ADXL_status_t (*fsm_set_event_callback)(ADXL_FSM_Events evt);

#define EVT_BUFFER_CAPACITY (10U * sizeof(FsmEvent_t))

uint8_t* ADXL_FSM_GetDataBuffer(void);
void ADXL_FSM_Init(uint8_t fifo_samples, fsm_error_callback error_cb, fsm_set_event_callback event_cb, helper_external_callbacks *ext_callbacks);
void ADXL_FSM_ProcessEvent(FsmEvent_t *user_event);
ADXL_StreamStatus ADXL_FSM_GetStatus(void);

#endif /* INC_ADXL_FSM_H_ */
