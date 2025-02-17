/*
 * FreeRTOS V202112.00
 * Copyright (C) 2017 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * http://www.FreeRTOS.org
 * http://aws.amazon.com/freertos
 *
 * 1 tab == 4 spaces!
 */


 /******************************************************************************
 *
 * http://www.FreeRTOS.org/cli
 *
 ******************************************************************************/
//
// Modified by PickleRix, alien firmware engineer 02/28/2022
//


/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"

/* Standard includes. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "spi_eeprom.h"
/* FreeRTOS+CLI includes. */
#include "FreeRTOS_CLI.h"
#include "stdbool.h"
#include "aht20.h"

#ifndef  configINCLUDE_TRACE_RELATED_CLI_COMMANDS
	#define configINCLUDE_TRACE_RELATED_CLI_COMMANDS 0
#endif

#ifndef configINCLUDE_QUERY_HEAP_COMMAND
	#define configINCLUDE_QUERY_HEAP_COMMAND 0
#endif

#define MAX_SPI_BUFFER_SIZE 128
#define MAX_SPI_WRITES 		64

extern SPI_HandleTypeDef hspi1;
/*
 * The function that registers the commands that are defined within this file.
 */
//void vRegisterCLICommands( void );

/*
 * Implements the spi command.
 */
static BaseType_t prvSPICommand( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString );
/*
 * Implements the get command.
 */
static BaseType_t prvGetCommand( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString );
/*
 * Implements the task-stats command.
 */

static BaseType_t prvTaskStatsCommand( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString );

/*
 * Implements the run-time-stats command.
 */
#if( configGENERATE_RUN_TIME_STATS == 1 )
	static BaseType_t prvRunTimeStatsCommand( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString );
#endif /* configGENERATE_RUN_TIME_STATS */

/*
 * Implements the echo-three-parameters command.
 */
static BaseType_t prvThreeParameterEchoCommand( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString );

/*
 * Implements the echo-parameters command.
 */
static BaseType_t prvParameterEchoCommand( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString );

/*
 * Implements the "query heap" command.
 */
#if( configINCLUDE_QUERY_HEAP_COMMAND == 1 )
	static BaseType_t prvQueryHeapCommand( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString );
#endif

/*
 * Implements the "trace start" and "trace stop" commands;
 */
#if( configINCLUDE_TRACE_RELATED_CLI_COMMANDS == 1 )
	static BaseType_t prvStartStopTraceCommand( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString );
#endif

/*
* Structure that defines the "spi" command line command.  This
* writes or reads SPI data depending on the parameters passed.
*/
static const CLI_Command_Definition_t xSPI =
{
	"spi", /* The command string to type. */
	"\r\nspi <...>:\r\n Writes/reads SPI data to/from SPI EEPROM\r\n  Example: spi -wr <offset> <data_byte(s)> \r\n  Example: spi -rd <offset> <num_bytes>\r\n  Example: spi -fill <offset> <num_bytes> <data_byte>",
	prvSPICommand, /* The function to run. */
	-1 /* The user can enter any number of commands. */
};

/*
 * Structure that defines the "get" command line command.  This
 * returns data depending on the parameter passed.
 */
static const CLI_Command_Definition_t xGet =
{
	"get", /* The command string to type. */
	"\r\nget <...>:\r\n Displays data for the specified item(s)\r\n Possible parameters: cpuid, flash_size, humidity, temperature",
	prvGetCommand, /* The function to run. */
	-1 /* The user can enter any number of commands. */
};

/* Structure that defines the "task-stats" command line command.  This generates
a table that gives information on each task in the system. */
static const CLI_Command_Definition_t xTaskStats =
{
	"task-stats", /* The command string to type. */
	"\r\ntask-stats:\r\n Displays a table showing the state of each FreeRTOS task",
	prvTaskStatsCommand, /* The function to run. */
	0 /* No parameters are expected. */
};

/* Structure that defines the "echo_3_parameters" command line command.  This
takes exactly three parameters that the command simply echos back one at a
time. */
static const CLI_Command_Definition_t xThreeParameterEcho =
{
	"echo-3-parameters",
	"\r\necho-3-parameters <param1> <param2> <param3>:\r\n Expects three parameters, echos each in turn",
	prvThreeParameterEchoCommand, /* The function to run. */
	3 /* Three parameters are expected, which can take any value. */
};

