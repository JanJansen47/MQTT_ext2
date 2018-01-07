/*
 * vars.h
 *
 *  Created on: Feb 25, 2017
 *      Author: jan
 */

#ifndef MAIN_VARS_H_
#define MAIN_VARS_H_

extern int jan ;
extern  struct show {
	int line;
    char  txt [21] ;
} displ;


 extern struct  message {
 	char port;
 	int pin;
 	int setpin;
 }mes;

 extern struct alarm_data
 {
      char waar [8];
      char wat [8];
      char name[30];
      char waarde[8];
      struct tm tijdstip;
 };
struct alarm_data buf_in_alarm_data;
int last_alarm = 0;
extern QueueHandle_t menu2Queue ;
extern xSemaphoreHandle accessTFT, accessMenu;
#endif /* MAIN_VARS_H_ */
