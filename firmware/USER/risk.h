#ifndef APP_RISK_H
#define APP_RISK_H
#include "tilt.h"
typedef struct {
    int32_t displacement100,angle100,soil10,rain10,forecast10;
    uint16_t score100;
    uint8_t baseline,valid,level,confirming;
} Risk_Result;
/* Experimental bench thresholds, not validated field-warning criteria. */
uint8_t Risk_SetBaseline(const Tilt_Result *tilt);
void Risk_Update(const Tilt_Result *tilt,int32_t soil10,int32_t rain10,int32_t forecast10);
const Risk_Result *Risk_Get(void);
void Risk_Init(void);
unsigned Risk_Weight(unsigned index);
int Risk_ApplyConfig(const char *csv);
void Risk_ConfigText(char *csv);
#endif