/* Structure that defines the "echo_parameters" command line command.  This
takes a variable number of parameters that the command simply echos back one at
a time. */
static const CLI_Command_Definition_t xParameterEcho =
{
	"echo-parameters",
	"\r\necho-parameters <...>:\r\n Take variable number of parameters, echos each in turn",
	prvParameterEchoCommand, /* The function to run. */
	-1 /* The user can enter any number of commands. */
};

#if( configGENERATE_RUN_TIME_STATS == 1 )
	/* Structure that defines the "run-time-stats" command line command.   This
	generates a table that shows how much run time each task has */
	static const CLI_Command_Definition_t xRunTimeStats =
	{
		"run-time-stats", /* The command string to type. */
		"\r\nrun-time-stats:\r\n Displays a table showing how much processing time each FreeRTOS task has used\r\n",
		prvRunTimeStatsCommand, /* The function to run. */
		0 /* No parameters are expected. */
	};
#endif /* configGENERATE_RUN_TIME_STATS */

#if( configINCLUDE_QUERY_HEAP_COMMAND == 1 )
	/* Structure that defines the "query_heap" command line command. */
	static const CLI_Command_Definition_t xQueryHeap =
	{
		"query-heap",
		"\r\nquery-heap:\r\n Displays the free heap space, and minimum ever free heap space.\r\n",
		prvQueryHeapCommand, /* The function to run. */
		0 /* The user can enter any number of commands. */
	};
#endif /* configQUERY_HEAP_COMMAND */

#if configINCLUDE_TRACE_RELATED_CLI_COMMANDS == 1
	/* Structure that defines the "trace" command line command.  This takes a single
	parameter, which can be either "start" or "stop". */
	static const CLI_Command_Definition_t xStartStopTrace =
	{
		"trace",
		"\r\ntrace [start | stop]:\r\n Starts or stops a trace recording for viewing in FreeRTOS+Trace\r\n",
		prvStartStopTraceCommand, /* The function to run. */
		1 /* One parameter is expected.  Valid values are "start" and "stop". */
	};
#endif /* configINCLUDE_TRACE_RELATED_CLI_COMMANDS */

/*-----------------------------------------------------------*/

void vRegisterCLICommands( void )
{
	/* Register all the command line commands defined immediately above. */
	FreeRTOS_CLIRegisterCommand( &xSPI );
	FreeRTOS_CLIRegisterCommand( &xGet );
	FreeRTOS_CLIRegisterCommand( &xTaskStats );	
	FreeRTOS_CLIRegisterCommand( &xThreeParameterEcho );
	FreeRTOS_CLIRegisterCommand( &xParameterEcho );

	#if( configGENERATE_RUN_TIME_STATS == 1 )
	{
		FreeRTOS_CLIRegisterCommand( &xRunTimeStats );
	}
	#endif
	
	#if( configINCLUDE_QUERY_HEAP_COMMAND == 1 )
	{
		FreeRTOS_CLIRegisterCommand( &xQueryHeap );
	}
	#endif

	#if( configINCLUDE_TRACE_RELATED_CLI_COMMANDS == 1 )
	{
		FreeRTOS_CLIRegisterCommand( &xStartStopTrace );
	}
	#endif
}

/*-----------------------------------------------------------*/

uint8_t SPI_Buffer[MAX_SPI_BUFFER_SIZE];

