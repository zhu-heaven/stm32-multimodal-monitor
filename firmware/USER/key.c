#include "key.h"
#include "buzzer.h"
#pragma O2
#pragma Ospace

static volatile uint32_t millis;
static volatile uint8_t presses[3];
static uint8_t candidate[3],stable[3],count[3],armed[3];
static uint8_t Released(uint8_t index)
{
    if(index==2) return (uint8_t)!GPIO_ReadInputDataBit(GPIOA,GPIO_Pin_0);
    return GPIO_ReadInputDataBit(GPIOE,index?GPIO_Pin_3:GPIO_Pin_4);
}

void Key_Init(void)
{
    GPIO_InitTypeDef io;
    uint8_t i;
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE|RCC_AHB1Periph_GPIOA,ENABLE);
    GPIO_StructInit(&io);
    io.GPIO_Pin=GPIO_Pin_4|GPIO_Pin_3;
    io.GPIO_Mode=GPIO_Mode_IN;
    io.GPIO_PuPd=GPIO_PuPd_UP;
    GPIO_Init(GPIOE,&io);
    io.GPIO_Pin=GPIO_Pin_0; io.GPIO_PuPd=GPIO_PuPd_DOWN; GPIO_Init(GPIOA,&io);
    for(i=0;i<3;i++) {
        candidate[i]=stable[i]=Released(i);
        armed[i]=stable[i]; count[i]=0; presses[i]=0;
    }
    SysTick_Config(SystemCoreClock/1000u);
}
void Key_Tick1ms(void)
{
    uint8_t i,raw;
    millis++;
    Buzzer_Tick1ms();
    for(i=0;i<3;i++) {
        raw=Released(i);
        if(raw!=candidate[i]) { candidate[i]=raw; count[i]=1; }
        else if(count[i]<20) count[i]++;
        if(count[i]>=20 && stable[i]!=candidate[i]) {
            stable[i]=candidate[i];
            if(stable[i]) armed[i]=1;
            else if(armed[i]) {
                if(presses[i]<255) presses[i]++;
                armed[i]=0;
            }
        }
    }
}
static uint8_t TakePress(uint8_t index)
{
    uint32_t mask=__get_PRIMASK();
    uint8_t result=0;
    __disable_irq();
    if(presses[index]) { presses[index]--; result=1; }
    __set_PRIMASK(mask);
    return result;
}
uint8_t Key_TakePress(void) { return TakePress(0); }
uint8_t Key1_TakePress(void) { return TakePress(1); }
uint8_t KeyUp_TakePress(void) { return TakePress(2); }
uint32_t Key_Millis(void) { return millis; }
