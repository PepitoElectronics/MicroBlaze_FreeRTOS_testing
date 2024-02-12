/*
    Copyright (C) 2017 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
    Copyright (c) 2012 - 2020 Xilinx, Inc. All Rights Reserved.
	SPDX-License-Identifier: MIT


    http://www.FreeRTOS.org
    http://aws.amazon.com/freertos


    1 tab == 4 spaces!

    many task running code
 */

#define LCD_RS_MASK 0x40  // RS signal mask
#define LCD_EN_MASK 0x01  // EN signal mask
#define LCD_RW_MASK 0x01  // RW signal mask



#include <stdlib.h>

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"
#include "semphr.h" // Include semaphore/mutex header
/* Xilinx includes. */
#include "xil_printf.h"
#include "xparameters.h"


#include "xgpio.h"
#include "sleep.h"
#include "xil_types.h"


#define TIMER_ID	1
#define DELAY_10_SECONDS	10000UL
#define DELAY_1_SECOND		1000UL
#define TIMER_CHECK_THRESHOLD	9

int my_sprintf_uint8(char *buf, uint8_t value);


static void prvSpeedSelectorTask( void *pvParameters );
static void randomGeneratorTask( void *pvParameters );
static void blinkLEDTask( void *pvParameters );
static void prvTxTask( void *pvParameters );
static void prvRxTask( void *pvParameters );
static void vTimerCallback( TimerHandle_t pxTimer );
/*-----------------------------------------------------------*/

/* The queue used by the Tx and Rx tasks, as described at the top of this file. */

static TaskHandle_t xSpeedSelectorTaskHandler;
static TaskHandle_t randomGeneratorTaskHandler;
static TaskHandle_t blinkLEDTaskHandler;
static TaskHandle_t xTxTask;
static TaskHandle_t xRxTask;
static QueueHandle_t xQueue = NULL;
static QueueHandle_t xQueueRandomGen = NULL;
static QueueHandle_t xQueueBlinkSpeed = NULL;
static TimerHandle_t xTimer = NULL;
char HWstring[15] = "Hello World";
long RxtaskCntr = 0;


SemaphoreHandle_t xMutexAccessGPIO0;
uint8_t data_t[4];


XGpio Gpio;


