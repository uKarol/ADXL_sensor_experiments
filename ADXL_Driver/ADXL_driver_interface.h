#ifndef ADXL_DRIVER_INTERFACE
#define ADXL_DRIVER_INTERFACE

#include "ADXL_defs.h"

ADXL_status_t ADXL_Driver_Init(ADXL_Init_t *init_data);
ADXL_status_t ADXL_GetConfig(char *readout, uint16_t max_size );
void ADXL_INT1InterruptHandler(void);
void ADXL_FIFO_Check();

ADXL_StreamStatus ADXL_GetStreamStatus(void);
void ADXL_I2CTxComplete(void);
uint8_t* ADXL_GetStreamedData(void);
ADXL_status_t ADXL_ReleaseDataBuffer(void);
void ADXL_DMAStreamComplete(void);

ADXL_Errors_t ADXL_GetLastError(void);
ADXL_status_t ADXL_ResetDriver(void);

ADXL_status_t ADXL_StartStreamMeasurements(void);
ADXL_status_t ADXL_StopStreamMeasurements(void);

#endif /* ADXL_DRIVER_INTERFACE */
