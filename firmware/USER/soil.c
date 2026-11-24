#include "soil.h"
#include "position.h"
#pragma O2

/* Provisional endpoints, not a calibrated volumetric water content. */
#define SOIL_DRY_ADC 4093L
#define SOIL_WET_ADC 1000L
static uint32_t lastSample;
static int32_t filtered256;
static int32_t shown=-1;

void Soil_Update(uint32_t now)
{
    uint16_t raw;
    int32_t percent;
    if((uint32_t)(now-lastSample)<100u) return;
    lastSample=now;
    if(!Analog_ReadChannel(12,&raw)) { shown=-1; return; }
    if(shown<0) filtered256=(int32_t)raw*256;
    else filtered256+=((int32_t)raw*256-filtered256)/8;
    percent=(SOIL_DRY_ADC-((filtered256+128)/256))*1000L/
            (SOIL_DRY_ADC-SOIL_WET_ADC);
    if(percent<0) percent=0;
    if(percent>1000) percent=1000;
    /* A fixed deadband against the displayed value preserves accumulated
     * slow changes; do not compare only consecutive samples. */
    if(shown<0 || percent-shown>=3 || shown-percent>=3) shown=percent;
}
int32_t Soil_Percent10(void) { return shown; }
