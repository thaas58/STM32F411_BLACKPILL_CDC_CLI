/*
 * SPI_EEPROM.c
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
#include "main.h"
#include "cmsis_os.h"
#include <spi_eeprom.h>

uint8_t EEPROM_StatusByte;
uint8_t RxBuffer[EEPROM_BUFFER_SIZE] = {0x00};
static SPI_HandleTypeDef * EEPROM_SPI;
static GPIO_TypeDef * EEPROM_SPI_SS_GPIO_Port;
static uint16_t EEPROM_SPI_CS_PIN;
/**
 * @brief Init EEPROM SPI
 *
 * @param hspi Pointer to SPI struct handler
 */
void EEPROM_SPI_INIT(SPI_HandleTypeDef * hspi, GPIO_TypeDef * gpio_port, uint16_t cs_pin)
{
    EEPROM_SPI = hspi;
    EEPROM_SPI_SS_GPIO_Port = gpio_port;
    EEPROM_SPI_CS_PIN = cs_pin;
    EEPROM_SPI_CS_HIGH();
}

/**
  * @brief  Enables the write access to the EEPROM.
  *
  * @param  None
  * @retval None
  */
void SPI_WriteEnable(void)
{
    // Select the EEPROM: Chip Select low
    EEPROM_SPI_CS_LOW();

    uint8_t command[1] = { EEPROM_WREN };
    /* Send "Write Enable" instruction */
    EEPROM_SPI_SendInstruction((uint8_t*)command, 1);

    // Deselect the EEPROM: Chip Select high
    EEPROM_SPI_CS_HIGH();
}

/**
  * @brief  Disables the write access to the EEPROM.
  *
  * @param  None
  * @retval None
  */
void SPI_WriteDisable(void)
{
    // Select the EEPROM: Chip Select low
    EEPROM_SPI_CS_LOW();

    uint8_t command[1] = { EEPROM_WRDI };

    /* Send "Write Disable" instruction */
    EEPROM_SPI_SendInstruction((uint8_t*)command, 1);

    // Deselect the EEPROM: Chip Select high
    EEPROM_SPI_CS_HIGH();
}

/**
  * @brief  Polls the status of the Write In Progress (WIP) flag in the EEPROM's
  *         status register and loop until write operation has completed.
  *
  * @param  None
  * @retval None
  */
uint8_t EEPROM_SPI_WaitStandbyState(void) {
    uint8_t eeprom_status_reg[1] = { 0x00 };
    uint8_t command[1] = { EEPROM_RDSR };

    // Select the EEPROM: Chip Select low
    EEPROM_SPI_CS_LOW();

    // Send "Read Status Register" instruction
    EEPROM_SPI_SendInstruction((uint8_t*)command, 1);

    // Loop as long as the memory is busy with a write cycle
    do {

        while (HAL_SPI_Receive(EEPROM_SPI, (uint8_t*)eeprom_status_reg, 1, 200) == HAL_BUSY) {
            osDelay(1);
        };

        osDelay(1);

    } while ((eeprom_status_reg[0] & EEPROM_WIP_FLAG) == SET); // Write in progress

    // Deselect the EEPROM: Chip Select high
    EEPROM_SPI_CS_HIGH();

    return 0;
}

/**
 * @brief Low level function to send header data to EEPROM
 *
 * @param instruction array of bytes to send
 * @param size        data size in bytes
 */
void EEPROM_SPI_SendInstruction(uint8_t *instruction, uint8_t size) {
    while (EEPROM_SPI->State == HAL_SPI_STATE_RESET) {
    	HAL_Delay(1000);
    }

    if (HAL_SPI_Transmit(EEPROM_SPI, (uint8_t*)instruction, (uint16_t)size, 200) != HAL_OK) {
        Error_Handler();
    }
}

/**
  * @brief  Writes more than one byte to the EEPROM with a single WRITE cycle
  *         (Page WRITE sequence).
  *
  * @note   The number of byte can't exceed the EEPROM page size.
  * @param  pBuffer: pointer to the buffer  containing the data to be written
  *         to the EEPROM.
  * @param  WriteAddr: EEPROM's internal address to write to.
  * @param  NumByteToWrite: number of bytes to write to the EEPROM, must be equal
  *         or less than "EEPROM_PAGESIZE" value.
  * @retval EepromOperations value: EEPROM_STATUS_COMPLETE or EEPROM_STATUS_ERROR
  */