static BaseType_t prvSPICommand( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString )
{
	/* Data for send and receive */
	const char *pcParameter;
	char param_buffer[64];
	BaseType_t xParameterStringLength, xReturn;
	static UBaseType_t uxParameterNumber = 0;
	static bool write_cycle = false;
	static bool read_cycle = false;
	static bool fill_cycle = false;
	static unsigned long offset = 0;
	static uint16_t offset16 = 0;
	static unsigned long num_reads = 0;
	static uint8_t num_reads8 = 0;
	static unsigned long num_writes = 0;
	static uint16_t spi_index = 0;
	/* Remove compile time warnings about unused parameters, and check the
	write buffer is not NULL.  NOTE - for simplicity, this example assumes the
	write buffer length is adequate, so does not check for buffer overflows. */
	( void ) pcCommandString;
	( void ) xWriteBufferLen;
	configASSERT( pcWriteBuffer );

	if( uxParameterNumber == 0 )
	{
		/* The first time the function is called after the command has been
		entered just a header string is returned. */
		sprintf( pcWriteBuffer, "\r\nSPI output:" );

		write_cycle = false;
		read_cycle = false;
		offset = 0;
		num_reads = 0;
		num_writes = 0;
		num_reads8 = 0;
		spi_index = 0;

		/* Next time the function is called the first parameter will be echoed
		back. */
		uxParameterNumber = 1U;

		/* There is more data to be returned as no parameters have been echoed
		back yet. */
		xReturn = pdPASS;
	}
	else
	{
		/* Obtain the parameter string. */
		pcParameter = FreeRTOS_CLIGetParameter
					(
						pcCommandString,		/* The command string itself. */
						uxParameterNumber,		/* Return the next parameter. */
						&xParameterStringLength	/* Store the parameter string length. */
					);

		if( pcParameter != NULL )
		{
			xReturn = pdTRUE;
			memset( pcWriteBuffer, 0x00, xWriteBufferLen );
			memset( param_buffer, 0x00, sizeof(param_buffer));
			strncpy(param_buffer,pcParameter,xParameterStringLength);
			if(uxParameterNumber == 1)
			{
				if(!stricmp("-wr", param_buffer) || !stricmp("-w", param_buffer))
				{
					write_cycle = true;
				}
				else if(!stricmp("-rd", param_buffer) || !stricmp("-r", param_buffer))
				{
					read_cycle = true;
				}
				else if(!stricmp("-fill", param_buffer) || !stricmp("-f", param_buffer))
				{
					fill_cycle = true;
				}
				else
				{
					sprintf(pcWriteBuffer,"\r\nParameter not supported: %s", param_buffer);
					xReturn = pdFALSE;
				}
			}
			else if(uxParameterNumber == 2)
			{
				if(write_cycle || read_cycle || fill_cycle)
				{
					offset = strtoul(param_buffer, NULL,0);

					if(offset > 0xFFFF)
					{
						sprintf(pcWriteBuffer,"\r\nOffset parameter should not be greater than 16-bits: %s", param_buffer);
						xReturn = pdFALSE;
					}
					else
					{
						offset16 = (uint16_t)(offset & 0xFFFF);
						sprintf(pcWriteBuffer,"\r\noffset parameter: %d", offset16);
					}
				}
				else
				{
					sprintf(pcWriteBuffer,"\r\nParameter not supported: %s", param_buffer);
					xReturn = pdFALSE;
				}
			}
			else if(read_cycle && uxParameterNumber == 3)
			{
				if(read_cycle)
				{
					if(num_reads == 0)
					{
						num_reads = strtoul(param_buffer, NULL, 0);
						if(num_reads > MAX_SPI_BUFFER_SIZE)
						{
							sprintf(pcWriteBuffer,"\r\nNumber of reads should not be greater than %d : %s", MAX_SPI_BUFFER_SIZE, param_buffer);
							xReturn = pdFALSE;
						}
						else if(num_reads == 0)
						{
							sprintf(pcWriteBuffer,"\r\nNumber of reads should be greater than %s", param_buffer);
							xReturn = pdFALSE;
						}
						else
						{
							num_reads8 = (uint8_t)(num_reads & 0xFF);
							sprintf(pcWriteBuffer,"\r\nnum_reads parameter: %d", num_reads8);
							if(EEPROM_STATUS_COMPLETE == EEPROM_SPI_ReadBuffer((uint8_t *)SPI_Buffer, offset16, (uint16_t)num_reads))
							{
								strncat(pcWriteBuffer,"\r\n SPI read SUCCESS", sizeof("\r\n SPI read SUCCESS")+1);
							}
							else
							{
								strncat(pcWriteBuffer,"\r\n SPI read FAILED", sizeof("\r\n SPI read FAILED")+1);
							}
						}
					}
					else
					{

					}
				}
			}
			else if(write_cycle && uxParameterNumber >= 3)
			{
				if(spi_index < MAX_SPI_WRITES)
				{
					unsigned long data = strtoul(param_buffer, NULL, 0);
					if(data > 0xFF)
					{
						sprintf(pcWriteBuffer,"\r\n Data byte should not be greater than 255: %s", param_buffer);
						xReturn = pdFALSE;
					}
					else
					{
						SPI_Buffer[spi_index++] = (uint8_t)(data & 0xFF);
					}
				}
				else
				{
					sprintf(pcWriteBuffer,"\r\nNumber of write bytes should not be greater than %d : %s", MAX_SPI_WRITES, param_buffer);
					xReturn = pdFALSE;
				}
			}
			else if(fill_cycle && uxParameterNumber == 3)
			{
				num_writes = strtoul(param_buffer, NULL, 0);
				if(num_writes > MAX_SPI_BUFFER_SIZE)
				{
					sprintf(pcWriteBuffer,"\r\n Number of fill bytes should not be greater than %d", MAX_SPI_BUFFER_SIZE);
					xReturn = pdFALSE;
				}
			}
			else if(fill_cycle && uxParameterNumber == 4)
			{
				unsigned long data = strtoul(param_buffer, NULL, 0);
				if(data > 0xFF)
				{
					sprintf(pcWriteBuffer,"\r\n Data byte should not be greater than 255: %s", param_buffer);
					xReturn = pdFALSE;
				}
				else if(data > 0)
				{
					for(int i = 0; i < num_writes; i++)
					{
						SPI_Buffer[i] = (uint8_t)(data & 0xFF);
					}

					if(EEPROM_STATUS_COMPLETE == EEPROM_SPI_WriteBuffer((uint8_t *)SPI_Buffer, offset16, (uint16_t)num_writes))
					{
						sprintf(pcWriteBuffer,"\r\nSPI FILL SUCCESS\r\nBytes written: %ld", num_writes);
					}
					else
					{
						strncat(pcWriteBuffer, "\r\nSPI FILL FAILED", sizeof("\r\nSPI FILL FAILED")+1);
					}
					fill_cycle = false;
				}
			}

			if(xReturn == pdTRUE)
			{
				/* There might be more parameters to return after this one. */
				uxParameterNumber++;
			}
			else
			{
				/* No more parameters were found.  Make sure the write buffer does
				not contain a valid string. */
				//pcWriteBuffer[ 0 ] = 0x00;
				uxParameterNumber = 0;
			}
		}
		else
		{
			xReturn = pdTRUE;
			memset( pcWriteBuffer, 0x00, xWriteBufferLen );
			if(read_cycle && spi_index < num_reads8)
			{
				char buffer[6];

				strncat(pcWriteBuffer,"\r\n ", sizeof("\r\n ")+1);
				for(int i = 0; i < 16; i++)
				{
					snprintf(buffer,6,"%02X ",SPI_Buffer[spi_index]);
					strncat(pcWriteBuffer, buffer, sizeof(buffer)+1);
					spi_index++;
					if(spi_index >= num_reads8)
					{
						read_cycle = false;
						break;
					}
				}
			}
			else if(write_cycle && spi_index > 0)
			{
				if(EEPROM_STATUS_COMPLETE == EEPROM_SPI_WritePage((uint8_t *)SPI_Buffer, offset16, spi_index))
				{
					sprintf(pcWriteBuffer,"\r\nSPI write SUCCESS\r\nBytes written: %d", spi_index);
				}
				else
				{
					strncat(pcWriteBuffer, "\r\nSPI write FAILED", sizeof("\r\nSPI write FAILED")+1);
				}
				write_cycle = false;
			}
			else
			{
				/* No more parameters were found.  Make sure the write buffer does
				not contain a valid string. */
				pcWriteBuffer[ 0 ] = 0x00;

				/* No more data to return. */
				xReturn = pdFALSE;

				/* Start over the next time this command is executed. */
				uxParameterNumber = 0;
			}

		}
	}
	return xReturn;
}
/*-----------------------------------------------------------*/

