#ifndef LCD_H
#define LCD_H
#include "stm32f4xx.h"
#define LCD_WIDTH 240u
#define LCD_HEIGHT 320u
/* PB5=SCL, PB6=SDA, PB7=RST, PB8=DC, PB9=CS, PB10=BL */
void LCD_Init(void);
void LCD_DelayMs(uint32_t ms);
void LCD_Fill(uint32_t rgb);
void LCD_TestPattern(uint32_t phase);
void LCD_Text(uint16_t x,uint16_t y,const char *text,uint32_t fg,uint32_t bg,uint8_t scale);
uint16_t LCD_DecodeUTF8(const char **text);
void LCD_Glyph16(uint16_t x,uint16_t y,uint16_t code,uint32_t fg,uint32_t bg);
#endif