EEPROMStatus EEPROM_SPI_WritePage(uint8_t* pBuffer, uint16_t WriteAddr, uint16_t NumByteToWrite) {
    while (EEPROM_SPI->State != HAL_SPI_STATE_READY) {
        osDelay(1);
    }

    HAL_StatusTypeDef spiTransmitStatus;

    SPI_WriteEnable();

    /*
        We're going to send commands in one packet of 3 bytes
     */
    uint8_t header[3];

    header[0] = EEPROM_WRITE;   // Send "Write to Memory" instruction
    header[1] = WriteAddr >> 8; // Send 16-bit address
    header[2] = WriteAddr;

    // Select the EEPROM: Chip Select low
    EEPROM_SPI_CS_LOW();

    EEPROM_SPI_SendInstruction((uint8_t*)header, 3);

    // Make 5 attemtps to write the data
    for (uint8_t i = 0; i < 5; i++) {
        spiTransmitStatus = HAL_SPI_Transmit(EEPROM_SPI, pBuffer, NumByteToWrite, 100);

        if (spiTransmitStatus == HAL_BUSY)
        {
            osDelay(5);
        }
        else
        {
        	break;
        }
    }

    // Deselect the EEPROM: Chip Select high
    EEPROM_SPI_CS_HIGH();

    // Wait the end of EEPROM writing
    EEPROM_SPI_WaitStandbyState();

    // Disable the write access to the EEPROM
    SPI_WriteDisable();

    if (spiTransmitStatus == HAL_ERROR) {
        return EEPROM_STATUS_ERROR;
    } else {
        return EEPROM_STATUS_COMPLETE;
    }
}
/**
  * @brief  Writes block of data to the EEPROM. In this function, the number of
  *         WRITE cycles are reduced, using Page WRITE sequence.
  *
  * @param  pBuffer: pointer to the buffer  containing the data to be written
  *         to the EEPROM.
  * @param  WriteAddr: EEPROM's internal address to write to.
  * @param  NumByteToWrite: number of bytes to write to the EEPROM.
  * @retval EepromOperations value: EEPROM_STATUS_COMPLETE or EEPROM_STATUS_ERROR
  */
EEPROMStatus EEPROM_SPI_WriteBuffer(uint8_t* pBuffer, uint16_t WriteAddr, uint16_t NumByteToWrite) {
    uint16_t NumOfPage = 0, NumOfSingle = 0, Addr = 0, count = 0, temp = 0;
    uint16_t sEE_DataNum = 0;

    EEPROMStatus pageWriteStatus = EEPROM_STATUS_PENDING;

    Addr = WriteAddr % EEPROM_PAGESIZE;
    count = EEPROM_PAGESIZE - Addr;
    NumOfPage =  NumByteToWrite / EEPROM_PAGESIZE;
    NumOfSingle = NumByteToWrite % EEPROM_PAGESIZE;

    if (Addr == 0) { /* WriteAddr is EEPROM_PAGESIZE aligned  */
        if (NumOfPage == 0) { /* NumByteToWrite < EEPROM_PAGESIZE */
            sEE_DataNum = NumByteToWrite;
            pageWriteStatus = EEPROM_SPI_WritePage(pBuffer, WriteAddr, sEE_DataNum);

            if (pageWriteStatus != EEPROM_STATUS_COMPLETE) {
                return pageWriteStatus;
            }

        } else { /* NumByteToWrite > EEPROM_PAGESIZE */
            while (NumOfPage--) {
                sEE_DataNum = EEPROM_PAGESIZE;
                pageWriteStatus = EEPROM_SPI_WritePage(pBuffer, WriteAddr, sEE_DataNum);

                if (pageWriteStatus != EEPROM_STATUS_COMPLETE) {
                    return pageWriteStatus;
                }

                WriteAddr +=  EEPROM_PAGESIZE;
                pBuffer += EEPROM_PAGESIZE;
            }

            if(NumOfSingle > 0)
            {
                sEE_DataNum = NumOfSingle;
                pageWriteStatus = EEPROM_SPI_WritePage(pBuffer, WriteAddr, sEE_DataNum);

                if (pageWriteStatus != EEPROM_STATUS_COMPLETE) {
                    return pageWriteStatus;
                }
            }
        }
    } else { /* WriteAddr is not EEPROM_PAGESIZE aligned  */
        if (NumOfPage == 0) { /* NumByteToWrite < EEPROM_PAGESIZE */
            if (NumOfSingle > count) { /* (NumByteToWrite + WriteAddr) > EEPROM_PAGESIZE */
                temp = NumOfSingle - count;
                sEE_DataNum = count;
                pageWriteStatus = EEPROM_SPI_WritePage(pBuffer, WriteAddr, sEE_DataNum);

                if (pageWriteStatus != EEPROM_STATUS_COMPLETE) {
                    return pageWriteStatus;
                }

                WriteAddr +=  count;
                pBuffer += count;

                sEE_DataNum = temp;
                pageWriteStatus = EEPROM_SPI_WritePage(pBuffer, WriteAddr, sEE_DataNum);
            } else if (NumByteToWrite > 0){
                sEE_DataNum = NumByteToWrite;
                pageWriteStatus = EEPROM_SPI_WritePage(pBuffer, WriteAddr, sEE_DataNum);
            }
            if (pageWriteStatus != EEPROM_STATUS_COMPLETE) {
            	return pageWriteStatus;
            }
        } else { /* NumByteToWrite > EEPROM_PAGESIZE */
            NumByteToWrite -= count;
            NumOfPage =  NumByteToWrite / EEPROM_PAGESIZE;
            NumOfSingle = NumByteToWrite % EEPROM_PAGESIZE;

            sEE_DataNum = count;

            pageWriteStatus = EEPROM_SPI_WritePage(pBuffer, WriteAddr, sEE_DataNum);

            if (pageWriteStatus != EEPROM_STATUS_COMPLETE) {
                return pageWriteStatus;
            }

            WriteAddr +=  count;
            pBuffer += count;

            while (NumOfPage--) {
                sEE_DataNum = EEPROM_PAGESIZE;

                pageWriteStatus = EEPROM_SPI_WritePage(pBuffer, WriteAddr, sEE_DataNum);

                if (pageWriteStatus != EEPROM_STATUS_COMPLETE) {
                    return pageWriteStatus;
                }

                WriteAddr +=  EEPROM_PAGESIZE;
                pBuffer += EEPROM_PAGESIZE;
            }

            if (NumOfSingle > 0) {
                sEE_DataNum = NumOfSingle;

                pageWriteStatus = EEPROM_SPI_WritePage(pBuffer, WriteAddr, sEE_DataNum);

                if (pageWriteStatus != EEPROM_STATUS_COMPLETE) {
                    return pageWriteStatus;
                }
            }
        }
    }

    return EEPROM_STATUS_COMPLETE;
}

