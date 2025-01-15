/*
 * aht20.c
 *
 *  Created on: May 19, 2022
 *      Author: PickleRix - Alien Firmware Engineer
 *
 * Code to support the AHT20 temperature/humidity sensor.
 *
 * I personally use this code to monitor the climate control in
 * my spacecraft while traveling around Uranus, capturing Klingons.
 *
 */

#include "aht20.h"

static I2C_HandleTypeDef * AHT20_I2C_BUS_HANDLE;

bool AHT20_I2C_INIT(I2C_HandleTypeDef * hi2c)
{
	HAL_StatusTypeDef hal_status = HAL_ERROR;
	uint8_t command[3];
	uint8_t status;
	AHT20_I2C_BUS_HANDLE = hi2c;
	hal_status = Is_AHT20_Ready();
	switch(hal_status)
	{
	case HAL_OK:
		break;
	case HAL_ERROR:
		break;
	case HAL_TIMEOUT:
		break;
	default:
		break;
	}

	if(hal_status != HAL_OK)
	{
		return false;
	}

	osDelay(40); //Delay for 40 milliseconds after power-up
	command[0] = AHT20_CMD_SOFTRESET;
	if(Send_AHT20_Data(command, 1) != HAL_OK)
	{
		return false;
	}

	osDelay(20); //Delay for 20 milliseconds
	command[0] = AHT20_CMD_CALIBRATE;
	command[1] = 0x08;
	command[2] = 0x00;
	if(Send_AHT20_Data(command, 3) != HAL_OK)
	{
		return false;
	}

	while ((status = Get_AHT20_Status()) & AHT20_STATUS_BUSY)
	{
		osDelay(10); //Delay for 10 milliseconds
	}

	if(status == AHT20_STATUS_ERROR)
	{
		return false;
	}

	if(!(status & AHT20_STATUS_CALIBRATED))
	{
		return false;
	}

	return true;
}

HAL_StatusTypeDef Is_AHT20_Ready()
{
	return(HAL_I2C_IsDeviceReady(AHT20_I2C_BUS_HANDLE, AHT20_ADDR, 1, AHT20_TIMEOUT));
}

HAL_StatusTypeDef Send_AHT20_Data(uint8_t * command_buffer, size_t size)
{
	return(HAL_I2C_Master_Transmit(AHT20_I2C_BUS_HANDLE, AHT20_ADDR, command_buffer, size, AHT20_TIMEOUT));
}

HAL_StatusTypeDef Get_AHT20_Data(uint8_t * data_buffer, size_t size)
{
	return(HAL_I2C_Master_Receive(AHT20_I2C_BUS_HANDLE, AHT20_ADDR, data_buffer, size, AHT20_TIMEOUT));
}

uint8_t Get_AHT20_Status(void)
{
	uint8_t status = 0;
	HAL_StatusTypeDef ret;
	uint8_t buffer[1];

	buffer[0] = AHT20_STATUS_CMD;
	ret = Send_AHT20_Data(buffer, 1);
	if(ret != HAL_OK)
	{
		status = (uint8_t)AHT20_STATUS_ERROR;
	}
	else
	{
		ret = Get_AHT20_Data(buffer, 1);
		if(ret != HAL_OK)
		{
			status = (uint8_t)AHT20_STATUS_ERROR;
		}
		else
		{
			status = buffer[0];
		}
	}

	return(status);
}

