/*
 * ADXL_driver_task.h
 *
 *  Created on: Jul 19, 2026
 *      Author: Karol
 */

#ifndef ADXL_DRIVER_ADXL_DRIVER_TASK_H_
#define ADXL_DRIVER_ADXL_DRIVER_TASK_H_

#include "ADXL_defs.h"
#include "ADXL_FSM.h"

ADXL_status_t ADXL_SetEvent(ADXL_FSM_Events evt);
ADXL_status_t ADXL_Task_Init();



#endif /* ADXL_DRIVER_ADXL_DRIVER_TASK_H_ */
