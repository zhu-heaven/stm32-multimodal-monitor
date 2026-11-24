#include "modem.h"
#include "project_config.h"
#include "stm32f4xx_usart.h"
#include <stdio.h>
#include <string.h>
#include "key.h"
#include "buzzer.h"
#include "pc_serial.h"
#include "cloud_command.h"
#include "risk.h"
#if defined(__CC_ARM)
#pragma O2
#endif

/* M100MG DTU firmware: fixed UART, no automatic AT probes.
 * DTU's configured UART must also be 115200, 8N1, no flow control.
 * RX counts only data received; zero does not indicate a DTU fault.
 * TX carries the OneJSON business payload; the DTU owns the MQTT session.
 */
#define DTU_BAUD 115200u
static volatile uint32_t received,errors;
static volatile uint32_t txCount;
static volatile uint8_t lastByte;
static volatile uint8_t rx[1024],overflow;
static volatile uint16_t head,tail;
static uint8_t controlChanged;
static uint32_t lastTx;
static const char replyTopic[]="$sys/" MONITOR_PRODUCT_ID "/" MONITOR_DEVICE_NAME "/thing/property/set_reply";

void USART2_IRQHandler(void)
{
    uint32_t status=USART2->SR;
    uint8_t value;
    uint16_t next;
    if(status & (USART_SR_RXNE|USART_SR_ORE|USART_SR_FE|USART_SR_NE|USART_SR_PE)) {
        value=(uint8_t)USART2->DR;
        if(status & (USART_SR_ORE|USART_SR_FE|USART_SR_NE|USART_SR_PE)) {
            errors++; overflow=1; return;
        }
        lastByte=value; received++;
        next=(uint16_t)((head+1u)&1023u);
        if(next==tail) {errors++; overflow=1;}
        else {rx[head]=value; head=next;}
    }
}

void Modem_Init(void)
{
    GPIO_InitTypeDef io;
    USART_InitTypeDef uart;
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA,ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2,ENABLE);
    GPIO_StructInit(&io);
    io.GPIO_Pin=GPIO_Pin_2|GPIO_Pin_3; io.GPIO_Mode=GPIO_Mode_AF;
    io.GPIO_OType=GPIO_OType_PP; io.GPIO_PuPd=GPIO_PuPd_UP;
    io.GPIO_Speed=GPIO_Speed_25MHz;
    GPIO_Init(GPIOA,&io);
    GPIO_PinAFConfig(GPIOA,GPIO_PinSource2,GPIO_AF_USART2);
    GPIO_PinAFConfig(GPIOA,GPIO_PinSource3,GPIO_AF_USART2);
    USART_StructInit(&uart);
    uart.USART_BaudRate=DTU_BAUD;
    uart.USART_WordLength=USART_WordLength_8b;
    uart.USART_StopBits=USART_StopBits_1;
    uart.USART_Parity=USART_Parity_No;
    uart.USART_HardwareFlowControl=USART_HardwareFlowControl_None;
    uart.USART_Mode=USART_Mode_Rx|USART_Mode_Tx;
    USART_Init(USART2,&uart);
    USART_Cmd(USART2,ENABLE);
    USART_ITConfig(USART2,USART_IT_RXNE,ENABLE);
    NVIC_SetPriority(USART2_IRQn,6); NVIC_EnableIRQ(USART2_IRQn);
}

/* Wait for the transmit register, then send one byte over USART2. */
static void Modem_PutChar(char c)
{
    while(!(USART2->SR & USART_SR_TXE)) { }
    USART2->DR=(uint8_t)c;
    txCount++;
}

/* Blocking send of a NUL-terminated string.
 * In DTU transparent mode this is the OneJSON payload; the DTU wraps it
 * into MQTT and publishes on the configured topic. */
void Modem_Send(const char *text)
{
    while((uint32_t)(Key_Millis()-lastTx)<40u) { }
    while(text && *text) Modem_PutChar(*text++);
    Modem_PutChar('\r'); Modem_PutChar('\n');
    lastTx=Key_Millis();
}

uint32_t Modem_TxCount(void)
{
    return txCount;
}

void Modem_Update(void)
{
    static char frame[768],lastId[14];
    static uint16_t length,depth;
    static uint8_t quoted,escape,discard,lastOn;
    static uint32_t frameByteMs;
    char ch,id[14],reply[160],config[192];
    unsigned char on=0;
    int parsed;
    if(depth && head==tail && (uint32_t)(Key_Millis()-frameByteMs)>200u) {
        length=depth=0; quoted=escape=discard=0;
    }
    if(overflow) {
        uint32_t mask=__get_PRIMASK();
        __disable_irq(); tail=head; overflow=0; __set_PRIMASK(mask);
        length=depth=0; quoted=escape=discard=0;
        PCSerial_Send("CLOUD_RX_OVERFLOW\r\n");
    }
    while(tail!=head) {
        ch=(char)rx[tail]; tail=(uint16_t)((tail+1u)&1023u);
        frameByteMs=Key_Millis();
        if(!depth) {
            if(ch!='{') continue; /* Optional DTU topic prefix is not JSON. */
            length=0; quoted=escape=discard=0;
        }
        if(length<sizeof(frame)-1) frame[length++]=ch; else discard=1;
        if(quoted) {
            if(escape) escape=0;
            else if(ch=='\\') escape=1;
            else if(ch=='"') quoted=0;
        } else if(ch=='"') quoted=1;
        else if(ch=='{') depth++;
        else if(ch=='}') depth--;
        if(depth) continue;
        if(discard) continue;
        frame[length]=0;
        parsed=CloudCommand_Parse(frame,id,&on,config);
        if(!parsed) continue;
        if(parsed==1 && Buzzer_AlarmLevel()) {
            parsed=-2; controlChanged=1; PCSerial_Send("MANUAL_BLOCKED\r\n");
        }
        if(parsed==2) {
            if(!Risk_ApplyConfig(config)) parsed=-1;
            else {controlChanged=1;PCSerial_Send("RISK_CONFIG_OK\r\n");}
        }
        if(parsed==1) {
            if(strcmp(lastId,id)) {
                Buzzer_Set(on); strcpy(lastId,id); lastOn=on;
                controlChanged=1;
                PCSerial_Send(on?"CLOUD BUZZER=ON\r\n":"CLOUD BUZZER=OFF\r\n");
            } else if(on!=lastOn) parsed=-1;
            else controlChanged=1; /* Duplicate ID: report actual state, do not toggle. */
        }
        sprintf(reply,"%s,{\"id\":\"%s\",\"code\":%u,\"msg\":\"%s\"}",
                replyTopic,id,parsed>0?200:parsed==-2?409:400,
                parsed>0?"success":parsed==-2?"automatic alarm active":"invalid params");
        Modem_Send(reply);
    }
}
uint8_t Modem_TakeControlChanged(void)
{ uint8_t result=controlChanged; controlChanged=0; return result; }

void Modem_Status(char *text)
{
    /* At most 27 characters plus NUL, within the 31-byte caller buffer. */
    sprintf(text,"DTU R%lu E%lu T%lu B%02X",
            (unsigned long)(received%10000),(unsigned long)(errors%10000),
            (unsigned long)(txCount%10000),lastByte);
}
