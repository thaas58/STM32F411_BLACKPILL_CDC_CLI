/*
 * aht20.h
 *
 *  Created on: May 19, 2022
 *      Author: PickleRix - Alien Firmware Engineer
 */

#ifndef INC_AHT20_H_
#define INC_AHT20_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "cmsis_os.h"

#define AHT20_ADDR  			(0x38 << 1) // Use 8-bit address

// AHT20 Commands
#define AHT20_STATUS_CMD 		0x71 // Status command
#define AHT20_CMD_SOFTRESET 	0xBA // Soft reset command
#define AHT20_CMD_CALIBRATE 	0xBE // Calibration command
#define AHT20_CMD_TRIGGER 		0xAC // Trigger reading command

// Status bits
#define AHT20_STATUS_BUSY 		0x80 // Busy Status bit
#define AHT20_STATUS_CALIBRATED 0x08 // Calibrated Status bit
#define AHT20_STATUS_ERROR		~(AHT20_STATUS_BUSY|AHT20_STATUS_CALIBRATED)

bool AHT20_I2C_INIT(I2C_HandleTypeDef * hi2c);
HAL_StatusTypeDef Send_AHT20_Data(uint8_t * command_buffer, size_t size);
HAL_StatusTypeDef Get_AHT20_Data(uint8_t * data_buffer, size_t size);
uint8_t Get_AHT20_Status(void);

#ifdef __cplusplus
}
#endif

#endif /* INC_AHT20_H_ */
