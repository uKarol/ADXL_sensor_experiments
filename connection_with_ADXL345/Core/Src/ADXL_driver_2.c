/*
 * ADXL_driver_2.c
 *
 *  Created on: Jun 25, 2026
 *      Author: Karol
 */

#include "fsm.h"
#include "i2c.h"
#include "ADXL_driver.h"
#include "ADXL_defs.h"
#include "simple_queue.h"
#include "stdio.h"
#include "stdbool.h"
#include "ADXL_FSM.h"
#include "ADXL_i2c_conn.h"

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


typedef struct
{
	ADXL_DriverState_t DriverState;
	ADXL_Errors_t LastError;
}ADXL_InternalState_t;

volatile static ADXL_InternalState_t CurrentState = {DRIVER_NOT_INITIALIZED, ADXL_ERR_NO_ERROR};


uint8_t adxl_queue_buffer[EVT_BUFFER_CAPACITY];
SimpleQueue_t ADXL_queue;

static void ADXL_SetEvent(ADXL_FSM_Events evt);

/**
 * @brief Read single ADXL register in blockin (polling) mode
 * @param in uint8_t ADXL register address
 * @param out uint8_t* pointer to data read from register
 * @returns ADXL_SUCCESS in case of successful operation
 * 			ADXL_FAILURE in case of error
 */

