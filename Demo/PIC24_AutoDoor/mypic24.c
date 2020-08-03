/* 
    Author: Youssouf El Allali.
    Date:   April 17th 2020.
 */

/* Standard includes. */
#include <stdio.h>
#include <stdlib.h>
#include <xc.h>

/* Application includes. */
#include "portmacro.h"
#include "FreeRTOSConfig.h"
#include "mypic24.h"

#define BAUD_RATE 9600

void initTRIS() {
    TRISA = 0b01;
    TRISB = 0;
    TRISC = 0;
    TRISE = 0b11;
    TRISD = 0b01;
    TRISG = 0b10;
}

void configOSC() {
    //FRCDIV mode is the default one.
    CLKDIVbits.DOZEN    = 0;
    CLKDIVbits.RCDIV    = 0b011;    //divide-by-8 = 1MHz.
    
}

void initUART1() {
    U1MODEbits.UEN      = 0;
    U1MODEbits.WAKE     = 0;
    U1MODEbits.BRGH     = 0;
    U1MODEbits.PDSEL    = 0;
    U1MODEbits.STSEL    = 0;
    U1MODEbits.UARTEN   = 1;
    
    U1BRG = (unsigned short)(( (float)configCPU_CLOCK_HZ / ( (float)16 * (float)BAUD_RATE ) ) - (float)0.5);
    
//    U1STAbits.UTXEN     = 1;
    U1STAbits.URXISEL   = 0;    //interrupt on single char received.
//    U1STAbits.UTXISEL0  = 0;
//    U1STAbits.UTXISEL1  = 0;
    
    /* Interrupts must not be allowed to occur before starting the scheduler 
       as they may attempt to perform a context switch. They'll be re-enabled 
       automatically by tasks' stacks. */
    portDISABLE_INTERRUPTS();
    
    IFS0bits.U1RXIF = 0;
//    IFS0bits.U1TXIF = 0;
    IPC2bits.U1RXIP = 2;    //interrupt priority (hardware).
//    IPC3bits.U1TXIP = 1;    //interrupt priority (hardware).
    IEC0bits.U1RXIE = 1;
//    IEC0bits.U1TXIE = 1;
    
    /* Clear the Rx buffer. */
    char c;
	while( U1STAbits.URXDA ) {
		c = U1RXREG;
	}
    
}
