/*
 * ADXL_driver.c
 *
 *  Created on: Jun 9, 2026
 *      Author: Karol
 */
#include "ADXL_defs.h"
#include "ADXL_driver.h"

#include "i2c.h"

#define DEVID 0U

static ADXL_status_t ADXL_ReadReg(uint8_t reg_id, uint8_t *pValueOut, uint8_t valueSize);
static ADXL_status_t ADXL_WriteReg(uint8_t reg_id, uint8_t *DataIn, uint8_t DataSize);


ADXL_status_t ADXL_ReadReg(uint8_t reg_id, uint8_t *pValueOut, uint8_t valueSize)
{
	ADXL_status_t ret_val = ADXL_FAILURE;
	if(HAL_I2C_Master_Transmit(&hi2c1, ADEXL_ID, &reg_id, 1, 100) == HAL_OK)
	{
		if(HAL_I2C_Master_Receive(&hi2c1, ADEXL_ID, pValueOut, valueSize, 100) == HAL_OK)
		{
			ret_val = ADXL_SUCCESS;
		}
	}
	return ret_val;
}


ADXL_status_t ADXL_WriteReg(uint8_t reg_id, uint8_t *DataIn, uint8_t DataSize)
{
	ADXL_status_t ret_val = ADXL_FAILURE;
	if(HAL_I2C_Mem_Write(&hi2c1, ADEXL_ID, reg_id, 1, DataIn, DataSize, 100) == HAL_OK)
	{
		ret_val = ADXL_SUCCESS;
	}
	return ret_val;
}


ADXL_status_t ADXL_init_default(void)
{
	ADXL_status_t ret_val = ADXL_FAILURE;
	HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_SET); // set CS to 1
	HAL_GPIO_WritePin(SDO_GPIO_Port, SDO_Pin, GPIO_PIN_RESET);

	if (HAL_I2C_IsDeviceReady(&hi2c1, ADEXL_ID, 5, 100) == HAL_OK)
	{
		uint8_t i2c_send_data = POWER_CTL_MEASURE;
		uint8_t i2c_rec_data;
		if( ADXL_ReadReg(DEVID, &i2c_rec_data, 1) == ADXL_SUCCESS )
		{
			if( ADXL_WriteReg(POWER_CTL, &i2c_send_data, 1) == ADXL_SUCCESS )
			{
				if( ADXL_ReadReg(POWER_CTL, &i2c_rec_data, 1) == ADXL_SUCCESS )
				{
					ret_val = ADXL_SUCCESS;
				}
			}
		}
	}
	return ret_val;
}


ADXL_status_t ADXL_ReadData(int16_t *Xdata, int16_t *Ydata, int16_t *Zdata)
{
	ADXL_status_t ret_val = ADXL_FAILURE;

	uint8_t all_regs[6];
	HAL_StatusTypeDef state = HAL_I2C_Mem_Read(&hi2c1, ADEXL_ID, DATAX0_REG, 1, all_regs, 6, 1000);
	if( HAL_OK == state)
	{
		*Xdata = (int16_t)(all_regs[1]<<8)|(all_regs[0]);
		*Ydata = (int16_t)(all_regs[3]<<8)|(all_regs[2]);
		*Zdata = (int16_t)(all_regs[5]<<8)|(all_regs[4]);
		ret_val = ADXL_SUCCESS;
	}

 	return ret_val;
}
