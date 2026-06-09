/*
 * ADXL_driver.h
 *
 *  Created on: Jun 9, 2026
 *      Author: Karol
 */

#ifndef INC_ADXL_DRIVER_H_
#define INC_ADXL_DRIVER_H_

#include <stdint.h>

typedef enum
{
	ADXL_SUCCESS,
	ADXL_FAILURE
}ADXL_status_t;

ADXL_status_t ADXL_ReadReg(uint8_t reg_id, uint8_t *pValueOut, uint8_t valueSize);
ADXL_status_t ADXL_WriteReg(uint8_t reg_id, uint8_t *DataIn, uint8_t DataSize);
ADXL_status_t ADXL_init_default();
ADXL_status_t ADXL_ReadData(int16_t *Xdata, int16_t *Ydata, int16_t *Zdata);

#endif /* INC_ADXL_DRIVER_H_ */
