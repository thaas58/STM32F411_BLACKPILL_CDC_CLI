/*
 * SPI_EEPROM.h
 *
 *  Created on: Oct 1, 2021
 *      Author: Terry Haas
 *
 *  This code is based off of this GitHub project:
 *  	https://github.com/firebull/STM32-EEPROM-SPI
 */
//
// Modified by PickleRix, alien firmware engineer 02/26/2022
//
#ifndef SPI_EEPROM_H_
#define SPI_EEPROM_H_
/* C++ detection */
#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "cmsis_os.h"

/* M95256 SPI EEPROM defines */
#define EEPROM_WRSR  0x01  /*!< Write Status Register */
#define EEPROM_WRITE 0x02  /*!< Write to Memory Array */
#define EEPROM_READ  0x03  /*!< Read from Memory Array */
#define EEPROM_WRDI  0x04  /*!< Write Disable */
#define EEPROM_RDSR  0x05  /*!< Read Status Register */
#define EEPROM_WREN  0x06  /*!< Write Enable */

#define EEPROM_WIP_FLAG        0x01  /*!< Write In Progress (WIP) flag */

#define EEPROM_PAGESIZE        64    /*!< Pagesize according to M95256 documentation */
#define EEPROM_BUFFER_SIZE     64    /*!< EEPROM Buffer size. Setup to your needs */

#define EEPROM_SPI_CS_HIGH()    HAL_GPIO_WritePin(EEPROM_SPI_SS_GPIO_Port, EEPROM_SPI_CS_PIN, GPIO_PIN_SET)
#define EEPROM_SPI_CS_LOW()     HAL_GPIO_WritePin(EEPROM_SPI_SS_GPIO_Port, EEPROM_SPI_CS_PIN, GPIO_PIN_RESET)
/**
 * EEPROM status enum values
 */
typedef enum {
    EEPROM_STATUS_PENDING,
    EEPROM_STATUS_COMPLETE,
    EEPROM_STATUS_ERROR
} EEPROMStatus;

void EEPROM_SPI_INIT(SPI_HandleTypeDef * hspi, GPIO_TypeDef * gpio_port, uint16_t cs_pin);
void SPI_WriteEnable(void);
void SPI_WriteDisable(void);
uint8_t EEPROM_SPI_WaitStandbyState(void);
void EEPROM_SPI_SendInstruction(uint8_t *instruction, uint8_t size);
EEPROMStatus EEPROM_SPI_WritePage(uint8_t* pBuffer, uint16_t WriteAddr, uint16_t NumByteToWrite);
EEPROMStatus EEPROM_SPI_WriteBuffer(uint8_t* pBuffer, uint16_t WriteAddr, uint16_t NumByteToWrite);
EEPROMStatus EEPROM_SPI_ReadBuffer(uint8_t* pBuffer, uint16_t ReadAddr, uint16_t NumByteToRead);

#ifdef __cplusplus
}
#endif

#endif /* SPI_EEPROM_H_ */