static BaseType_t prvGetCommand( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString )
{
	const char *pcParameter;
	char param_buffer[64];
	BaseType_t xParameterStringLength, xReturn;
	static UBaseType_t uxParameterNumber = 0;
	/* Remove compile time warnings about unused parameters, and check the
	write buffer is not NULL.  NOTE - for simplicity, this example assumes the
	write buffer length is adequate, so does not check for buffer overflows. */
	( void ) pcCommandString;
	( void ) xWriteBufferLen;
	configASSERT( pcWriteBuffer );

	if( uxParameterNumber == 0 )
	{
		/* The first time the function is called after the command has been
		entered just a header string is returned. */
		sprintf( pcWriteBuffer, "\r\nget output:" );

		/* Next time the function is called the first parameter will be echoed
		back. */
		uxParameterNumber = 1U;

		/* There is more data to be returned as no parameters have been echoed
		back yet. */
		xReturn = pdPASS;
	}
	else
	{
		/* Obtain the parameter string. */
		pcParameter = FreeRTOS_CLIGetParameter
					(
						pcCommandString,		/* The command string itself. */
						uxParameterNumber,		/* Return the next parameter. */
						&xParameterStringLength	/* Store the parameter string length. */
					);

		if( pcParameter != NULL )
		{
			memset( pcWriteBuffer, 0x00, xWriteBufferLen );
			memset( param_buffer, 0x00, sizeof(param_buffer));
			strncpy(param_buffer,pcParameter,xParameterStringLength);
			if(!stricmp("cpuid", param_buffer))
			{
				uint32_t cpuid = MMIO32(CPUID);
				sprintf( pcWriteBuffer, "\r\nCPUID: 0x%08lX", cpuid);
			}
			else if(!stricmp("flash_size", param_buffer))
			{
				uint16_t flash_size = MMIO16(FLASH_SZ);
				sprintf( pcWriteBuffer, "\r\nFLASH_SIZE: 0x%04X, %d Kbytes", flash_size, flash_size);
			}
			else if(!stricmp("humidity", param_buffer) || !stricmp("h", param_buffer))
			{
				float humidity = 0;
				float temperature = 0;
				Get_Values(&humidity, &temperature);
				sprintf( pcWriteBuffer, "\r\n AHT20 Relative Humidity: %5.2f%%, Temperature: %5.2f degrees C", humidity, temperature);
			}
			else if(!stricmp("temperature", param_buffer) || !stricmp("t", param_buffer))
			{
				float humidity = 0;
				float temperature = 0;
				Get_Values(&humidity, &temperature);
				sprintf( pcWriteBuffer, "\r\n AHT20 Temperature: %5.2f degrees C, Relative Humidity: %5.2f%%",
						temperature, humidity);
			}
			else
			{
				sprintf(pcWriteBuffer,"\r\nParameter not supported: %s", param_buffer);
			}
			/* There might be more parameters to return after this one. */
			xReturn = pdTRUE;
			uxParameterNumber++;
		}
		else
		{
			/* No more parameters were found.  Make sure the write buffer does
			not contain a valid string. */
			pcWriteBuffer[ 0 ] = 0x00;

			/* No more data to return. */
			xReturn = pdFALSE;

			/* Start over the next time this command is executed. */
			uxParameterNumber = 0;
		}
	}
	return xReturn;
}
/*-----------------------------------------------------------*/

