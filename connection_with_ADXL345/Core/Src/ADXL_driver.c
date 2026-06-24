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
#include <stdbool.h>

#define DEV_ID_REG 0U
#define DEV_ID 0xE5

#define REG_READ_NO 6
#define TEMP_STR_SIZE 20


#define MAX_NUMBER_OF_SAMPLES 32U // number of samples per watermark
#define MAX_READOUT_SIZE (ONE_SAMPLE_SIZE * MAX_NUMBER_OF_SAMPLES)

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


typedef struct
{
	ADXL_DriverState_t DriverState;
	ADXL_Errors_t LastError;
	ADXL_StreamStatus CurrentStreamState;
	uint8_t FifoSamplesNum;
	uint8_t readout_idx;
    uint32_t fifo_overrun_cnt;
    uint32_t readout_problem_cnt;
}ADXL_InternalState_t;

volatile static ADXL_InternalState_t CurrentState = {DRIVER_NOT_INITIALIZED, ADXL_NO_ERROR, STREAM_IDLE, 0, 0};
uint8_t ADXL_raw_data[MAX_READOUT_SIZE];

/*
 * private functions - decsription in code below
 */
static ADXL_Errors_t ADXL_RegSequencer(const RegConfDesc *reg_sequence, uint8_t seq_size, uint8_t *failed_step);
static ADXL_Errors_t ADXL_ReadReg(uint8_t reg_id, uint8_t *pValueOut);
static ADXL_Errors_t ADXL_WriteReg(uint8_t reg_id, uint8_t DataIn);
static void ADXL_StreamRead(void);


static bool ADXL_IsErrRecoveravle(ADXL_Errors_t curr_err)
{
	switch(curr_err)
	{
		case ADXL_NO_ERROR:				// fallthrough
		case ADXL_OVERRUN:				// fallthrough
		case ADXL_READOUT_INCOMPLETE:
			return true;
		default:
			return false;
	}
}


static inline void ADXL_SetError(ADXL_Errors_t CurrentError)
{
	CurrentState.LastError = CurrentError;
	if(!ADXL_IsErrRecoveravle(CurrentError))
	{
		CurrentState.DriverState = DRIVER_ERROR;
	}
}

ADXL_Errors_t ADXL_GetLastError(void)
{
	return CurrentState.LastError;
}

/**
 * @brief Read single ADXL register in blockin (polling) mode
 * @param in uint8_t ADXL register address
 * @param out uint8_t* pointer to data read from register
 * @returns ADXL_SUCCESS in case of successful operation
 * 			ADXL_FAILURE in case of error
 */
