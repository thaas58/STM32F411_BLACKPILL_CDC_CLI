/*
 * dispatcher.c
 *
 *  Created on: Jan 4, 2021
 *      Author: Terry Haas
 */
//
// Modified by PickleRix, alien firmware engineer 02/22/2022
//
#include "dispatcher.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

uint8_t buffer_data[64];
uint8_t buffer_index;
uint32_t buffer_length;
bool data_buffer_ready = false;

void DispatcherTask(void *argument);

//QueueHandle_t xQueue;

osThreadId_t dispatcherTaskHandle;
const osThreadAttr_t dispatcherTask_attributes = {
  .name = "dispatcherTask",
  .priority = (osPriority_t) osPriorityNormal,
  .stack_size = 128 * 4
};

/**
  * @brief  Function implementing the DispatcherTask thread.
  * @param  argument: Not used
  * @retval None
  */
void DispatcherTask(void *argument)
{
  Q_Buffer rx_buffer;
	// Infinite loop

  for(;;)
  {
	if(data_buffer_ready == false)
	{
		xQueueReceive(xQueue, &rx_buffer, portMAX_DELAY);
		memcpy(&buffer_data, rx_buffer.data.uint8Data, rx_buffer.length);
		buffer_length = rx_buffer.length;
		buffer_index = 0;
		if(buffer_length > 0)
		{
			data_buffer_ready = true;
		}
	}
	else
	{
		taskYIELD();
	}
  }
}

bool CDC_Receive(uint8_t *pData)
{
	bool status = false;
	if(data_buffer_ready)
	{
		*pData = buffer_data[buffer_index];
		status = true;
		buffer_index++;
		if(buffer_index >= buffer_length)
		{
			data_buffer_ready = false;
		}
	}
	return status;
}

void DispatcherThreadInit()
{
	xQueue = xQueueCreate(2, sizeof(Q_Buffer));
	if(xQueue)
	{
		dispatcherTaskHandle = osThreadNew(DispatcherTask, NULL, &dispatcherTask_attributes);
	}
	else
	{
		//Failed to create the queue
	}
}