static BaseType_t prvTaskStatsCommand( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString )
{
const char *const pcHeader = " State  Priority  Stack    #\r\n************************************************\r\n";
BaseType_t xSpacePadding;

	/* Remove compile time warnings about unused parameters, and check the
	write buffer is not NULL.  NOTE - for simplicity, this example assumes the
	write buffer length is adequate, so does not check for buffer overflows. */
	( void ) pcCommandString;
	( void ) xWriteBufferLen;
	configASSERT( pcWriteBuffer );

	/* Generate a table of task stats. */
	strcpy( pcWriteBuffer, "\r\nTask" );
	pcWriteBuffer += strlen( pcWriteBuffer );

	/* Minus three for the null terminator and half the number of characters in
	"Task" so the column lines up with the centre of the heading. */
	configASSERT( configMAX_TASK_NAME_LEN > 3 );
	for( xSpacePadding = strlen( "Task" ); xSpacePadding < ( configMAX_TASK_NAME_LEN - 3 ); xSpacePadding++ )
	{
		/* Add a space to align columns after the task's name. */
		*pcWriteBuffer = ' ';
		pcWriteBuffer++;

		/* Ensure always terminated. */
		*pcWriteBuffer = 0x00;
	}
	strncpy( pcWriteBuffer, pcHeader, strlen(pcHeader)+1 );
	vTaskList( pcWriteBuffer + strlen( pcHeader ) );

	/* There is no more data to return after this single string, so return
	pdFALSE. */
	return pdFALSE;
}
/*-----------------------------------------------------------*/