static ADXL_Errors_t ADXL_ReadReg(uint8_t reg_id, uint8_t *pValueOut)
{
	ADXL_Errors_t ret_val = ADXL_NO_ERROR;

	if(HAL_I2C_Mem_Read(&hi2c1, ADEXL_ID, reg_id, 1, pValueOut, 1, 100) != HAL_OK)
	{
		ret_val = ADXL_COMMUNICATION_LOST;
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
static ADXL_Errors_t ADXL_WriteReg(uint8_t reg_id, uint8_t DataIn)
{
	ADXL_Errors_t ret_val = ADXL_NO_ERROR;
	if(HAL_I2C_Mem_Write(&hi2c1, ADEXL_ID, reg_id, 1, &DataIn, 1, 100) != HAL_OK)
	{
		ret_val = ADXL_COMMUNICATION_LOST;
	}
	return ret_val;
}

/*
 * @brief write or check sequence of registers in blocking mode
 *
 */
static ADXL_Errors_t ADXL_RegSequencer(const RegConfDesc *reg_sequence, uint8_t seq_size, uint8_t *failed_step)
{
	ADXL_Errors_t ret_val = ADXL_NO_ERROR;

	for(uint8_t i =0; i < seq_size; i++)
	{
		if(reg_sequence[i].reg_op == WRITE_REG )
		{
			if( ADXL_WriteReg(reg_sequence[i].reg_addr, reg_sequence[i].reg_val) != ADXL_NO_ERROR )
			{
				*failed_step = i;
				ret_val = ADXL_COMMUNICATION_LOST;
				break;
			}
		}
		else
		{
			uint8_t axdl_out_val;
			if( ADXL_ReadReg(reg_sequence[i].reg_addr, &axdl_out_val) == ADXL_NO_ERROR )
			{
				if(axdl_out_val != reg_sequence[i].reg_val)
				{
					*failed_step = i;
					ret_val = ADXL_UNEXPECTED_REG_VAL;
					break;
				}
			}
			else
			{
				*failed_step = i;
				ret_val = ADXL_COMMUNICATION_LOST;
				break;
			}
		}
	}
	return ret_val;
}

static ADXL_Errors_t ADXL_FlushFifoInternal()
{
	ADXL_Errors_t ret_val = ADXL_NO_ERROR;
	uint8_t out_val;
	ADXL_Errors_t reg_status = ADXL_ReadReg(FIFO_STATUS, &out_val) ;
	if(reg_status == ADXL_NO_ERROR)
	{
		uint8_t samples_to_flush = out_val & FIFO_ENTRIES_BIT_MSK;

		uint8_t data_regs[6];
		for(uint8_t i = 0; i< samples_to_flush; i++){
			if(HAL_I2C_Mem_Read(&hi2c1, ADEXL_ID, DATAX0_REG, 1, data_regs, 6, 1000) != HAL_OK)
			{
				ret_val = ADXL_COMMUNICATION_LOST;
				break;
			}
		}
		if(ret_val == ADXL_NO_ERROR)
		{
			CurrentState.CurrentStreamState = STREAM_IDLE;
		}
	}
	else
	{
		ret_val = reg_status;
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
	// FIFO may be used during initialization of a driver or during runtime
	if(CurrentState.DriverState == DRIVER_READY)
	{
		ADXL_Errors_t fifo_status = ADXL_FlushFifoInternal();
		if(fifo_status == ADXL_NO_ERROR)
		{
			ret_val = ADXL_SUCCESS;
		}
		else
		{
			ADXL_SetError(fifo_status);
			ret_val = ADXL_FAILURE;
		}
	}
	else
	{
		ret_val = ADXL_NOT_INITIALIZED;
	}
	return ret_val;
}

ADXL_status_t ADXL_ResetDriver(void)
{
	return ADXL_RegInitAlternative();
}

ADXL_status_t ADXL_StartStreamMeasurements(void)
{
	// step 3 perform initial readout to reset int flags (without this step fifo will be overrun)
	ADXL_status_t ret_val = ADXL_FAILURE;
	if(CurrentState.DriverState == DRIVER_HALTED)
	{
		ADXL_Errors_t reg_status = ADXL_WriteReg(POWER_CTL, POWER_CTL_MEASURE);
		if(reg_status == ADXL_NO_ERROR)
		{
			ADXL_Errors_t FifoState = ADXL_FlushFifoInternal();
			if(FifoState == ADXL_NO_ERROR)
			{
				CurrentState.DriverState = DRIVER_READY;
				CurrentState.CurrentStreamState = STREAM_IDLE;
				CurrentState.readout_idx = 0;
				ret_val = ADXL_SUCCESS;
			}
			else
			{
				ADXL_SetError(FifoState);
			}
		}
		else
		{
			ADXL_SetError(reg_status);
		}
	}
	return ret_val;
}
ADXL_status_t ADXL_StopStreamMeasurements(void)
{
	ADXL_status_t ret_val = ADXL_FAILURE;
	if(CurrentState.DriverState == DRIVER_READY)
	{
		ADXL_Errors_t reg_status = ADXL_WriteReg(POWER_CTL, 0x0);
		if(reg_status == ADXL_NO_ERROR)
		{
			CurrentState.DriverState = DRIVER_HALTED;
			ret_val = ADXL_SUCCESS;
		}
		else
		{
			ADXL_SetError(reg_status);
		}
	}
	return ret_val;
}

/**
 * @brief default function to perform initialization of ADXL module
 * @returns ADXL_SUCCESS in case of successful operation
 * 			ADXL_FAILURE in case of error
 */
ADXL_status_t ADXL_RegInitAlternative(ADXL_Init_t *init_data)
{
	ADXL_status_t ret_val = ADXL_SUCCESS;

	RegConfDesc reg_init_sequence[] = {
			{DEV_ID_REG, DEV_ID, VERIFY_REG},										// read DEV ID reg - check if device has proper ID
			{FIFO_CTL, ADXL_FIFO_CTL_STREAM | (init_data->FifoSamples & FIFO_CTL_SAMPLES_MASK), WRITE_REG},	// configure FIFO - set mode stream, set wartemark to 16 samples
			{FIFO_CTL, ADXL_FIFO_CTL_STREAM | (init_data->FifoSamples & FIFO_CTL_SAMPLES_MASK), VERIFY_REG},	// check if config was applied properly
			{INT_ENABLE_REG, ADXL_INT_ENABLE_WATERMARK | ADXL_INT_ENABLE_OVERRUN , WRITE_REG},				// enable interrupt watermark
			{INT_ENABLE_REG, ADXL_INT_ENABLE_WATERMARK | ADXL_INT_ENABLE_OVERRUN, VERIFY_REG},				// check if config was applied properly
			{INT_MAP_REG, 0, WRITE_REG},										// set int mapping to 0 - interrupts mapet to INT 1 output
//			{POWER_CTL, POWER_CTL_MEASURE, WRITE_REG},							// set POWER_CTL_MEASURE - enable measurements
//			{POWER_CTL, POWER_CTL_MEASURE, VERIFY_REG},							// check if proper bits are set
	};
	// step 1
	HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_SET); // set CS to 1 - I2C mode
	HAL_GPIO_WritePin(SDO_GPIO_Port, SDO_Pin, GPIO_PIN_RESET); // set SDO to 0 - select address 0x53 (0xA6 - write 0xA7 read)

	// check if sensor is on bus
	if(HAL_I2C_IsDeviceReady(&hi2c1, ADEXL_ID, 5, 100) == HAL_OK)
	{
		// step 2 - configure adxl registers
		uint8_t failed_step = 0xFF;
		uint8_t sequence_size = sizeof(reg_init_sequence)/sizeof(reg_init_sequence[0]);
		ADXL_Errors_t SeqState = ADXL_RegSequencer(reg_init_sequence, sequence_size, &failed_step);
		if( SeqState == ADXL_NO_ERROR )
		{
			CurrentState.DriverState = DRIVER_HALTED;
			CurrentState.FifoSamplesNum = init_data->FifoSamples;
			ret_val = ADXL_SUCCESS;
		}
		else
		{
			ret_val = ADXL_FAILURE;
			ADXL_SetError(SeqState);
		}

	}
	else
	{
		ret_val = ADXL_FAILURE; // sensor not respond
		ADXL_SetError(ADXL_COMMUNICATION_LOST);
	}

	return ret_val;

}


ADXL_status_t ADXL_GetConfig(char *readout, uint16_t max_size)
{
	ADXL_status_t ret_val = ADXL_SUCCESS;
	if((CurrentState.DriverState == DRIVER_READY) || (CurrentState.DriverState == DRIVER_HALTED))
	{
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
			if( ADXL_ReadReg(ReadoutRegs[i].reg_addr, &temp_val) == ADXL_NO_ERROR)
			{
				offset += snprintf(&(readout[offset]), max_size-offset, "%s %d\n", ReadoutRegs[i].reg_name, temp_val);
			}
			else
			{
				ADXL_SetError(ADXL_COMMUNICATION_LOST);
				ret_val = ADXL_FAILURE;
				break;
			}
		}
	}
	else
	{
		ret_val = ADXL_NOT_INITIALIZED;
	}
	return ret_val;
}

/**
 * breif Read data from ADXL in blocking mode
 */
ADXL_status_t ADXL_ReadData(int16_t *Xdata, int16_t *Ydata, int16_t *Zdata)
{
	ADXL_status_t ret_val = ADXL_FAILURE;

	if(CurrentState.DriverState == DRIVER_READY)
	{
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
		else
		{
			ADXL_SetError(ADXL_COMMUNICATION_LOST);
		}
	}
	else
	{
		ret_val = ADXL_NOT_INITIALIZED;
	}

 	return ret_val;
}

uint8_t dbg_ctr;
// for debug purposes
void ADXL_FIFO_Check(void)
{
	if(CurrentState.DriverState == DRIVER_READY)
	{
		uint8_t data_out;
		if(ADXL_ReadReg(FIFO_STATUS, &data_out) == ADXL_NO_ERROR)
		{
			dbg_ctr++;
		}
		if(ADXL_ReadReg(INT_SOURCE, &data_out) == ADXL_NO_ERROR)
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
		if(ADXL_ReadReg(INT_SOURCE, &data_out) == ADXL_NO_ERROR)
		{
			ADXL_ReadReg(FIFO_STATUS, &data_out);
			dbg_ctr++;
		}
	}
}

void ADXL_INT1InterruptHandler(void)
{
	if(CurrentState.DriverState == DRIVER_READY)
	{
		uint8_t out_val;
		ADXL_ReadReg(INT_SOURCE, &out_val);
		if(out_val & ADXL_INT_ENABLE_OVERRUN)
		{
			// overrun occured
			CurrentState.fifo_overrun_cnt++;
			ADXL_SetError(ADXL_OVERRUN);
			CurrentState.CurrentStreamState = STREAM_ERROR;
		}
		else if(out_val & ADXL_INT_ENABLE_WATERMARK)
		{
			if(ADXL_ReadReg(FIFO_STATUS, &out_val) == ADXL_NO_ERROR)
			{
				if((out_val & FIFO_ENTRIES_BIT_MSK) >= CurrentState.FifoSamplesNum )
				{
					ADXL_StreamRead();
				}
				else
				{
					CurrentState.CurrentStreamState = STREAM_ERROR;
					ADXL_SetError(ADXL_READOUT_INCOMPLETE);
					CurrentState.readout_problem_cnt++;
				}
			}
			else
			{
				ADXL_SetError(ADXL_COMMUNICATION_LOST);
			}
		}
		else
		{
			ADXL_SetError(ADXL_UNEXPECTED_BEHAVIOUR);
		}
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
			ADXL_SetError(ADXL_DMA_PROBLEM);
		}
	}
	else
	{
		CurrentState.CurrentStreamState = STREAM_ERROR;
	}
}