/**
  * @brief  Reads a block of data from the EEPROM.
  *
  * @param  pBuffer: pointer to the buffer that receives the data read from the EEPROM.
  * @param  ReadAddr: EEPROM's internal address to read from.
  * @param  NumByteToRead: number of bytes to read from the EEPROM.
  * @retval None
  */
EEPROMStatus EEPROM_SPI_ReadBuffer(uint8_t* pBuffer, uint16_t ReadAddr, uint16_t NumByteToRead) {
    while (EEPROM_SPI->State != HAL_SPI_STATE_READY) {
        osDelay(1);
    }

    /*
        We;re going to send all commands in one packet of 3 bytes
     */

    uint8_t header[3];

    header[0] = EEPROM_READ;    // Send "Read from Memory" instruction
    header[1] = ReadAddr >> 8;  // Send 16-bit address
    header[2] = ReadAddr;

    // Select the EEPROM: Chip Select low
    EEPROM_SPI_CS_LOW();

    /* Send WriteAddr address byte to read from */
    EEPROM_SPI_SendInstruction(header, 3);

    while (HAL_SPI_Receive(EEPROM_SPI, (uint8_t*)pBuffer, NumByteToRead, 200) == HAL_BUSY) {
        osDelay(1);
    };

    // Deselect the EEPROM: Chip Select high
    EEPROM_SPI_CS_HIGH();

    return EEPROM_STATUS_COMPLETE;
}

/**
  * @brief  Sends a byte through the SPI interface and return the byte received
  *         from the SPI bus.
  *
  * @param  byte: byte to send.
  * @retval The value of the received byte.
  */
uint8_t EEPROM_SendByte(uint8_t byte) {
    uint8_t answerByte;

    /* Loop while DR register in not empty */
    while (EEPROM_SPI->State == HAL_SPI_STATE_RESET) {
        osDelay(1);
    }

    /* Send byte through the SPI peripheral */
    if (HAL_SPI_Transmit(EEPROM_SPI, &byte, 1, 200) != HAL_OK) {
        Error_Handler();
    }

    /* Wait to receive a byte */
    while (EEPROM_SPI->State == HAL_SPI_STATE_RESET) {
        osDelay(1);
    }

    /* Return the byte read from the SPI bus */
    if (HAL_SPI_Receive(EEPROM_SPI, &answerByte, 1, 200) != HAL_OK) {
        Error_Handler();
    }

    return (uint8_t)answerByte;
}
