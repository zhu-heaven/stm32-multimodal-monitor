#ifndef APP_DISPLAY_H
#define APP_DISPLAY_H
#include "stm32f4xx.h"
void Display_Line(uint16_t y,const char *text,uint32_t color);
void Display_Row(uint16_t y,const char *label,const char *value,uint32_t color);
#endif
