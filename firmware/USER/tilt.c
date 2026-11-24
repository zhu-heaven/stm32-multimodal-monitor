#include "tilt.h"
#include <math.h>
#pragma O2
#pragma Ospace

/* Existing MPU configuration is +/-16g: 2048 raw counts/g.
 * Quasi-static gravity inclinometer; not a yaw/heading estimator.
 * Axis offsets/scales require physical calibration for precision work. */
#define DEG_PER_RAD 57.2957795131f
#define FILTER_MS 500.0f
#define RATE_WINDOW_MS 5000u
static Tilt_Result result;
static float x,y,z,rateBase;
static uint32_t lastMs,rateMs;
static uint8_t started;
static int32_t positionBase;
static uint32_t positionSum;
static uint16_t positionCount;

static int32_t RoundSigned(float value)
{ return (int32_t)(value>=0?value+0.5f:value-0.5f); }

void Tilt_Invalidate(void)
{
    started=0;
    result.valid=0; result.rateValid=0;
}
const Tilt_Result *Tilt_Get(void) { return &result; }

void Tilt_Update(int16_t ax,int16_t ay,int16_t az,uint32_t nowMs,int32_t position100)
{
    float fx=(float)ax/2048.0f,fy=(float)ay/2048.0f,fz=(float)az/2048.0f;
    float magnitude2=fx*fx+fy*fy+fz*fz;
    float alpha,angle;
    uint32_t elapsed=nowMs-lastMs;
    /* Reject obvious shocks/free fall; this does not reject every source
     * of linear acceleration. Long sample gaps restart filter/rate history. */
    if(magnitude2<0.64f || magnitude2>1.44f) { Tilt_Invalidate(); return; }
    if(!started || elapsed>1500u) {
        x=fx; y=fy; z=fz;
        rateMs=nowMs; result.rateValid=0;
        started=1;
        angle=atan2f(sqrtf(x*x+y*y),z)*DEG_PER_RAD;
        rateBase=angle;
        positionBase=position100;
        positionSum=position100<0?0:(uint32_t)position100;
        positionCount=position100<0?0:1;
    } else {
        alpha=(float)elapsed/(FILTER_MS+(float)elapsed);
        x+=alpha*(fx-x); y+=alpha*(fy-y); z+=alpha*(fz-z);
        if(elapsed && position100>=0 && positionBase>=0) {
            positionSum+=(uint32_t)position100; positionCount++;
        }
    }
    lastMs=nowMs;
    /* Guard low-pass cancellation during a rapid 180-degree flip. */
    if(x*x+y*y+z*z<0.25f) { Tilt_Invalidate(); return; }
    angle=atan2f(sqrtf(x*x+y*y),z)*DEG_PER_RAD;
    result.angle100=RoundSigned(angle*100.0f);
    result.roll100=RoundSigned(atan2f(y,z)*DEG_PER_RAD*100.0f);
    result.pitch100=RoundSigned(atan2f(-x,sqrtf(y*y+z*z))*DEG_PER_RAD*100.0f);
    result.valid=1;
    /* Both rates require an uninterrupted window of valid paired samples. */
    if(position100<0 || positionBase<0) {
        positionBase=position100; rateBase=angle; rateMs=nowMs;
        result.rateValid=0;
        positionSum=position100<0?0:(uint32_t)position100;
        positionCount=position100<0?0:1;
    }
    if((uint32_t)(nowMs-rateMs)>=RATE_WINDOW_MS) {
        result.rate1000=RoundSigned((angle-rateBase)*60000000.0f/(float)(nowMs-rateMs));
        result.rateValid=1;
        result.positionRate100=RoundSigned((position100-positionBase)*60000.0f/(float)(nowMs-rateMs));
        result.positionMean100=(int32_t)((positionSum+positionCount/2u)/positionCount);
        result.windowId++;
        positionSum=(uint32_t)position100; positionCount=1;
        positionBase=position100;
        rateBase=angle; rateMs=nowMs;
    }
}
