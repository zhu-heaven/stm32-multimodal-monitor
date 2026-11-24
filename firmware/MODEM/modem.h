#ifndef MODEM_H
#define MODEM_H
#include "stm32f4xx.h"
void Modem_Init(void);
void Modem_Update(void);
/* Blocking TX over USART2. In DTU transparent mode this carries the
 * OneJSON business payload; the DTU owns the MQTT session and topic. */
void Modem_Send(const char *text);
uint8_t Modem_TakeControlChanged(void);
/* Total bytes handed to the USART2 TX data register (diagnostic only). */
uint32_t Modem_TxCount(void);
/* Destination must hold at least 31 bytes. */
void Modem_Status(char *text);
#endif
