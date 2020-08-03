/* 
    Author: Youssouf El Allali.
    Date:   April 17th 2020.
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

#define DOOR_MASK   (0b11110)
#define DOOR_PORT   PORTC
#define SENSORO_IS  PORTEbits.RE1
#define SENSORC_IS  PORTEbits.RE0
#define DOOR_DELAY  1000    //in ms. //DOOR_DELAY is a delay for mechanical constraints. Relative delay.
#define PRESENCE    PORTGbits.RG1   //Presence input signal port.
#define LED_PRS     PORTGbits.RG0   //Presence led port.
#define FIRE_SIG    PORTAbits.RA0   //Fire alarm input signal port.
#define LED_FIRE    PORTAbits.RA1   //Fire alarm led port.
#define SEC_SIG     PORTDbits.RD0   //Security alarm input signal port.
#define LED_SEC     PORTDbits.RD1   //Security alarm led port.

#define PRESENCE_DELAY          300 //in ms. //Presence task period (relative delay).
#define SECURITY_DELAY          200 //in ms. //Security task period (absolute delay).
#define FIRE_DELAY              200 //in ms. //Fire task period     (absolute delay).

#define SENSORS_FOR_PRESENCE    1   //enable door sensors for presence task?
#define SENSORS_FOR_SECURITY    0   //enable door sensors for security task?
#define SENSORS_FOR_FIRE        0   //enable door sensors for fire task?
#define SENSORS_FOR_UART        0   //enable door sensors for UART task?

extern xSemaphoreHandle mutexD;
extern xSemaphoreHandle semphrArray[NBSEMPHR];
extern stateMsg sMsg;
extern xSemaphoreHandle openSemphr, closeSemphr, sensorEnSemphr;
extern QueueSetHandle_t doorCmdSet;
extern xQueueHandle uartCmdQ;


char openDoor( const int sem_ind, const char sensor_en ) {
    const unsigned int open_state = open + (sensor_en * SENSORO_IS);
    //previous line means that the door is considered open here if it is really fully open or if the sensorO is enabled AND triggered.
    
    if (sensor_en == 0)
        xSemaphoreTake(sensorEnSemphr, 0);  //disable door sensors.
    else
        xSemaphoreGive(sensorEnSemphr);     //enable door sensors.
    
    if ( (sMsg.door != open_state) && (sMsg.door != opening) ) {
        
        if (xSemaphoreTake(mutexD, pdMS_TO_TICKS(DOOR_DELAY)) == pdPASS) {  //wait for mutex.
            if (uxSemaphoreGetCount(semphrArray[sem_ind])) {    //to check whether the semaphore (permission) has been taken meanwhile.
                xSemaphoreGive(openSemphr);
                xSemaphoreGive(mutexD);     //give back mutex.
                return 1;   //command sent.
            }
            else
                xSemaphoreGive(mutexD);
        }
        
        return 2;   //mutex or semphr not granted.
    }

    return 0;   //already fully open or opening.
}

char closeDoor( const int sem_ind, const char sensor_en ) {
    const unsigned int closed_state = closed + (sensor_en * SENSORC_IS);
    //previous line means that the door is considered closed here if it is really fully closed or if the sensorC is enabled AND triggered.
        
    if (sensor_en == 0)
        xSemaphoreTake(sensorEnSemphr, 0);  //disable door sensors.
    else
        xSemaphoreGive(sensorEnSemphr);     //enable door sensors.
    
    if ( (sMsg.door != closed_state) && (sMsg.door != closing) ) {
        
        if (xSemaphoreTake(mutexD, pdMS_TO_TICKS(DOOR_DELAY)) == pdPASS) {  //wait for mutex.
            if (uxSemaphoreGetCount(semphrArray[sem_ind])) {    //to check whether the semaphore (permission) has been taken meanwhile.
                xSemaphoreGive(closeSemphr);
                xSemaphoreGive(mutexD);     //give back mutex.
                return 1;   //command sent.
            }
            else
                xSemaphoreGive(mutexD);
        }
        
        return 2;   //mutex or semphr not granted.
    }
    
    return 0;   //already fully closed or closing.
}

void presenceTask(int sem_ind) {
    while(1) {
        
        if (PRESENCE) {
            LED_PRS = 1;
            if (uxSemaphoreGetCount(semphrArray[sem_ind])) {  //to avoid taking mutex while not allowed.
                openDoor(sem_ind, SENSORS_FOR_PRESENCE);
            }
        }
        else {
            LED_PRS = 0;
            if (uxSemaphoreGetCount(semphrArray[sem_ind])) {  //to avoid taking mutex while not allowed.
                closeDoor(sem_ind, SENSORS_FOR_PRESENCE);
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(PRESENCE_DELAY));  //relative delay.
    }
    
    vTaskDelete(NULL);
}

void securityAlarmTask(int sem_ind) {
    TickType_t xLastWake = xTaskGetTickCount();
    char giveback = 0;
    
    BaseType_t sem_taken[sem_ind];
    for (int i=0; i<sem_ind; i++) {
        sem_taken[i] = pdFALSE;
    }
    
    while(1) {
        if (SEC_SIG) {
            LED_SEC = 1;
            sMsg.security = 1;
            
            for (int i=0; i<sem_ind; i++) {
                if (sem_taken[i] != pdTRUE)
                    sem_taken[i] = xSemaphoreTake(semphrArray[i], 0);
            }
            giveback = 1;   //mark that semaphores are to be given back later.
            
            if (uxSemaphoreGetCount(semphrArray[sem_ind])) {  //to avoid taking mutex while not allowed.
                closeDoor(sem_ind, SENSORS_FOR_SECURITY);
            }
            
        }
        else if (giveback) {     //to avoid unnecessary for-loops.
            LED_SEC = 0;
            sMsg.security = 0;
            
            for (int i=0; i<sem_ind; i++) {
                if (sem_taken[i] == pdTRUE)
                    if (xSemaphoreGive(semphrArray[i]))
                        sem_taken[i] = pdFALSE;
            }
            
            giveback = 0;
            
        }
        
        vTaskDelayUntil(&xLastWake, pdMS_TO_TICKS(SECURITY_DELAY)); //absolute delay.
    }
    
    vTaskDelete(NULL);
}

void fireAlarmTask(int sem_ind) {
    TickType_t xLastWake = xTaskGetTickCount();
    char giveback = 0;
    
    BaseType_t sem_taken[sem_ind];
    for (int i=0; i<sem_ind; i++) {
        sem_taken[i] = pdFALSE;
    }

    while(1) {
        if (FIRE_SIG) {
            LED_FIRE = 1;
            sMsg.fire = 1;
            
            for (int i=0; i<sem_ind; i++) {
                if (sem_taken[i] != pdTRUE)
                    sem_taken[i] = xSemaphoreTake(semphrArray[i], 0);
            }
            giveback = 1;   //marks that semaphores are to be given back later.
            
            if (uxSemaphoreGetCount(semphrArray[sem_ind])) {  //to avoid taking mutex while not allowed.
                openDoor(sem_ind, SENSORS_FOR_FIRE);
            }
            
        }
        else if (giveback) {     //to avoid unnecessary for-loops.
            LED_FIRE = 0;
            sMsg.fire = 0;
            
            for (int i=0; i<sem_ind; i++) {
                if (sem_taken[i] == pdTRUE)
                    if (xSemaphoreGive(semphrArray[i]))
                        sem_taken[i] = pdFALSE;
            }
            
            giveback = 0;
            
        }

        vTaskDelayUntil(&xLastWake, pdMS_TO_TICKS(FIRE_DELAY)); //absolute delay.
    }
    
    vTaskDelete(NULL);
}

void actionDoor() {
    SemaphoreHandle_t receivedSemphr;
    char sensor_en;
    
    while( (receivedSemphr = xQueueSelectFromSet(doorCmdSet, portMAX_DELAY)) ) {    //waits indefinitly, better than looping in vain with while(1).  
        
        xSemaphoreTake(receivedSemphr, 0);  //only taken after being selected from set. To avoid desynchronisation of Give/Select/Take mechanism of the relevent semaphore.
        
        if (receivedSemphr == openSemphr) {
            
            while( ((DOOR_PORT & DOOR_MASK) !=0) && !(SENSORO_IS && ( sensor_en=uxSemaphoreGetCount(sensorEnSemphr) )) ) {    //is the door still not fully open?
                sMsg.door = opening; //if still not fully open.
                DOOR_PORT = DOOR_PORT & ((DOOR_PORT & DOOR_MASK)>>1 | ~DOOR_MASK);
                xQueuePeek(closeSemphr, NULL, pdMS_TO_TICKS(DOOR_DELAY));  //keep an eye on the other cmd semphr while delaying.
            }
            
            sMsg.door = open + sensor_en; //mark the door as open or openSens.
        }
        else if (receivedSemphr == closeSemphr) {
            
            while( ((DOOR_PORT & DOOR_MASK) != DOOR_MASK) && !(SENSORC_IS && ( sensor_en=uxSemaphoreGetCount(sensorEnSemphr) )) ) {   //is the door still not fully closed?
                sMsg.door = closing; //if still not fully closed.
                DOOR_PORT = DOOR_PORT | ((DOOR_PORT | ~DOOR_MASK)<<1 & DOOR_MASK);
    //          PORTDbits.RD10 = 1;   //for timing measurement.
                xQueuePeek(openSemphr, NULL, pdMS_TO_TICKS(DOOR_DELAY));  //keep an eye on the other cmd semphr while delaying.
    //          PORTDbits.RD10 = 0;   //for timing measurement.
            }
            
            sMsg.door = closed + sensor_en; //mark the door as closed or closedSens.
        }
        //DOOR_DELAY is a delay for mechanical constraints. Relative delay.
    }
    
    vTaskDelete(NULL);
}

void uartRXTask(int sem_ind) {
    unsigned char cmd;  //uart command from ISR.
    char giveback = 0;
    
    BaseType_t sem_taken[sem_ind];
    for (int i=0; i<sem_ind; i++) {
        sem_taken[i] = pdFALSE;
    }
    
    while ( xQueueReceive(uartCmdQ, &cmd, portMAX_DELAY) ) {    //is ready each time a cmd is in queue.
        switch(cmd) {
            case 207:               //for simulation purposes. It should be in fact 'o'.
                for (int i=0; i<sem_ind; i++) {
                    if (sem_taken[i] != pdTRUE)
                        sem_taken[i] = xSemaphoreTake(semphrArray[i], 0);
                }
                giveback = 1;
                
                if (uxSemaphoreGetCount(semphrArray[sem_ind])) {  //to avoid taking mutex while not allowed.
                    xSemaphoreTake(sensorEnSemphr, 0);  //disable door sensors.
                    openDoor(sem_ind, 0);
                }
                break;
                
            case 195:               //for simulation purposes. It should be in fact 'c'.
                for (int i=0; i<sem_ind; i++) {
                    if (sem_taken[i] != pdTRUE)
                        sem_taken[i] = xSemaphoreTake(semphrArray[i], 0);
                }
                giveback = 1;
                
                if (uxSemaphoreGetCount(semphrArray[sem_ind])) {  //to avoid taking mutex while not allowed.
                    xSemaphoreTake(sensorEnSemphr, 0);  //disable door sensors.
                    closeDoor(sem_ind, 0);
                }
                break;
                
            case 198:               //for simulation purposes. It should be in fact 'f'.
                if (giveback) {     //to avoid unnecessary for-loops.
                    for (int i=0; i<sem_ind; i++) {
                        if (sem_taken[i] == pdTRUE)
                            if (xSemaphoreGive(semphrArray[i]))
                                sem_taken[i] = pdFALSE;
                    }
                    giveback = 0;
                }
                break;
        }
    }
    
    vTaskDelete(NULL);
}

/*___INTERRUPTS___*/
void __attribute__((__interrupt__, auto_psv)) _U1RXInterrupt( void )
{
    unsigned char c;
    static unsigned char pc;     //MUST BE static.
    static char i = -1;          //MUST BE static.
    portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;

	IFS0bits.U1RXIF = 0;    //clear interrupt flag.

	while( U1STAbits.URXDA ) {
		c = U1RXREG;
        
		if (c == 122)               //for simulation purposes. It should be in fact ':'.
            i = 0;
        else if (i == 0) {
            pc = c;     //memorise the character.
            i = 1;
        }
        else if (i == 1) {
            if (c == 13)            //for simulation purposes. It should be in fact '\n'.
                xQueueOverwriteFromISR(uartCmdQ, &pc, &xHigherPriorityTaskWoken);
            else
                i = -1;
        }
	}

	if( xHigherPriorityTaskWoken != pdFALSE )
	{
		taskYIELD();
	}
}