static bool ADXL_IsErrRecoveravle(ADXL_Errors_t curr_err)
{
	switch(curr_err)
	{
		case ADXL_ERR_NO_ERROR:				// fallthrough
		case ADXL_ERR_OVERRUN:				// fallthrough
		case ADXL_ERR_READOUT_INCOMPLETE:
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


/*
 * @brief write or check sequence of registers in blocking mode
 *
 */
static ADXL_Errors_t ADXL_RegSequencer(const RegConfDesc *reg_sequence, uint8_t seq_size, uint8_t *failed_step)
{
	ADXL_Errors_t ret_val = ADXL_ERR_NO_ERROR;

	for(uint8_t i =0; i < seq_size; i++)
	{
		if(reg_sequence[i].reg_op == WRITE_REG )
		{
			if( ADXL_WriteRegBlocking(reg_sequence[i].reg_addr, reg_sequence[i].reg_val) != ADXL_ERR_NO_ERROR )
			{
				*failed_step = i;
				ret_val = ADXL_ERR_COMMUNICATION_LOST;
				break;
			}
		}
		else
		{
			uint8_t axdl_out_val;
			if( ADXL_ReadRegBlocking(reg_sequence[i].reg_addr, &axdl_out_val) == ADXL_ERR_NO_ERROR )
			{
				if(axdl_out_val != reg_sequence[i].reg_val)
				{
					*failed_step = i;
					ret_val = ADXL_ERR_UNEXPECTED_REG_VAL;
					break;
				}
			}
			else
			{
				*failed_step = i;
				ret_val = ADXL_ERR_COMMUNICATION_LOST;
				break;
			}
		}
	}
	return ret_val;
}

void ADXL_task()
{
	FsmEvent_t current_event;
	if( SimpleQueueGet(&ADXL_queue, (void*)(&current_event), sizeof(current_event)) == QUEUE_OK)
	{
		ADXL_FSM_ProcessEvent(&current_event);
	}
	else
	{
		// no event in queue
	}
}

void ADXL_INT1InterruptHandler(void)
{
	if(CurrentState.DriverState == DRIVER_READY)
	{
		ADXL_SetEvent(ADXL_EVT_EXTI_IRQ);
	}
}

/**
 * @brief Get Status of readout stream
 */
ADXL_StreamStatus ADXL_GetStreamStatus(void)
{
	if(CurrentState.DriverState == DRIVER_READY)
	{
		return ADXL_FSM_GetStatus();
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
		ADXL_SetEvent(ADXL_EVT_BUFFER_RELEASE_REQ);
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
		ret_val = ADXL_FSM_GetDataBuffer();
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
		ADXL_SetEvent(ADXL_EVT_START_STREAM);
		CurrentState.DriverState = DRIVER_READY;
	}
	return ret_val;
}
ADXL_status_t ADXL_StopStreamMeasurements(void)
{
	ADXL_status_t ret_val = ADXL_FAILURE;
	if(CurrentState.DriverState == DRIVER_READY)
	{
		ADXL_Errors_t reg_status = ADXL_WriteRegBlocking(POWER_CTL, 0x0);
		if(reg_status == ADXL_ERR_NO_ERROR)
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

void ADXL_ConnCallback(ADXL_ConnEvt evt)
{
	switch(evt)
	{
	case SINGLE_REG_WRITE_FINISH:
		ADXL_SetEvent(ADXL_EVT_I2C_TX_COMPLETED);
		break;
	case SINGLE_REG_READ_FINISH: // fallthrough
	case MULTIPLE_REGS_READ_FINISH:
		ADXL_SetEvent(ADXL_EVT_I2C_RX_COMPLETED);
		break;
	case UNEXPECTED_EVT:
		ADXL_SetEvent(ADXL_EVT_ERROR_OCCURED);
		break;
	}
}

/**
 * @brief default function to perform initialization of ADXL module
 * @returns ADXL_SUCCESS in case of successful operation
 * 			ADXL_FAILURE in case of error
 */
ADXL_status_t ADXL_RegInitAlternative(ADXL_Init_t *init_data)
{
	ADXL_status_t ret_val = ADXL_SUCCESS;
	ADXLConn_Init(ADXL_ConnCallback);
	RegConfDesc reg_init_sequence[] = {
			{DEV_ID_REG, DEV_ID, VERIFY_REG},										// read DEV ID reg - check if device has proper ID
			{FIFO_CTL, ADXL_FIFO_CTL_STREAM | (init_data->FifoSamples & FIFO_CTL_SAMPLES_MASK), WRITE_REG},	// configure FIFO - set mode stream, set wartemark to 16 samples
			{FIFO_CTL, ADXL_FIFO_CTL_STREAM | (init_data->FifoSamples & FIFO_CTL_SAMPLES_MASK), VERIFY_REG},	// check if config was applied properly
			{INT_ENABLE_REG, ADXL_INT_ENABLE_WATERMARK | ADXL_INT_ENABLE_OVERRUN , WRITE_REG},				// enable interrupt watermark
			{INT_ENABLE_REG, ADXL_INT_ENABLE_WATERMARK | ADXL_INT_ENABLE_OVERRUN, VERIFY_REG},				// check if config was applied properly
			{INT_MAP_REG, 0, WRITE_REG},										// set int mapping to 0 - interrupts mapet to INT 1 output
			{BW_RATE_REG, 0xA, WRITE_REG}
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
		if( SeqState == ADXL_ERR_NO_ERROR )
		{
			CurrentState.DriverState = DRIVER_HALTED;
			SimpleQueueInit(&ADXL_queue, adxl_queue_buffer, EVT_BUFFER_CAPACITY);
			ret_val = ADXL_SUCCESS;
			ADXL_FSM_Init(init_data->FifoSamples, ADXL_SetError, ADXL_SetEvent);
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
		ADXL_SetError(ADXL_ERR_COMMUNICATION_LOST);
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
			if( ADXL_ReadRegBlocking(ReadoutRegs[i].reg_addr, &temp_val) == ADXL_ERR_NO_ERROR)
			{
				offset += snprintf(&(readout[offset]), max_size-offset, "%s %d\n", ReadoutRegs[i].reg_name, temp_val);
			}
			else
			{
				ADXL_SetError(ADXL_ERR_COMMUNICATION_LOST);
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
		if(ADXL_ERR_NO_ERROR == ADXL_ReadMultipleRegsBlocking(DATAX0_REG, all_regs, 6))
		{
			*Xdata = (int16_t)(all_regs[1]<<8)|(all_regs[0]);
			*Ydata = (int16_t)(all_regs[3]<<8)|(all_regs[2]);
			*Zdata = (int16_t)(all_regs[5]<<8)|(all_regs[4]);
			ret_val = ADXL_SUCCESS;
		}
		else
		{
			ADXL_SetError(ADXL_ERR_COMMUNICATION_LOST);
		}
	}
	else
	{
		ret_val = ADXL_NOT_INITIALIZED;
	}

 	return ret_val;
}

void ADXL_SetEvent(ADXL_FSM_Events evt)
{
	FsmEvent_t user_event = {evt, NULL};
	SimpleQueuePut(&ADXL_queue, (void*)(&user_event), sizeof(user_event));
}
