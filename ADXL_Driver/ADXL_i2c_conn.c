/*
 * ADXL_i2c_conn.c
 *
 *  Created on: Jun 29, 2026
 *      Author: Karol
 */

#include "i2c.h"
#include "ADXL_defs.h"
#include "ADXL_i2c_conn.h"
#include "timer_evt.h"

#define BLOCKING_DEF_TIMEOUT 100U
#define COMM_OPERATION_TIMEOUT 100U

static volatile ADXL_Operation_t Current_operation;


conn_evt_callback evt_callback;
uint8_t comm_evt_tmr_id;

void ADXL_CommTimeout(void)
{
	Current_operation = ADXL_OP_NO_OPERATION;
	evt_callback(COMM_TIMEOUT);
}

void ADXL_I2CTxComplete(void)
{
	EvtTimerStop(comm_evt_tmr_id);
	if(Current_operation == ADXL_WRITE_SINGLE_REG)
	{
		Current_operation = ADXL_OP_NO_OPERATION;
		evt_callback(SINGLE_REG_WRITE_FINISH);
	}
	else
	{
		Current_operation = ADXL_OP_NO_OPERATION;
		evt_callback(UNEXPECTED_EVT);
	}
}

void ADXL_I2CRxComplete(void)
{
	EvtTimerStop(comm_evt_tmr_id);
	switch(Current_operation)
	{
		case ADXL_READ_SINGLE_REG:
			Current_operation = ADXL_OP_NO_OPERATION;
			evt_callback(SINGLE_REG_READ_FINISH);
			break;

		case ADXL_READ_MULTIPLE_REGS:
			Current_operation = ADXL_OP_NO_OPERATION;
			evt_callback(MULTIPLE_REGS_READ_FINISH);
			break;
		default:
			Current_operation = ADXL_OP_NO_OPERATION;
			evt_callback(UNEXPECTED_EVT);
	}
}

void ADXLConn_Init(conn_evt_callback callback)
{
	evt_callback = callback;
	Current_operation = ADXL_OP_NO_OPERATION;
	comm_evt_tmr_id = EvtTimerInit(ADXL_CommTimeout);
}


ADXL_Operation_t ADXLConn_GetCurrentOperation()
{
	return Current_operation;
}

ADXL_Errors_t ADXL_ReadMultipleRegsNonBlocking(uint8_t reg_id, uint8_t *pValueOut, uint8_t size)
{
	ADXL_Errors_t ret_val = ADXL_ERR_COMMUNICATION_LOST;
	if(Current_operation == ADXL_OP_NO_OPERATION)
	{
		Current_operation = ADXL_READ_MULTIPLE_REGS;
		if(HAL_I2C_Mem_Read_DMA(&hi2c1, ADEXL_ID, reg_id, 1, pValueOut, size) == HAL_OK )
		{
			EvtTimerStart(comm_evt_tmr_id, COMM_OPERATION_TIMEOUT);
			ret_val = ADXL_ERR_NO_ERROR;
		}
	}
	return ret_val;
}

ADXL_Errors_t ADXL_ReadRegNonBlocking(uint8_t reg_id, uint8_t *pValueOut)
{
	ADXL_Errors_t ret_val = ADXL_ERR_COMMUNICATION_LOST;
	if(Current_operation == ADXL_OP_NO_OPERATION)
	{
		Current_operation = ADXL_READ_SINGLE_REG;
		if(HAL_I2C_Mem_Read_DMA(&hi2c1, ADEXL_ID, reg_id, 1, pValueOut, 1) == HAL_OK )
		{
			EvtTimerStart(comm_evt_tmr_id, COMM_OPERATION_TIMEOUT);
			ret_val = ADXL_ERR_NO_ERROR;
		}
	}
	return ret_val;
}

ADXL_Errors_t ADXL_WriteRegNonBlocking(uint8_t reg_id, uint8_t *DataIn)
{
	ADXL_Errors_t ret_val = ADXL_ERR_COMMUNICATION_LOST;
	if(Current_operation == ADXL_OP_NO_OPERATION)
	{
		Current_operation = ADXL_WRITE_SINGLE_REG;
		if(HAL_I2C_Mem_Write_IT(&hi2c1, ADEXL_ID, reg_id, 1, DataIn, 1) == HAL_OK)
		{
			EvtTimerStart(comm_evt_tmr_id, COMM_OPERATION_TIMEOUT);
			ret_val = ADXL_ERR_NO_ERROR;

		}
	}
	return ret_val;
}

ADXL_Errors_t ADXL_WriteRegBlocking(uint8_t reg_id, uint8_t DataIn)
{
	ADXL_Errors_t ret_val = ADXL_ERR_NO_ERROR;
	if(HAL_I2C_Mem_Write(&hi2c1, ADEXL_ID, reg_id, 1, &DataIn, 1, BLOCKING_DEF_TIMEOUT) != HAL_OK)
	{
		ret_val = ADXL_ERR_COMMUNICATION_LOST;
	}
	return ret_val;
}

ADXL_Errors_t ADXL_ReadRegBlocking(uint8_t reg_id, uint8_t *pValueOut)
{
	ADXL_Errors_t ret_val = ADXL_ERR_NO_ERROR;

	if(HAL_I2C_Mem_Read(&hi2c1, ADEXL_ID, reg_id, 1, pValueOut, 1, BLOCKING_DEF_TIMEOUT) != HAL_OK)
	{
		ret_val = ADXL_ERR_COMMUNICATION_LOST;
	}
	return ret_val;
}

ADXL_Errors_t ADXL_ReadMultipleRegsBlocking(uint8_t reg_id, uint8_t *pValueOut, uint8_t size)
{
	ADXL_Errors_t ret_val = ADXL_ERR_NO_ERROR;

	if(HAL_I2C_Mem_Read(&hi2c1, ADEXL_ID, reg_id, 1, pValueOut, size, BLOCKING_DEF_TIMEOUT) != HAL_OK )
	{
		ret_val = ADXL_ERR_COMMUNICATION_LOST;
	}
	return ret_val;
}

