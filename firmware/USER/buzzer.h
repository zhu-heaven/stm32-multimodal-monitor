#ifndef APP_BUZZER_H
#define APP_BUZZER_H
#include "stm32f4xx.h"
/* PA8 drives the IO/S of a high-level-triggered active buzzer module. */
void Buzzer_Init(void);
void Buzzer_Toggle(void); /* manual cycle: off -> double-beep -> continuous -> off */
void Buzzer_Set(uint8_t on);
uint8_t Buzzer_IsOn(void);
uint8_t Buzzer_Mode(void); /* effective mode, 0 silent / 1 double-beep / 2 continuous */
void Buzzer_SetAlarm(uint8_t level); /* 0 manual, 1 double-beep, 2 continuous */
void Buzzer_Tick1ms(void);
uint8_t Buzzer_AlarmLevel(void);
#endif
