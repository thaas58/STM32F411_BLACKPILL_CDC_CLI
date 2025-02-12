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
#include "semphr.h"

#define cmdMAX_MUTEX_WAIT		pdMS_TO_TICKS( 300 )

static I2C_HandleTypeDef * AHT20_I2C_BUS_HANDLE;
static float temperature_store = 0;
static float humidity_store = 0;
static SemaphoreHandle_t storeMutex = NULL;

static bool Set_Values(float humidity, float temperature)
{
	bool status = false;

	if( xSemaphoreTake( storeMutex, cmdMAX_MUTEX_WAIT ) == pdPASS )
	{
		humidity_store = humidity;
		temperature_store = temperature;
		/* Must ensure to give the mutex back. */
		xSemaphoreGive( storeMutex );
		status = true;
	}
	return(status);
}

bool Get_Values(float* humidity, float* temperature)
{
	bool status = false;

	if( xSemaphoreTake( storeMutex, cmdMAX_MUTEX_WAIT ) == pdPASS )
	{
		*humidity = humidity_store;
		*temperature = temperature_store;
		/* Must ensure to give the mutex back. */
		xSemaphoreGive( storeMutex );
		status = true;
	}
	return(status);
}

bool AHT20_I2C_INIT(I2C_HandleTypeDef * hi2c)
{
	HAL_StatusTypeDef hal_status = HAL_ERROR;
	uint8_t command[3];
	uint8_t status;

	/* Create the semaphore used to access stored humidity and temperature values. */
	storeMutex = xSemaphoreCreateMutex();
	configASSERT( storeMutex );

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

bool Get_AHT20_Values(float* humidity, float* temperature)
{
	float _humidity = 0;
	float _temperature = 0;
	uint8_t command[3];
	uint8_t data[6];

	command[0] = AHT20_CMD_TRIGGER;
	command[1] = 0x33;
	command[2] = 0x00;
	if(Send_AHT20_Data(command, 3) != HAL_OK)
	{
		return false;
	}

	// AHT20 datasheet says to wait at least 80 milliseconds for measurement.
	osDelay(80); //Delay for 80 milliseconds
	while(Get_AHT20_Status() & AHT20_STATUS_BUSY)
	{
		osDelay(10); //Delay for 10 milliseconds
	}

	if(Get_AHT20_Data(data, 6) != HAL_OK)
	{
		return false;
	}

	// Extract and calculate humidity percentage
	uint32_t h_data = data[1];
	h_data <<= 8;
	h_data |= data[2];
	h_data <<= 4;
	h_data |= data[3] >> 4;
	_humidity = ((float)h_data * 100) / 0x100000;

	// Extract and calculate temperature
	uint32_t t_data = data[3] & 0x0F;
	t_data <<= 8;
	t_data |= data[4];
	t_data <<= 8;
	t_data |= data[5];
	_temperature = ((float)t_data * 200 / 0x100000) - 50;

	if(humidity != NULL)
	{
		*humidity = _humidity;
	}

	if(temperature != NULL)
	{
		*temperature = _temperature;
	}

	Set_Values(_humidity, _temperature);

	return true;
}
