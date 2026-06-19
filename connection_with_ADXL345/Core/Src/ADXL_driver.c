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

#define ONE_SAMPLE_SIZE 6U // sample size in bytes
#define NUMBER_OF_SAMPLES 16U // number of samples per watermark
#define READOUT_SIZE (ONE_SAMPLE_SIZE * NUMBER_OF_SAMPLES)

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

typedef enum
{
	ADXL_NO_ERROR,
	ADXL_COMMUNICATION_LOST,
	ADXL_READOUT_INCOMPLETE,
	ADXL_UNEXPECTED_BEHAVIOUR
}ADXL_InterruptErrors_t;

volatile uint8_t ADXL_raw_data[READOUT_SIZE];


typedef struct
{
	ADXL_StreamStatus CurrentStreamState;
	uint8_t readout_idx;
}ADXL_InternalStreamState_t;

static ADXL_InternalStreamState_t CurrentState;

/*
 * private functions - decsription in code below
 */
static ADXL_status_t ADXL_RegSequencer(const RegConfDesc *reg_sequence, uint8_t seq_size, uint8_t *failed_step);
static ADXL_status_t ADXL_ReadReg(uint8_t reg_id, uint8_t *pValueOut);
static ADXL_status_t ADXL_WriteReg(uint8_t reg_id, uint8_t DataIn);
static void ADXL_StreamRead(void);

/**
 * @brief Read single ADXL register in blockin (polling) mode
 * @param in uint8_t ADXL register address
 * @param out uint8_t* pointer to data read from register
 * @returns ADXL_SUCCESS in case of successful operation
 * 			ADXL_FAILURE in case of error
 */
ADXL_status_t ADXL_ReadReg(uint8_t reg_id, uint8_t *pValueOut)
{
	ADXL_status_t ret_val = ADXL_FAILURE;
	if(HAL_I2C_Master_Transmit(&hi2c1, ADEXL_ID, &reg_id, 1, 100) == HAL_OK)
	{
		if(HAL_I2C_Master_Receive(&hi2c1, ADEXL_ID, pValueOut, 1, 100) == HAL_OK)
		{
			ret_val = ADXL_SUCCESS;
		}
	}
	return ret_val;
}

/**
 * @brief Read single ADXL register in blockin (polling) mode
 * @param in uint8_t ADXL register address
 * @param in uint8_t value to be written into register
 * @returns ADXL_SUCCESS in case of successful operation
 * 			ADXL_FAILURE in case of error
 */
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
			if( ADXL_ReadReg(reg_sequence[i].reg_addr, &axdl_out_val) == ADXL_SUCCESS )
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

/**
 * @brief Read all data from ADXL in order to clear FIFO (in blocking mode), this function should be called after init or overrun
 * @returns ADXL_SUCCESS in case of successful operation
 * 			ADXL_FAILURE in case of error
 */
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
			break;
		}
	}
	return ret_val;
}

/**
 * @brief default function to perform initialization of ADXL module
 * @returns ADXL_SUCCESS in case of successful operation
 * 			ADXL_FAILURE in case of error
 */
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

	CurrentState.CurrentStreamState = STREAM_IDLE;
	CurrentState.readout_idx = 0;
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
		if( ADXL_ReadReg(ReadoutRegs[i].reg_addr, &temp_val) == ADXL_SUCCESS )
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

/**
 * breif Read data from ADXL in blocking mode
 */
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

uint8_t dbg_ctr;
// for debug purposes
void ADXL_FIFO_Check(void)
{
	uint8_t data_out;
	if(ADXL_ReadReg(FIFO_STATUS, &data_out) == ADXL_SUCCESS)
	{
		dbg_ctr++;
	}
	if(ADXL_ReadReg(INT_SOURCE, &data_out) == ADXL_SUCCESS)
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
	if(ADXL_ReadReg(INT_SOURCE, &data_out) == ADXL_SUCCESS)
	{
		ADXL_ReadReg(FIFO_STATUS, &data_out);
		dbg_ctr++;
	}
}


void ADXL_ErrorHandler(ADXL_InterruptErrors_t current_err)
{
	// to be implemented in future
}

void ADXL_INT1InterruptHandler(void)
{
	uint8_t out_val;
	ADXL_ReadReg(INT_SOURCE, &out_val);
	if(out_val & ADXL_INT_ENABLE_WATERMARK)
	{
		if(ADXL_ReadReg(FIFO_STATUS, &out_val) == ADXL_SUCCESS)
		{
			if((out_val & FIFO_ENTRIES_BIT_MSK) >= NUMBER_OF_SAMPLES )
			{
				ADXL_StreamRead();
			}
			else
			{
				ADXL_ErrorHandler(ADXL_READOUT_INCOMPLETE);
			}
		}
		else
		{
			ADXL_ErrorHandler(ADXL_COMMUNICATION_LOST);
		}
	}
	else
	{
		ADXL_ErrorHandler(ADXL_UNEXPECTED_BEHAVIOUR);
	}
}

/**
 * @brief Start reading data from ADXL in non-blocking mode by means of DMA
 */
void ADXL_StreamRead(void)
{
	// read 96 bytes (DMA) and put them to array
	if(STREAM_IDLE == CurrentState.CurrentStreamState)
	{
		if( HAL_I2C_Mem_Read_DMA(&hi2c1, ADEXL_ID, DATAX0_REG, 1, ADXL_raw_data, ONE_SAMPLE_SIZE) == HAL_OK)
		{
			CurrentState.CurrentStreamState = STREAM_IN_PROGRESS;
		}
		else
		{
			CurrentState.CurrentStreamState = STREAM_ERROR;
		}
	}
	else
	{
		CurrentState.CurrentStreamState = STREAM_ERROR;
	}
}



void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
	if(hi2c == &hi2c1)
	{
		if(STREAM_IN_PROGRESS == CurrentState.CurrentStreamState)
		{
			if(CurrentState.readout_idx == (NUMBER_OF_SAMPLES-1))
			{
				CurrentState.CurrentStreamState = STREAM_COMPLETED;
				CurrentState.readout_idx = 0;
			}
			else
			{
				CurrentState.readout_idx++;
				if( HAL_I2C_Mem_Read_DMA(&hi2c1, ADEXL_ID, DATAX0_REG, 1, &(ADXL_raw_data[CurrentState.readout_idx * ONE_SAMPLE_SIZE]), ONE_SAMPLE_SIZE) == HAL_OK)
				{
					CurrentState.CurrentStreamState = STREAM_IN_PROGRESS;
				}
				else
				{
					CurrentState.CurrentStreamState = STREAM_ERROR;
				}
			}
		}
	}
}

ADXL_StreamStatus ADXL_GetStreamStatus(void)
{
	return CurrentState.CurrentStreamState;
}

void ADXL_ReleaseDataBuffer(void)
{
	CurrentState.CurrentStreamState = STREAM_IDLE;
}

volatile uint8_t* ADXL_GetStreamedData(void)
{
	volatile uint8_t *ret_val = NULL;
	if(STREAM_COMPLETED == CurrentState.CurrentStreamState)
	{
		ret_val = ADXL_raw_data;
	}
	return ret_val;
}
