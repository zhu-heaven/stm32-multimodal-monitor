#include "display.h"
#include "lcd.h"
#include <string.h>
#pragma O2
#pragma Ospace

/* Thirty 8-pixel cells; a Chinese glyph occupies two cells. All rows use
 * y multiples of eight and a 16-pixel font. Pages share the same row positions. */
static uint16_t previous[40][30];
static uint32_t colors[40];
static uint8_t painted[40];
void Display_Line(uint16_t y,const char *text,uint32_t color)
{
    uint16_t cells[30],code;
    uint8_t i=0,width,row=(uint8_t)(y/8);
    if(y%8 || y+16>LCD_HEIGHT) return;
    while(*text && i<30) {
        code=LCD_DecodeUTF8(&text); width=code<128?1:2;
        if(i+width>30) break;
        cells[i++]=code;
        if(width==2) cells[i++]=0xFFFF;
    }
    while(i<30) cells[i++]=' ';
    for(i=0;i<30;i++) {
        if(cells[i]==0xFFFF) continue;
        if(!painted[row] || colors[row]!=color || previous[row][i]!=cells[i] ||
           (cells[i]>=128 && previous[row][i+1]!=0xFFFF))
            LCD_Glyph16(i*8u,y,cells[i],color,0);
    }
    memcpy(previous[row],cells,sizeof(cells));
    colors[row]=color; painted[row]=1;
}
void Display_Row(uint16_t y,const char *label,const char *value,uint32_t color)
{
    char text[128];
    const char *p=label;
    unsigned cells=0,n=(unsigned)strlen(label);
    while(*p) cells+=LCD_DecodeUTF8(&p)<128?1:2;
    if(n>70) return;
    memcpy(text,label,n);
    while(cells++<14) text[n++]=' ';
    while(*value && n<sizeof(text)-1) text[n++]=*value++;
    text[n]=0; Display_Line(y,text,color);
}
