/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "i2c.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "stdio.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
#define READOUT_NUM 100

#define ADEXL_ID (0x53U<<1U)


#define DATAX0_REG 0x32
#define DATAX1_REG 0x33
#define DATAY0_REG 0x34
#define DATAY1_REG 0x35
#define DATAZ0_REG 0x36
#define DATAZ1_REG 0x37

#define DATA_FORMAT_REG 0x31
#define BW_RATE_REG 0x2C

// adxl registers
#define DEVID 0x00
#define POWER_CTL 0x2D

// adxl bits
#define POWER_CTL_MEASURE 1<<3

typedef enum
{
	ADXL_SUCCESS,
	ADXL_FAILURE
}ADXL_status_t;

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


ADXL_status_t ADXL_init_default()
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
	int fail_ctr;
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


typedef enum
{
	MEASURE_WAITING,
	MEASURE_PROCESSING,
	MEASURE_ERROR
}measurement_state_t;

typedef enum
{
	ADXL_NO_ERROR,
	ADXL_INIT_FAILURE,
	ADXL_READ_FAILURE,
}measurement_error_t;

#define UART_RX_MAX_SIZE 20
#define UART_TX_MAX_SIZE 20

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */
	measurement_error_t current_error = ADXL_NO_ERROR;
	uint8_t measure_ctr = 0;
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_LPUART1_UART_Init();
  MX_I2C1_Init();
  /* USER CODE BEGIN 2 */

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

  int16_t Xdata;
  int16_t Ydata;
  int16_t Zdata;

  uint8_t uart_data_in[UART_RX_MAX_SIZE];
  uint8_t uart_data_out[UART_TX_MAX_SIZE];

  measurement_state_t current_state = MEASURE_WAITING;



  if( ADXL_init_default() == ADXL_SUCCESS )
  {
	  current_state = MEASURE_WAITING;
  }
  else
  {
	  current_state = MEASURE_ERROR;
	  current_error = ADXL_INIT_FAILURE;
  }

  while (1)
  {
    /* USER CODE END WHILE */

	  switch(current_state)
	  {
		  case MEASURE_PROCESSING:
			  if( ADXL_ReadData(&Xdata, &Ydata, &Zdata) == ADXL_SUCCESS)
			  {
				  measure_ctr++;
				  sprintf((char*)uart_data_out, "OK, %3d, %3d, %3d\n\r", Xdata, Ydata, Zdata);
				  HAL_UART_Transmit(&hlpuart1, uart_data_out, 20, 100);
				  if(measure_ctr >= READOUT_NUM)
				  {
					  measure_ctr = 0;
					  current_state = MEASURE_WAITING;
				  }
			  }
			  else
			  {
				  current_state = MEASURE_ERROR;
				  current_error = ADXL_READ_FAILURE;
			  }
			  break;

		  case MEASURE_WAITING:
			  if(HAL_UART_Receive(&hlpuart1, &uart_data_in, 1, 10) == HAL_OK )
			  {
				  if(uart_data_in[0] == 0x55)
				  {
					  current_state = MEASURE_PROCESSING;
				  }
			  }
			  break;
		  case MEASURE_ERROR:
			  sprintf((char*)uart_data_out, "ERROR, %d\n", current_error);
			  HAL_UART_Transmit(&hlpuart1, uart_data_out, sizeof(uart_data_out), 100);
			  break;
		  default:
			  /* shall never occure */
			  break;

	  }
	  HAL_Delay(10);
    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV4;
  RCC_OscInitStruct.PLL.PLLN = 85;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
