#include "position.h"
#include "stm32f4xx_adc.h"
#pragma O2
#pragma Ospace
#if POSITION_ZERO_ADC == POSITION_REFERENCE_ADC
#error Position calibration endpoints must differ
#endif
#if POSITION_REFERENCE_CENTIMM <= 0 || POSITION_REFERENCE_CENTIMM > POSITION_STROKE_CENTIMM
#error Invalid position reference distance
#endif
void Position_Init(void)
{
    GPIO_InitTypeDef io;
    ADC_CommonInitTypeDef common;
    ADC_InitTypeDef adc;
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC,ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1,ENABLE);
    GPIO_StructInit(&io);
    io.GPIO_Pin=GPIO_Pin_0|GPIO_Pin_1|GPIO_Pin_2; io.GPIO_Mode=GPIO_Mode_AN;
    io.GPIO_PuPd=GPIO_PuPd_NOPULL; GPIO_Init(GPIOC,&io);
    ADC_CommonStructInit(&common);
    common.ADC_Mode=ADC_Mode_Independent;
    common.ADC_Prescaler=ADC_Prescaler_Div4;
    common.ADC_DMAAccessMode=ADC_DMAAccessMode_Disabled;
    common.ADC_TwoSamplingDelay=ADC_TwoSamplingDelay_5Cycles;
    ADC_CommonInit(&common);
    ADC_StructInit(&adc);
    adc.ADC_Resolution=ADC_Resolution_12b;
    adc.ADC_ScanConvMode=DISABLE; adc.ADC_ContinuousConvMode=DISABLE;
    adc.ADC_ExternalTrigConvEdge=ADC_ExternalTrigConvEdge_None;
    adc.ADC_DataAlign=ADC_DataAlign_Right; adc.ADC_NbrOfConversion=1;
    ADC_Init(ADC1,&adc);
    /* PC0 = ADC1 channel 10. Long acquisition suits a potentiometer. */
    ADC_RegularChannelConfig(ADC1,ADC_Channel_10,1,ADC_SampleTime_480Cycles);
    ADC_Cmd(ADC1,ENABLE);
}
uint8_t Analog_ReadChannel(uint8_t channel,uint16_t *raw)
{
    uint8_t i; uint32_t sum=0,timeout;
    if(channel<ADC_Channel_10 || channel>ADC_Channel_12) return 0;
    ADC_RegularChannelConfig(ADC1,channel,1,ADC_SampleTime_480Cycles);
    /* Discard first conversion after mux switch, then average 16 samples. */
    for(i=0;i<17;i++) {
        ADC_ClearFlag(ADC1,ADC_FLAG_EOC);
        ADC_SoftwareStartConv(ADC1); timeout=100000;
        while(ADC_GetFlagStatus(ADC1,ADC_FLAG_EOC)==RESET)
            if(!--timeout) return 0;
        if(i) sum+=ADC_GetConversionValue(ADC1);
        else (void)ADC_GetConversionValue(ADC1);
    }
    *raw=(uint16_t)((sum+8)/16); return 1;
}
uint8_t Position_Read(uint16_t *raw) { return Analog_ReadChannel(ADC_Channel_10,raw); }
/* Optimize only this pure arithmetic conversion, preserving ADC timings. */
#pragma O2
uint32_t Position_CentiMM(uint16_t raw)
{
    int32_t span=POSITION_REFERENCE_ADC-POSITION_ZERO_ADC;
    int32_t delta=(int32_t)raw-POSITION_ZERO_ADC;
    int32_t value;
    /* Normalize polarity, clamp below zero, and round to the nearest 0.01 mm. */
    if(span<0) { span=-span; delta=-delta; }
    if(delta<=0) return 0;
    value=(delta*POSITION_REFERENCE_CENTIMM+span/2)/span;
    if(value>POSITION_STROKE_CENTIMM) value=POSITION_STROKE_CENTIMM;
    return (uint32_t)value;
}
