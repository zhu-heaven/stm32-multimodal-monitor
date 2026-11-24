#include "buzzer.h"
#pragma O2
#pragma Ospace
static volatile uint8_t enabled,manual,alarm;
static volatile uint16_t phase;
static void Drive(void)
{
    uint8_t mode=alarm?alarm:manual;
    uint8_t on=mode==2?1:mode==1?(phase<150 || (phase>=300 && phase<450)):0;
    if(on!=enabled) {
        enabled=on; GPIO_WriteBit(GPIOA,GPIO_Pin_8,on?Bit_SET:Bit_RESET);
    }
}
void Buzzer_Init(void)
{
    GPIO_InitTypeDef io;
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA,ENABLE);
    GPIO_ResetBits(GPIOA,GPIO_Pin_8);
    GPIO_StructInit(&io);
    io.GPIO_Pin=GPIO_Pin_8;
    io.GPIO_Mode=GPIO_Mode_OUT;
    io.GPIO_OType=GPIO_OType_PP;
    io.GPIO_Speed=GPIO_Speed_2MHz;
    io.GPIO_PuPd=GPIO_PuPd_DOWN;
    GPIO_Init(GPIOA,&io);
    enabled=manual=alarm=0; phase=0;
}
void Buzzer_Toggle(void)
{
    if(alarm) return;
    manual=(uint8_t)(manual<2?manual+1:0); phase=0; Drive();
}
void Buzzer_Set(uint8_t on)
{
    if(alarm) { manual=0; return; }
    manual=on?2u:0u; phase=0; Drive();
}
uint8_t Buzzer_IsOn(void) { return enabled; }
uint8_t Buzzer_Mode(void) { return alarm?alarm:manual; }
uint8_t Buzzer_AlarmLevel(void) { return alarm; }
void Buzzer_SetAlarm(uint8_t level)
{
    if(level!=alarm) { alarm=level; phase=0; if(level) manual=0; }
    Drive();
}
void Buzzer_Tick1ms(void)
{
    if(++phase>=1000) phase=0;
    Drive();
}
