#ifndef SOIL_H
#define SOIL_H
#include <stdint.h>
/* Shared relative humidity, tenths of a percent; -1 means ADC failure. */
void Soil_Update(uint32_t now);
int32_t Soil_Percent10(void);
#endif
