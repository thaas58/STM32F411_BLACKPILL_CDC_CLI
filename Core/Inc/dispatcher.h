/*
 * dispatcher.h
 *
 *  Created on: Jan 4, 2021
 *      Author: Terry Haas
 */
//
// Modified by PickleRix, alien firmware engineer 02/22/2022
//

#ifndef INC_DISPATCHER_H_
#define INC_DISPATCHER_H_

#ifdef __cplusplus
 extern "C" {
#endif

#include "FreeRTOS.h"
#include "queue.h"
#include "event_groups.h"
#include "usbd_cdc_if.h"

typedef union
{
	char ucData[64];
	uint8_t uint8Data[64];
} Data8;

typedef struct
{
	uint32_t MessageID;
	Data8 data;
	uint32_t length;
} Q_Buffer;



extern QueueHandle_t xQueue;

void DispatcherThreadInit();
bool CDC_Receive(uint8_t *pData);

#ifdef __cplusplus
}
#endif

#endif /* INC_DISPATCHER_H_ */
