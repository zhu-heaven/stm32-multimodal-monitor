#include "stm32f4xx.h"
/* Reset clock is the internal 16 MHz HSI. No board crystal assumption. */
uint32_t SystemCoreClock = 16000000u;
void SystemInit(void)
{
    SCB->CPACR |= (0xFu << 20);
    SCB->VTOR = FLASH_BASE;
}
void SystemCoreClockUpdate(void)
{
    RCC_ClocksTypeDef clocks;
    RCC_GetClocksFreq(&clocks);
    SystemCoreClock = clocks.HCLK_Frequency;
}