#if( configINCLUDE_QUERY_HEAP_COMMAND == 1 )

	static BaseType_t prvQueryHeapCommand( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString )
	{
		/* Remove compile time warnings about unused parameters, and check the
		write buffer is not NULL.  NOTE - for simplicity, this example assumes the
		write buffer length is adequate, so does not check for buffer overflows. */
		( void ) pcCommandString;
		( void ) xWriteBufferLen;
		configASSERT( pcWriteBuffer );

		sprintf( pcWriteBuffer, "Current free heap %d bytes, minimum ever free heap %d bytes\r\n", ( int ) xPortGetFreeHeapSize(), ( int ) xPortGetMinimumEverFreeHeapSize() );

		/* There is no more data to return after this single string, so return
		pdFALSE. */
		return pdFALSE;
	}

#endif /* configINCLUDE_QUERY_HEAP */
/*-----------------------------------------------------------*/

#if( configGENERATE_RUN_TIME_STATS == 1 )
	
	static BaseType_t prvRunTimeStatsCommand( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString )
	{
	const char * const pcHeader = "  Abs Time      % Time\r\n****************************************\r\n";
	BaseType_t xSpacePadding;

		/* Remove compile time warnings about unused parameters, and check the
		write buffer is not NULL.  NOTE - for simplicity, this example assumes the
		write buffer length is adequate, so does not check for buffer overflows. */
		( void ) pcCommandString;
		( void ) xWriteBufferLen;
		configASSERT( pcWriteBuffer );

		/* Generate a table of task stats. */
		strcpy( pcWriteBuffer, "Task" );
		pcWriteBuffer += strlen( pcWriteBuffer );

		/* Pad the string "task" with however many bytes necessary to make it the
		length of a task name.  Minus three for the null terminator and half the
		number of characters in	"Task" so the column lines up with the centre of
		the heading. */
		for( xSpacePadding = strlen( "Task" ); xSpacePadding < ( configMAX_TASK_NAME_LEN - 3 ); xSpacePadding++ )
		{
			/* Add a space to align columns after the task's name. */
			*pcWriteBuffer = ' ';
			pcWriteBuffer++;

			/* Ensure always terminated. */
			*pcWriteBuffer = 0x00;
		}

		strcpy( pcWriteBuffer, pcHeader );
		vTaskGetRunTimeStats( pcWriteBuffer + strlen( pcHeader ) );

		/* There is no more data to return after this single string, so return
		pdFALSE. */
		return pdFALSE;
	}
	
#endif /* configGENERATE_RUN_TIME_STATS */
/*-----------------------------------------------------------*/

static BaseType_t prvThreeParameterEchoCommand( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString )
{
const char *pcParameter;
BaseType_t xParameterStringLength, xReturn;
static UBaseType_t uxParameterNumber = 0;

	/* Remove compile time warnings about unused parameters, and check the
	write buffer is not NULL.  NOTE - for simplicity, this example assumes the
	write buffer length is adequate, so does not check for buffer overflows. */
	( void ) pcCommandString;
	( void ) xWriteBufferLen;
	configASSERT( pcWriteBuffer );

	if( uxParameterNumber == 0 )
	{
		/* The first time the function is called after the command has been
		entered just a header string is returned. */
		sprintf( pcWriteBuffer, "\r\nThe three parameters were:\r\n" );

		/* Next time the function is called the first parameter will be echoed
		back. */
		uxParameterNumber = 1U;

		/* There is more data to be returned as no parameters have been echoed
		back yet. */
		xReturn = pdPASS;
	}
	else
	{
		/* Obtain the parameter string. */
		pcParameter = FreeRTOS_CLIGetParameter
						(
							pcCommandString,		/* The command string itself. */
							uxParameterNumber,		/* Return the next parameter. */
							&xParameterStringLength	/* Store the parameter string length. */
						);

		/* Sanity check something was returned. */
		configASSERT( pcParameter );

		/* Return the parameter string. */
		memset( pcWriteBuffer, 0x00, xWriteBufferLen );
		sprintf( pcWriteBuffer, "%d: ", ( int ) uxParameterNumber );
		strncat( pcWriteBuffer, pcParameter, ( size_t ) xParameterStringLength );
		strncat( pcWriteBuffer, "\r\n", strlen( "\r\n" ) + 1 );

		/* If this is the last of the three parameters then there are no more
		strings to return after this one. */
		if( uxParameterNumber == 3U )
		{
			/* If this is the last of the three parameters then there are no more
			strings to return after this one. */
			xReturn = pdFALSE;
			uxParameterNumber = 0;
		}
		else
		{
			/* There are more parameters to return after this one. */
			xReturn = pdTRUE;
			uxParameterNumber++;
		}
	}

	return xReturn;
}
/*-----------------------------------------------------------*/

