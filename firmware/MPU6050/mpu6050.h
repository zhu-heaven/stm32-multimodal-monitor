#ifndef MPU6050_H
#define MPU6050_H
#include "stm32f4xx.h"
typedef struct { int16_t ax,ay,az,gx,gy,gz,temperature; } MPU6050_Sample;
uint8_t MPU6050_Init(void);
uint8_t MPU6050_Read(MPU6050_Sample *sample);
uint8_t MPU6050_GetID(void);
#endif
