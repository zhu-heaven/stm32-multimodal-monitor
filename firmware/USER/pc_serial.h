#ifndef PC_SERIAL_H
#define PC_SERIAL_H

/* USART1: PA9 TX -> USB-TTL RXD; PA10 RX <- USB-TTL TXD; common GND.
 * 3.3 V TTL, 115200 baud, 8 data bits, no parity, 1 stop bit. */
void PCSerial_Init(void);
void PCSerial_Send(const char *text);
/* Returns a complete ASCII line; malformed/overflowed lines are discarded. */
int PCSerial_ReadLine(char *text,unsigned capacity);

#endif