int main( void )
{
	const TickType_t x10seconds = pdMS_TO_TICKS( DELAY_10_SECONDS );

	xil_printf( "********* FreeRTOS experiment code ***************\r\n" );
	xil_printf( "************************************************************\r\n" );
	xil_printf( "********* This script try to use all the peripherals on the KC705 demo board ***************\r\n" );


	xMutexAccessGPIO0 = xSemaphoreCreateMutex();

	XGpio_Initialize(&Gpio, XPAR_AXI_GPIO_0_DEVICE_ID);

	/* Create the two tasks.  The Tx task is given a lower priority than the
	Rx task, so the Rx task will leave the Blocked state and pre-empt the Tx
	task as soon as the Tx task places an item in the queue. */
	xTaskCreate( prvTxTask, 					/* The function that implements the task. */
			( const char * ) "Tx", 		/* Text name for the task, provided to assist debugging only. */
			configMINIMAL_STACK_SIZE, 	/* The stack allocated to the task. */
			NULL, 						/* The task parameter is not used, so set to NULL. */
			tskIDLE_PRIORITY,			/* The task runs at the idle priority. */
			&xTxTask );

	xTaskCreate( prvRxTask,
			( const char * ) "GB",
			configMINIMAL_STACK_SIZE,
			NULL,
			tskIDLE_PRIORITY + 1,
			&xRxTask );

	/***********************************************************************************/

	/* Create the random generator. */
	xTaskCreate( randomGeneratorTask, 					/* The function that implements the task. */
			( const char * ) "RG", 		/* Text name for the task, provided to assist debugging only. */
			configMINIMAL_STACK_SIZE, 	/* The stack allocated to the task. */
			NULL, 						/* The task parameter is not used, so set to NULL. */
			tskIDLE_PRIORITY,			/* The task runs at the idle priority. */
			&randomGeneratorTaskHandler);

	/***********************************************************************************/

	/* Create the blink tqsk. */
	xTaskCreate( blinkLEDTask, 					/* The function that implements the task. */
			( const char * ) "Blinky", 		/* Text name for the task, provided to assist debugging only. */
			configMINIMAL_STACK_SIZE, 	/* The stack allocated to the task. */
			NULL, 						/* The task parameter is not used, so set to NULL. */
			tskIDLE_PRIORITY,			/* The task runs at the idle priority. */
			&blinkLEDTaskHandler);

	/***********************************************************************************/

	/* Create the blink tqsk. */
	xTaskCreate( prvSpeedSelectorTask, 					/* The function that implements the task. */
			( const char * ) "BS", 		/* Text name for the task, provided to assist debugging only. */
			configMINIMAL_STACK_SIZE, 	/* The stack allocated to the task. */
			NULL, 						/* The task parameter is not used, so set to NULL. */
			tskIDLE_PRIORITY,			/* The task runs at the idle priority. */
			&xSpeedSelectorTaskHandler);

	/***********************************************************************************/


	/* Create the queue used by the tasks.  The Rx task has a higher priority
	than the Tx task, so will preempt the Tx task and remove values from the
	queue as soon as the Tx task writes to the queue - therefore the queue can
	never have more than one item in it. */
	xQueue = xQueueCreate( 	1,						/* There is only one space in the queue. */
			sizeof( HWstring ) );	/* Each space in the queue is large enough to hold a uint32_t. */
	/***********************************************************************************/
	/* Queue pour passer le nb rando;  a la task qui  print sur l uart */
	xQueueRandomGen = xQueueCreate(1,sizeof(uint8_t));
	/***********************************************************************************/
	/* pass info from switch task to blink task */
	xQueueBlinkSpeed = xQueueCreate(1,sizeof(u32));

	/***********************************************************************************/

	/* Check the queues were created. */
	configASSERT( xQueue );
	configASSERT( xQueueRandomGen );
	configASSERT( xQueueBlinkSpeed );

	/***********************************************************************************/

	/* Create a timer with a timer expiry of 10 seconds. The timer would expire
	 after 10 seconds and the timer call back would get called. In the timer call back
	 checks are done to ensure that the tasks have been running properly till then.
	 The tasks are deleted in the timer call back and a message is printed to convey that
	 the example has run successfully.
	 The timer expiry is set to 10 seconds and the timer set to not auto reload. */
	xTimer = xTimerCreate( (const char *) "Timer",
			x10seconds,
			pdTRUE,
			(void *) TIMER_ID,
			vTimerCallback);
	/* Check the timer was created. */
	configASSERT( xTimer );

	/* start the timer with a block time of 0 ticks. This means as soon
	   as the schedule starts the timer will start running and will expire after
	   10 seconds */
	xTimerStart( xTimer, 0 );

	/* Start the tasks and timer running. */
	vTaskStartScheduler();

	/* If all is well, the scheduler will now be running, and the following line
	will never be reached.  If the following line does execute, then there was
	insufficient FreeRTOS heap memory available for the idle and/or timer tasks
	to be created.  See the memory management section on the FreeRTOS web site
	for more details. */
	for( ;; );
}




/*-----------------------------------------------------------*/
static void prvSpeedSelectorTask( void *pvParameters )
{
	xSemaphoreTake(xMutexAccessGPIO0,portMAX_DELAY);
	XGpio_SetDataDirection(&Gpio, 1, 0x01);
	xSemaphoreGive(xMutexAccessGPIO0);
	u32 speed = 0;
	u32 oldspeed = 0;
	while(TRUE)
	{
		xSemaphoreTake(xMutexAccessGPIO0,portMAX_DELAY);
		//speed = XGpio_DiscreteRead(&Gpio, 1);
		speed = 1;
		xSemaphoreGive(xMutexAccessGPIO0);
		sleep(1);
		if(speed != oldspeed){
			oldspeed = speed;
			xQueueSend( xQueueBlinkSpeed,			/* The queue being written to. */
					&speed, /* The address of the data being sent. */
					0UL );			/* The block time. */

		}
	}
}

