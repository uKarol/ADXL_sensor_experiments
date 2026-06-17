/*
 * ADXL_driver.c
 *
 *  Created on: Jun 9, 2026
 *      Author: Karol
 */
#include "ADXL_defs.h"
#include "ADXL_driver.h"
#include "string.h"
#include "i2c.h"
#include <stdio.h>

#define DEV_ID_REG 0U
#define DEV_ID 0xE5

#define REG_READ_NO 6
#define TEMP_STR_SIZE 20

typedef struct
{
	uint8_t reg_addr;
	const char *reg_name;
}RegDesc_t;

typedef enum
{
	WRITE_REG,
	VERIFY_REG,
}RegOperation_t;

typedef struct
{
	uint8_t reg_addr;
	uint8_t reg_val;
	RegOperation_t reg_op;
}RegConfDesc;

static ADXL_status_t ADXL_RegSequencer(const RegConfDesc *reg_sequence, uint8_t seq_size, uint8_t *failed_step);
static ADXL_status_t ADXL_ReadReg(uint8_t reg_id, uint8_t *pValueOut, uint8_t valueSize);
static ADXL_status_t ADXL_WriteReg(uint8_t reg_id, uint8_t DataIn);

ADXL_status_t ADXL_SetupFifoAndInt();

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


ADXL_status_t ADXL_WriteReg(uint8_t reg_id, uint8_t DataIn)
{
	ADXL_status_t ret_val = ADXL_FAILURE;
	if(HAL_I2C_Mem_Write(&hi2c1, ADEXL_ID, reg_id, 1, &DataIn, 1, 100) == HAL_OK)
	{
		ret_val = ADXL_SUCCESS;
	}
	return ret_val;
}


static ADXL_status_t ADXL_RegSequencer(const RegConfDesc *reg_sequence, uint8_t seq_size, uint8_t *failed_step)
{
	ADXL_status_t ret_val = ADXL_SUCCESS;

	for(uint8_t i =0; i < seq_size; i++)
	{
		if(reg_sequence[i].reg_op == WRITE_REG )
		{
			if( ADXL_WriteReg(reg_sequence[i].reg_addr, reg_sequence[i].reg_val) != ADXL_SUCCESS )
			{
				*failed_step = i;
				ret_val = ADXL_FAILURE;
				break;
			}
		}
		else
		{
			uint8_t axdl_out_val;
			if( ADXL_ReadReg(reg_sequence[i].reg_addr, &axdl_out_val, 1) == ADXL_SUCCESS )
			{
				if(axdl_out_val != reg_sequence[i].reg_val)
				{
					*failed_step = i;
					ret_val = ADXL_FAILURE;
					break;
				}
			}
			else
			{
				*failed_step = i;
				ret_val = ADXL_FAILURE;
				break;
			}
		}
	}
	return ret_val;
}

ADXL_status_t ADXL_FlushFifo()
{
	ADXL_status_t ret_val = ADXL_SUCCESS;
	int16_t Xdata;
	int16_t Ydata;
	int16_t Zdata;
	for(uint8_t i = 0; i< 32; i++){
		if(ADXL_ReadData(&Xdata, &Ydata, &Zdata) != ADXL_SUCCESS)
		{
			ret_val = ADXL_FAILURE;
		}
	}
	return ret_val;
}