static BaseType_t prvParameterEchoCommand( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString )
{
const char *pcParameter;
BaseType_t xParameterStringLength, xReturn;
static UBaseType_t uxParameterNumber = 0;

	/* Remove compile time warnings about unused parameters, and check the
	write buffer is not NULL.  NOTE - for simplicity, this example assumes the
	write buffer length is adequate, so does not check for buffer overflows. */
	( void ) pcCommandString;
	( void ) xWriteBufferLen;
	configASSERT( pcWriteBuffer );

	if( uxParameterNumber == 0 )
	{
		/* The first time the function is called after the command has been
		entered just a header string is returned. */
		sprintf( pcWriteBuffer, "\r\nThe parameters were:\r\n" );

		/* Next time the function is called the first parameter will be echoed
		back. */
		uxParameterNumber = 1U;

		/* There is more data to be returned as no parameters have been echoed
		back yet. */
		xReturn = pdPASS;
	}
	else
	{
		/* Obtain the parameter string. */
		pcParameter = FreeRTOS_CLIGetParameter
						(
							pcCommandString,		/* The command string itself. */
							uxParameterNumber,		/* Return the next parameter. */
							&xParameterStringLength	/* Store the parameter string length. */
						);

		if( pcParameter != NULL )
		{
			/* Return the parameter string. */
			memset( pcWriteBuffer, 0x00, xWriteBufferLen );
			sprintf( pcWriteBuffer, "%d: ", ( int ) uxParameterNumber );
			strncat( pcWriteBuffer, ( char * ) pcParameter, ( size_t ) xParameterStringLength );
			strncat( pcWriteBuffer, "\r\n", strlen( "\r\n" )+1 );

			/* There might be more parameters to return after this one. */
			xReturn = pdTRUE;
			uxParameterNumber++;
		}
		else
		{
			/* No more parameters were found.  Make sure the write buffer does
			not contain a valid string. */
			pcWriteBuffer[ 0 ] = 0x00;

			/* No more data to return. */
			xReturn = pdFALSE;

			/* Start over the next time this command is executed. */
			uxParameterNumber = 0;
		}
	}

	return xReturn;
}
/*-----------------------------------------------------------*/

#if configINCLUDE_TRACE_RELATED_CLI_COMMANDS == 1

	static BaseType_t prvStartStopTraceCommand( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString )
	{
	const char *pcParameter;
	BaseType_t lParameterStringLength;

		/* Remove compile time warnings about unused parameters, and check the
		write buffer is not NULL.  NOTE - for simplicity, this example assumes the
		write buffer length is adequate, so does not check for buffer overflows. */
		( void ) pcCommandString;
		( void ) xWriteBufferLen;
		configASSERT( pcWriteBuffer );

		/* Obtain the parameter string. */
		pcParameter = FreeRTOS_CLIGetParameter
						(
							pcCommandString,		/* The command string itself. */
							1,						/* Return the first parameter. */
							&lParameterStringLength	/* Store the parameter string length. */
						);

		/* Sanity check something was returned. */
		configASSERT( pcParameter );

		/* There are only two valid parameter values. */
		if( strncmp( pcParameter, "start", strlen( "start" ) ) == 0 )
		{
			/* Start or restart the trace. */
			vTraceStop();
			vTraceClear();
			vTraceStart();

			sprintf( pcWriteBuffer, "Trace recording (re)started.\r\n" );
		}
		else if( strncmp( pcParameter, "stop", strlen( "stop" ) ) == 0 )
		{
			/* End the trace, if one is running. */
			vTraceStop();
			sprintf( pcWriteBuffer, "Stopping trace recording.\r\n" );
		}
		else
		{
			sprintf( pcWriteBuffer, "Valid parameters are 'start' and 'stop'.\r\n" );
		}

		/* There is no more data to return after this single string, so return
		pdFALSE. */
		return pdFALSE;
	}

#endif /* configINCLUDE_TRACE_RELATED_CLI_COMMANDS */
