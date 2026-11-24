#ifndef APP_KEY_H
#define APP_KEY_H
#include "stm32f4xx.h"
/* Board example: K0 = PE4, active low. No external key wiring required. */
void Key_Init(void);
void Key_Tick1ms(void);
uint8_t Key_TakePress(void);
/* K1 = PE3, active low; one event per debounced press. */
uint8_t Key1_TakePress(void);
/* Board K-UP/WK_UP = PA0, active high. */
uint8_t KeyUp_TakePress(void);
uint32_t Key_Millis(void);
#endif