void ADXL_DMAStreamComplete(void)
{
	uint8_t fifo_status;
	ADXL_ReadReg(FIFO_STATUS, &fifo_status);
	char data[8];
	snprintf(data, 8, "FIFO %x\n", fifo_status);
	UART_Com_TransmitString(data);

	if(CurrentState.DriverState == DRIVER_READY)
	{
		if(STREAM_IN_PROGRESS == CurrentState.CurrentStreamState)
		{
			if(CurrentState.readout_idx == (CurrentState.FifoSamplesNum-1))
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
					ADXL_SetError(ADXL_DMA_PROBLEM);
				}
			}
		}
		else
		{
			CurrentState.CurrentStreamState = STREAM_ERROR;
		}
	}
}

/**
 * @brief Get Status of readout stream
 */
ADXL_StreamStatus ADXL_GetStreamStatus(void)
{
	if(CurrentState.DriverState == DRIVER_READY)
	{
		return CurrentState.CurrentStreamState;
	}
	else
	{
		return STREAM_NOT_INITIALIZED;
	}
}

/**
 * @brief Release buffer with samples
 */
void ADXL_ReleaseDataBuffer(void)
{
	if(CurrentState.DriverState == DRIVER_READY)
	{
		CurrentState.CurrentStreamState = STREAM_IDLE;
	}
}

/**
 * @brief Release buffer with samples
 * @returns NULL - in case of error
 * 			uint8_t* - address of buffer with samples - fixed size of 16
 */
uint8_t* ADXL_GetStreamedData(void)
{
	uint8_t *ret_val = NULL;
	if(CurrentState.DriverState == DRIVER_READY)
	{
		if(STREAM_COMPLETED == CurrentState.CurrentStreamState)
		{
			ret_val = ADXL_raw_data;
		}
	}
	return ret_val;
}
