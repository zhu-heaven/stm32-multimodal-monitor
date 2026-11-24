#include "stm32f4xx.h"
#include "stm32f4xx_usart.h"
#include "pc_serial.h"
#pragma O2
#pragma Ospace

static volatile uint8_t rx[256],head,tail,overflow;

void USART1_IRQHandler(void)
{
    uint32_t status=USART1->SR;
    uint8_t value,next;
    if(status & (USART_SR_RXNE|USART_SR_ORE|USART_SR_FE|USART_SR_NE|USART_SR_PE)) {
        value=(uint8_t)USART1->DR;
        if(status & (USART_SR_ORE|USART_SR_FE|USART_SR_NE|USART_SR_PE)) {
            overflow=1; return;
        }
        next=(uint8_t)(head+1);
        if(next==tail) overflow=1;
        else { rx[head]=value; head=next; }
    }
}

int PCSerial_ReadLine(char *text,unsigned capacity)
{
    static char line[96];
    static unsigned length;
    static uint8_t discard;
    unsigned i;
    uint8_t ch;
    if(overflow) { overflow=0; discard=1; length=0; }
    while(tail!=head) {
        ch=rx[tail]; tail=(uint8_t)(tail+1);
        if(ch=='\r') continue;
        if(ch=='\n') {
            if(discard) { discard=0; length=0; continue; }
            if(length && length<capacity) {
                for(i=0;i<length;i++) text[i]=line[i];
                text[length]=0; length=0; return 1;
            }
            length=0;
        } else if(!discard) {
            if(ch<32 || ch>126 || length>=sizeof(line)-1) {
                discard=1; length=0;
            } else line[length++]=(char)ch;
        }
    }
    return 0;
}

void PCSerial_Init(void)
{
    GPIO_InitTypeDef io;
    USART_InitTypeDef uart;
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA,ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1,ENABLE);
    GPIO_StructInit(&io);
    io.GPIO_Pin=GPIO_Pin_9|GPIO_Pin_10;
    io.GPIO_Mode=GPIO_Mode_AF;
    io.GPIO_OType=GPIO_OType_PP;
    io.GPIO_PuPd=GPIO_PuPd_UP;
    io.GPIO_Speed=GPIO_Speed_25MHz;
    GPIO_Init(GPIOA,&io);
    GPIO_PinAFConfig(GPIOA,GPIO_PinSource9,GPIO_AF_USART1);
    GPIO_PinAFConfig(GPIOA,GPIO_PinSource10,GPIO_AF_USART1);
    USART_StructInit(&uart);
    uart.USART_BaudRate=115200;
    uart.USART_Mode=USART_Mode_Tx|USART_Mode_Rx;
    uart.USART_HardwareFlowControl=USART_HardwareFlowControl_None;
    USART_Init(USART1,&uart);
    USART_Cmd(USART1,ENABLE);
    USART_ITConfig(USART1,USART_IT_RXNE,ENABLE);
    NVIC_SetPriority(USART1_IRQn,7);
    NVIC_EnableIRQ(USART1_IRQn);
}

void PCSerial_Send(const char *text)
{
    while(text && *text) {
        while(USART_GetFlagStatus(USART1,USART_FLAG_TXE)==RESET) { }
        USART_SendData(USART1,(uint8_t)*text++);
    }
}
