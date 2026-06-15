/*
 * ADXL_defs.h
 *
 *  Created on: Jun 9, 2026
 *      Author: Karol
 */

#ifndef INC_ADXL_DEFS_H_
#define INC_ADXL_DEFS_H_

#define ADEXL_ID (0x53U<<1U)

#define DATAX0_REG 0x32U
#define DATAX1_REG 0x33U
#define DATAY0_REG 0x34U
#define DATAY1_REG 0x35U
#define DATAZ0_REG 0x36U
#define DATAZ1_REG 0x37U
#define OFSX_REG 0x1E
#define OFSY_REG 0x1F
#define OFSZ_REG 0x20
#define DATA_FORMAT_REG 0x31U
#define BW_RATE_REG 0x2CU

// adxl registers

#define POWER_CTL 0x2DU

// adxl bits
#define POWER_CTL_MEASURE (1U<<3U)


#endif /* INC_ADXL_DEFS_H_ */