/*-----------------------------------------------------------*/
static void blinkLEDTask( void *pvParameters )
{
	XGpio Gpio1;
	XGpio_Initialize(&Gpio1, XPAR_AXI_GPIO_1_DEVICE_ID);
	XGpio_SetDataDirection(&Gpio1, 1, 0x00);
	u32 blink_value = 1;
	u32 newSpeed = 1;
	int ctr = 0;
	const TickType_t xTimeoutInTicks = pdMS_TO_TICKS(1);
	unsigned long blinkSpeed = 20000;
	while(TRUE)
	{
		/* Block to wait for data arriving on the queue. */

		if(xQueueReceive(xQueueBlinkSpeed,&newSpeed,xTimeoutInTicks ) == pdPASS){
			blinkSpeed = 20000 * ((unsigned long)newSpeed+1);
			/* Print the received data. */
			xil_printf( "New blink speed: %u\r\n", blinkSpeed );
		}


		XGpio_DiscreteWrite(&Gpio1, 1, blink_value);
		usleep(blinkSpeed);
		blink_value <<= 1;
		ctr++;
		if(ctr == 8){
			blink_value=1;
			ctr = 0;
		}
	}
}
/*-----------------------------------------------------------*/
static void randomGeneratorTask( void *pvParameters )
{
	const TickType_t x1second = pdMS_TO_TICKS(3* DELAY_1_SECOND );
	uint8_t randomValue;
	for( ;; )
	{
		/* Delay for 1 second. */
		vTaskDelay( x1second );
		randomValue = rand()%256;
		/* Send the next value on the queue.  The queue should always be
		empty at this point so a block time of 0 is used. */
		xQueueSend( xQueueRandomGen,			/* The queue being written to. */
				&randomValue, /* The address of the data being sent. */
				0UL );			/* The block time. */
	}
}

/*-----------------------------------------------------------*/
static void prvTxTask( void *pvParameters )
{
	const TickType_t x1second = pdMS_TO_TICKS( DELAY_1_SECOND );

	for( ;; )
	{
		/* Delay for 1 second. */
		vTaskDelay( x1second );

		/* Send the next value on the queue.  The queue should always be
		empty at this point so a block time of 0 is used. */
		xQueueSend( xQueue,			/* The queue being written to. */
				HWstring, /* The address of the data being sent. */
				0UL );			/* The block time. */
	}
}

