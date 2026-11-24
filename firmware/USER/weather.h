#ifndef WEATHER_H
#define WEATHER_H
#include "stm32f4xx.h"
void Weather_Update(void);
uint8_t Weather_Page(void);
void Weather_NextPage(void);
void Weather_Draw(void (*line)(uint16_t,const char *,uint32_t));
int32_t Weather_Rain3d10(void); /* -1 if missing or stale */
#endif
