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

ADXL_status_t ADXL_init_default();
ADXL_status_t ADXL_ReadData(int16_t *Xdata, int16_t *Ydata, int16_t *Zdata);
ADXL_status_t ADXL_GetConfig(char *readout, uint16_t max_size );
void ADXL_INT1InterruptHandler(void);
void ADXL_FIFO_Check();
ADXL_status_t ADXL_RegInitAlternative();
#endif /* INC_ADXL_DRIVER_H_ */
