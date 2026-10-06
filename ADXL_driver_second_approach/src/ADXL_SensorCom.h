/*
 * ADXL_i2c_conn.h
 *
 *  Created on: Jun 29, 2026
 *      Author: Karol
 */

#ifndef INC_ADXL_SENSOR_COM_H_
#define INC_ADXL_SENSOR_COM_H_

#include "ADXL_defs.h"

typedef enum
{
	ADXL_OP_NO_OPERATION,
	ADXL_WRITE_SINGLE_REG,
	ADXL_READ_SINGLE_REG,
	ADXL_READ_MULTIPLE_REGS,
	ADXL_OP_ERROR,
}ADXL_Operation_t;

typedef enum
{
	SINGLE_REG_WRITE_FINISH,
	SINGLE_REG_READ_FINISH,
	MULTIPLE_REGS_READ_FINISH,
	COMM_TIMEOUT,
	COMM_TIMER_FAILURE,
	UNEXPECTED_EVT,
}ADXL_ComEvt;

typedef void(*conn_evt_callback)(ADXL_ComEvt evt);

void ADXL_ComRxComplete(void);
void ADXL_ComTxComplete(void);
ADXL_status_t ADXL_Com_Init(conn_evt_callback callback);
ADXL_Operation_t ADXL_Com_GetCurrentOperation();

ADXL_Errors_t ADXL_ReadMultipleRegsNonBlocking(uint8_t reg_id, uint8_t *pValueOut, uint8_t size);
ADXL_Errors_t ADXL_ReadRegNonBlocking(uint8_t reg_id, uint8_t *pValueOut);
ADXL_Errors_t ADXL_WriteRegNonBlocking(uint8_t reg_id, uint8_t *DataIn);

ADXL_Errors_t ADXL_ReadRegBlocking(uint8_t reg_id, uint8_t *pValueOut);
ADXL_Errors_t ADXL_WriteRegBlocking(uint8_t reg_id, uint8_t DataIn);
ADXL_Errors_t ADXL_ReadMultipleRegsBlocking(uint8_t reg_id, uint8_t *pValueOut, uint8_t size);


#endif /* INC_ADXL_SENSOR_COM_H_ */