/*-----------------------------------------------------------*/
static void prvRxTask( void *pvParameters )
{
	xSemaphoreTake(xMutexAccessGPIO0,portMAX_DELAY);
	XGpio_SetDataDirection(&Gpio, 2, 0x00);
	xSemaphoreGive(xMutexAccessGPIO0);
	usleep(15000);

	uint8_t receivedRandom;
	//const char lcdBUffer[] ="PEPITO";


	void LCD_SendCommand(uint8_t command) {
		uint8_t data_u, data_l;
		data_u = (command&0xf0)>>1;
		data_l = ((command<<4)&0xf0)>>1;

		// Write upper nibble with EN = 1
		XGpio_DiscreteWrite(&Gpio, 2, ((data_u) | LCD_EN_MASK | LCD_RW_MASK));
		usleep(100);  // Wait for pulse width
		// Write upper nibble with EN = 0
		XGpio_DiscreteWrite(&Gpio, 2, (data_u) | LCD_RW_MASK);
		usleep(100);  // Wait for pulse width

		// Write lower nibble with EN = 1
		XGpio_DiscreteWrite(&Gpio, 2, ((data_l) | LCD_EN_MASK) | LCD_RW_MASK);
		usleep(100);  // Wait for pulse width

		// Write lower nibble with EN = 0
		XGpio_DiscreteWrite(&Gpio, 2, (data_l) | LCD_RW_MASK);
		usleep(100);  // Wait for pulse width

	}

	void LCD_SendData(char data) {
		char data_u, data_l;
		data_u = (data&0xf0)>>1;
		data_l = ((data<<4)&0xf0)>>1;
		// Set RS = 1

		// Write upper nibble with EN = 1
		XGpio_DiscreteWrite(&Gpio, 2, (data_u) | LCD_EN_MASK | LCD_RS_MASK | LCD_RW_MASK);
		usleep(100);  // Wait for pulse width

		// Write upper nibble with EN = 0
		XGpio_DiscreteWrite(&Gpio, 2, ((data_u) | LCD_RS_MASK | LCD_RW_MASK));
		usleep(100);  // Wait for pulse width

		// Write lower nibble with EN = 1
		XGpio_DiscreteWrite(&Gpio, 2, ((data_l) | LCD_EN_MASK | LCD_RS_MASK | LCD_RW_MASK));
		usleep(100);  // Wait for pulse width

		// Write lower nibble with EN = 0
		XGpio_DiscreteWrite(&Gpio, 2, ((data_l) | LCD_RS_MASK | LCD_RW_MASK));
		usleep(100);  // Wait for pulse width

	}

	void LCD_Init(void) {
		// Function Set: Initialize in 8-bit mode first
		LCD_SendCommand(0x30); // First part of the command (DL = 1)
		usleep(5000);  // Wait for the command to complete

		LCD_SendCommand(0x20); // Second part of the command (DL = 0, 4-bit mode)
		usleep(5000);  // Wait for the command to complete

		LCD_SendCommand(0x20); // Second part of the command (DL = 0, 4-bit mode)
		usleep(5000);  // Wait for the command to complete

		// Function Set: 4-bit mode, 2 lines, 5x8 dot character font
		LCD_SendCommand(0x28);
		usleep(2000);  // Wait for the command to complete

		// Display On/Off Control: Display on, cursor off, blinking off
		LCD_SendCommand(0x0C);
		usleep(2000);  // Wait for the command to complete

		// Clear Display
		LCD_SendCommand(0x01);
		usleep(2000);  // Wait for the command to complete

		// Entry Mode Set: Increment cursor, no display shift
		LCD_SendCommand(0x06);
		usleep(2000);  // Wait for the command to complete

		usleep(2000);          // Wait for clear display command to complete
	}

	void LCD_Print(char *str) {
		while (*str) {
			LCD_SendData(*str++);
		}
	}

	void LCD_Clear(void) {
		LCD_SendCommand(0x01); // Clear display
		usleep(2000);          // Wait for clear display command to complete
	}

	void LCD_SetCursor(uint8_t row, uint8_t col) {
		// Calculate DDRAM address based on row and column
		uint8_t address = col + (row == 0 ? 0x00 : 0x40);

		// Set cursor to specified address
		LCD_SendCommand(0x80 | address);
	}
	LCD_Init();
	LCD_SetCursor(0,0);
	LCD_Clear();
	//my_sprintf_uint8(lcdBUffer, receivedRandom);
	LCD_Print("PEPITO");

	for( ;; )
	{
		/* Block to wait for data arriving on the queue. */
		xQueueReceive( 	xQueueRandomGen,				/* The queue being read. */
				&receivedRandom,	/* Data is read into this address. */
				portMAX_DELAY );	/* Wait without a timeout for data. */

		/* Print the received data. */
		xil_printf( "Rx task received string from Tx task: %u\r\n", receivedRandom );
		//sprintf(lcdBUffer,"data : %u",receivedRandom);

	}
}

/*-----------------------------------------------------------*/
static void vTimerCallback( TimerHandle_t pxTimer )
{
	long lTimerId;
	configASSERT( pxTimer );

	lTimerId = ( long ) pvTimerGetTimerID( pxTimer );

	if (lTimerId != TIMER_ID) {
		xil_printf("FreeRTOS Hello World Example FAILED");
	}
	xil_printf("timer reloaded");
}



int my_sprintf_uint8(char *buf, uint8_t value) {
	char digits[3];  // Maximum 3 digits for a uint8_t value
	int len = 0;

	// Convert the uint8_t value to a string of digits
	int i = 0;
	do {
		digits[i++] = '0' + (value % 10);
		value /= 10;
	} while (value > 0);
	digits[i] = '\0';

	// Reverse the string of digits
	for (int j = 0; j < i / 2; j++) {
		char temp = digits[j];
		digits[j] = digits[i - j - 1];
		digits[i - j - 1] = temp;
	}

	// Copy the string of digits to the buffer
	while (digits[len] != '\0') {
		buf[len] = digits[len];
		len++;
	}
	buf[len] = '\0';

	return len;  // Return the length of the formatted string
}
