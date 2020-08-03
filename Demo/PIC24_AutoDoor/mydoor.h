/* 
 * File:   mydoor.h
 * Author: Youssouf El Allali
 *
 * Created on 17 avril 2020, 19:14
 */

#ifndef MYDOOR_H
#define	MYDOOR_H

#ifdef	__cplusplus
extern "C" {
#endif
    
#define NBSEMPHR    4   //number of door signal types.
    
enum door_state {open, openSens, opening, closed, closedSens, closing}; //!!! don't change the order.
//openSens/closedSens : declare open/closed due to door sensor signal.

typedef struct{
    enum door_state door;
    char fire;          //may be useful in further upgrade.
    char security;      //may be useful in further upgrade.
    char sensorOerr;    //may be useful in further upgrade.
    char sensorCerr;    //may be useful in further upgrade.
} stateMsg;

void presenceTask(int);
void fireAlarmTask(int);
void securityAlarmTask(int);
void actionDoor();
void uartRXTask(int);

#ifdef	__cplusplus
}
#endif

#endif	/* MYDOOR_H */