ADXL_status_t ADXL_RegInitAlternative()
{
	ADXL_status_t ret_val = ADXL_SUCCESS;

	RegConfDesc reg_init_sequence[] = {
			{DEV_ID_REG, DEV_ID, VERIFY_REG},										// read DEV ID reg - check if device has proper ID
			{FIFO_CTL, ADXL_FIFO_CTL_STREAM | ADFL_FIFO_16_SAMPLES, WRITE_REG},	// configure FIFO - set mode stream, set wartemark to 16 samples
			{FIFO_CTL, ADXL_FIFO_CTL_STREAM | ADFL_FIFO_16_SAMPLES, VERIFY_REG},	// check if config was applied properly
			{INT_ENABLE_REG, ADXL_INT_ENABLE_WATERMARK, WRITE_REG},				// enable interrupt watermark
			{INT_ENABLE_REG, ADXL_INT_ENABLE_WATERMARK, VERIFY_REG},				// check if config was applied properly
			{INT_MAP_REG, 0, WRITE_REG},										// set int mapping to 0 - interrupts mapet to INT 1 output
			{POWER_CTL, POWER_CTL_MEASURE, WRITE_REG},							// set POWER_CTL_MEASURE - enable measurements
			{POWER_CTL, POWER_CTL_MEASURE, VERIFY_REG},							// check if proper bits are set
	};
	// step 1
	HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_SET); // set CS to 1 - I2C mode
	HAL_GPIO_WritePin(SDO_GPIO_Port, SDO_Pin, GPIO_PIN_RESET); // set SDO to 0 - select address 0x53 (0xA6 - write 0xA7 read)

	// step 2 - configure adxl registers
	if(HAL_I2C_IsDeviceReady(&hi2c1, ADEXL_ID, 5, 100) == HAL_OK)
	{
		uint8_t failed_step = 0xFF;
		uint8_t sequence_size = sizeof(reg_init_sequence)/sizeof(reg_init_sequence[0]);
		if( ADXL_RegSequencer(reg_init_sequence, sequence_size, &failed_step) != ADXL_SUCCESS )
		{
			ret_val = ADXL_FAILURE;
		}
	}
	else
	{
		ret_val = ADXL_FAILURE;
	}
	// step 3 perform initial readout to reset int flags (without this step fifo will be overrun)

	ADXL_FlushFifo();

	return ret_val;
}


ADXL_status_t ADXL_GetConfig(char *readout, uint16_t max_size)
{
	ADXL_status_t ret_val = ADXL_SUCCESS;
	size_t offset = 0;
	const RegDesc_t ReadoutRegs[REG_READ_NO] =
	{
			{DATA_FORMAT_REG, "DATA_FORMAT"},
			{BW_RATE_REG, "BW_RATE"},
			{POWER_CTL, "POWER_CTL"},
			{OFSX_REG, "OFSX"},
			{OFSY_REG, "OFSY"},
			{OFSZ_REG, "OFSZ"}
	};

	for(uint8_t i = 0; i<REG_READ_NO; i++)
	{
		uint8_t temp_val;
		if( ADXL_ReadReg(ReadoutRegs[i].reg_addr, &temp_val, 1) == ADXL_SUCCESS )
		{
			offset += snprintf(&(readout[offset]), max_size-offset, "%s %d\n", ReadoutRegs[i].reg_name, temp_val);
		}
		else
		{
			ret_val = ADXL_FAILURE;
			break;
		}
	}
	return ret_val;
}

ADXL_status_t ADXL_ReadData(int16_t *Xdata, int16_t *Ydata, int16_t *Zdata)
{
	ADXL_status_t ret_val = ADXL_FAILURE;

	uint8_t all_regs[6];
	// read multiple registers from DATAX0 to DATAZ1 (6 regs starting from addr 0x32)
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


uint8_t success_rate;


uint8_t dbg_ctr;
// for debug purposes
void ADXL_FIFO_Check(void)
{
	uint8_t data_out;
	if(ADXL_ReadReg(FIFO_STATUS, &data_out, 1) == ADXL_SUCCESS)
	{
		dbg_ctr++;
	}
	if(ADXL_ReadReg(INT_SOURCE, &data_out, 1) == ADXL_SUCCESS)
	{
		if(data_out != 0)
		{
			dbg_ctr++;
			int16_t Xdata;
			int16_t Ydata;
			int16_t Zdata;
			for(int i = 0; i< 32; i++){
				ADXL_ReadData(&Xdata, &Ydata, &Zdata);
			}
		}

	}
	if(ADXL_ReadReg(INT_SOURCE, &data_out, 1) == ADXL_SUCCESS)
	{
		ADXL_ReadReg(FIFO_STATUS, &data_out, 1);
		dbg_ctr++;
	}
}

void ADXL_INT1InterruptHandler(void)
{
	uint8_t out_val;
	ADXL_ReadReg(INT_SOURCE, &out_val, 1);
	if(out_val & ADXL_INT_ENABLE_WATERMARK)
	{
//		for(int i = 0; i< 32; i++){
//			int16_t Xdata;
//			int16_t Ydata;
//			int16_t Zdata;
//			ADXL_ReadData(&Xdata, &Ydata, &Zdata);
//		}
		success_rate++;
	}
	ADXL_ReadReg(FIFO_STATUS, &out_val, 1);
	if(out_val==16)
	{
		success_rate++;
	}

}
