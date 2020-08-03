/* 
    Author: Youssouf El Allali.
    Date:   April 17th 2020.
 */

/*
 * FreeRTOS Kernel V10.3.1
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
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

/*
 * Creates all the demo application tasks, then starts the scheduler.  The WEB
 * documentation provides more details of the standard demo application tasks.
 */

/* Standard includes. */
#include <stdio.h>
#include <stdlib.h>
#include <xc.h>

//#define _XTAL_FREQ 1000000


/* Scheduler includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "croutine.h"
#include "semphr.h"

/* Application includes. */
#include "mydoor.h"
#include "mypic24.h"

/*-----------------------------------------------------------*/

xSemaphoreHandle mutexD;
xSemaphoreHandle semphrArray[NBSEMPHR];
stateMsg sMsg;
xSemaphoreHandle openSemphr, closeSemphr, sensorEnSemphr;
QueueSetHandle_t doorCmdSet;
xQueueHandle uartCmdQ;
   
/* Prototypes for the standard FreeRTOS callback/hook functions implemented
within this file. */
void vApplicationStackOverflowHook( TaskHandle_t pxTask, char *pcTaskName );

/*-----------------------------------------------------------*/
/*
 * 
 */
int main( void )
{   
    //TRIS init
    initTRIS();
    //OSC init
    configOSC();
    //UART init
    initUART1();
    
    mutexD = xSemaphoreCreateMutex();   //By default, initialised as "given".

    for (int i=0; i<NBSEMPHR; i++) {
        semphrArray[i] = xSemaphoreCreateBinary();  //By default, initialised as "taken".
        xSemaphoreGive(semphrArray[i]);
    }

    sMsg.door = -1; //MUST BE initialised, by a value out of range [0,5].
//    sMsg.fire = 0;
//    sMsg.security = 0;
//    sMsg.sensorOerr = 0;
//    sMsg.sensorCerr = 0;
    
    openSemphr = xSemaphoreCreateBinary();
    closeSemphr = xSemaphoreCreateBinary();
    sensorEnSemphr = xSemaphoreCreateBinary();
    doorCmdSet = xQueueCreateSet( 2 );    //!!!enough space for two BinSemphrHandles (min two events at a time) to avoid an overwrite, and hence a break of Give/Select/Take mechanism for either openSemphr or closeSemphr.
    xQueueAddToSet(openSemphr, doorCmdSet);     //in main since "Cannot add a queue/semaphore to a queue set if there are already items in the queue/semaphore."
    xQueueAddToSet(closeSemphr, doorCmdSet);    //in main since "Cannot add a queue/semaphore to a queue set if there are already items in the queue/semaphore."
    uartCmdQ = xQueueCreate(1, sizeof(char));
    
    /* Creating tasks. */
    xTaskCreate( (TaskFunction_t)presenceTask, "PresenceT", configMINIMAL_STACK_SIZE, 0, tskIDLE_PRIORITY+1, NULL);
    xTaskCreate( (TaskFunction_t)securityAlarmTask, "SecurityAlarmT", configMINIMAL_STACK_SIZE, 1,tskIDLE_PRIORITY+2, NULL);
    xTaskCreate( (TaskFunction_t)fireAlarmTask, "FireAlarmT", configMINIMAL_STACK_SIZE, 2,tskIDLE_PRIORITY+3, NULL);
    xTaskCreate( (TaskFunction_t)uartRXTask, "UartRXTask", configMINIMAL_STACK_SIZE, 3, tskIDLE_PRIORITY+4, NULL);
    xTaskCreate( (TaskFunction_t)actionDoor, "ActionDoorT", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY+1, NULL);
    
    /* Finally start the scheduler. */
	vTaskStartScheduler();

	/* Will only reach here if there is insufficient heap available to start
	the scheduler. */
	return 0;
}

/*-----------------------------------------------------------*/

void vApplicationIdleHook( void )
{

}
/*-----------------------------------------------------------*/

void vApplicationStackOverflowHook( TaskHandle_t pxTask, char *pcTaskName )
{
	( void ) pcTaskName;
	( void ) pxTask;

	/* Run time stack overflow checking is performed if
	configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2.  This hook
	function is called if a stack overflow is detected. */
    PORTEbits.RE9 = 1;
	taskDISABLE_INTERRUPTS();
	for( ;; );
}

void vApplicationTickHook( void ) {
    //PORTDbits.RD9 = ~PORTDbits.RD9;   //for tick period measurement.

    //For debug:
/*  
    if (uxSemaphoreGetCount(openSemphr))
        PORTDbits.RD3 = 1;
    else
        PORTDbits.RD3 = 0;
    
    if (uxSemaphoreGetCount(closeSemphr))
        PORTDbits.RD6 = 1;
    else
        PORTDbits.RD6 = 0;
    
    if (uxSemaphoreGetCount(mutexD))
        PORTDbits.RD10 = 1;
    else
        PORTDbits.RD10 = 0;
*/
/*    
    if (PORTAbits.RA0)
        xSemaphoreGive(openSemphr);
    if (PORTDbits.RD0)
        xSemaphoreGive(closeSemphr);
*/
}
