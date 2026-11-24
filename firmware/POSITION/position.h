#ifndef POSITION_H
#define POSITION_H
#include "stm32f4xx.h"
/* KTC-75: terminal 1 = 3.3 V, wiper 2 = PC0, terminal 3 = GND.
 * Measured 2026-11-16: retracted 4085; ruler reference 20 mm = 2991.
 * The 10 mm check (3533..3534) maps to 10.07..10.09 mm.
 * Travel above 20 mm is extrapolated, not a verified full-stroke calibration. */
#define POSITION_ZERO_ADC 4085L
#define POSITION_REFERENCE_ADC 2991L
#define POSITION_REFERENCE_CENTIMM 2000L
#define POSITION_STROKE_CENTIMM 7500L
void Position_Init(void);
uint8_t Position_Read(uint16_t *raw);
/* ADC1 shared by position PC0, rain PC1 and soil PC2. */
uint8_t Analog_ReadChannel(uint8_t channel,uint16_t *raw);
uint32_t Position_CentiMM(uint16_t raw);
#endif
