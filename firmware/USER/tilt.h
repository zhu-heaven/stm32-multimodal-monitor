#ifndef APP_TILT_H
#define APP_TILT_H
#include <stdint.h>
typedef struct {
    int32_t angle100,roll100,pitch100;
    int32_t rate1000; /* signed degrees/minute * 1000, five-second window */
    int32_t positionRate100; /* signed mm/min * 100, same exact window */
    int32_t positionMean100; /* arithmetic mean of paired samples, 0.01 mm */
    uint32_t windowId; /* increments only after a complete paired window */
    uint8_t valid,rateValid;
} Tilt_Result;
/* position100: calibrated position in 0.01 mm; negative means invalid. */
void Tilt_Update(int16_t ax,int16_t ay,int16_t az,uint32_t nowMs,int32_t position100);
void Tilt_Invalidate(void);
const Tilt_Result *Tilt_Get(void);
#endif
