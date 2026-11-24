#include "lcd.h"
#include "lcd_font.h"
#include "lcd_hz.h"
#include "key.h"

#define SCL GPIO_Pin_5
#define SDA GPIO_Pin_6
#define RST GPIO_Pin_7
#define DC  GPIO_Pin_8
#define CS  GPIO_Pin_9
#define BL  GPIO_Pin_10
#define LOW(pin)  GPIO_ResetBits(GPIOB, (pin))
#define HIGH(pin) GPIO_SetBits(GPIOB, (pin))

void LCD_DelayMs(uint32_t ms)
{
    uint32_t start=Key_Millis();
    /* Use the interrupt tick; debug trace counters need not run at power-on. */
    while((uint32_t)(Key_Millis()-start)<ms) { }
}

/* Atomic writes change only LCD pins; PB11/PB12 IMU pins are unaffected.
 * Keep SPL for peripheral setup, avoid a GPIO function call for every pixel bit. */
#define LCD_BSRR (*(__IO uint32_t *)(GPIOB_BASE+0x18u))
#define SEND_BIT(mask) do { \
    LCD_BSRR=((uint32_t)SCL<<16)|((value&(mask))?SDA:((uint32_t)SDA<<16)); \
    LCD_BSRR=SCL; \
} while(0)
static __forceinline void SendByte(uint8_t value)
{
    SEND_BIT(0x80); SEND_BIT(0x40); SEND_BIT(0x20); SEND_BIT(0x10);
    SEND_BIT(0x08); SEND_BIT(0x04); SEND_BIT(0x02); SEND_BIT(0x01);
    LCD_BSRR=(uint32_t)SCL<<16;
}

static void Command(uint8_t cmd)
{
    LOW(DC); LOW(CS); SendByte(cmd); HIGH(CS);
}

static void Data(uint8_t value)
{
    HIGH(DC); LOW(CS); SendByte(value); HIGH(CS);
}

static void Register(uint8_t cmd, const uint8_t *data, uint8_t count)
{
    Command(cmd);
    while (count--) Data(*data++);
}

static void Area(uint16_t x,uint16_t y,uint16_t w,uint16_t h)
{
    uint16_t xe=x+w-1,ye=y+h-1;
    uint8_t columns[4],rows[4];
    columns[0]=x>>8; columns[1]=x; columns[2]=xe>>8; columns[3]=xe;
    rows[0]=y>>8; rows[1]=y; rows[2]=ye>>8; rows[3]=ye;
    Register(0x2A, columns, sizeof(columns));
    Register(0x2B, rows, sizeof(rows));
    Command(0x2C);
    HIGH(DC); LOW(CS);
}
static void Window(void) { Area(0,0,LCD_WIDTH,LCD_HEIGHT); }

static __forceinline void Pixel(uint32_t rgb)
{
    /* Match supplier example: COLMOD=0x66, RGB666, three bytes/pixel. */
    SendByte((uint8_t)(rgb >> 16) & 0xFCu);
    SendByte((uint8_t)(rgb >> 8) & 0xFCu);
    SendByte((uint8_t)rgb & 0xFCu);
}

void LCD_Init(void)
{
    GPIO_InitTypeDef io;
    const uint8_t porch[] = {0x0C,0x0C,0x00,0x33,0x33};
    const uint8_t power[] = {0xA6,0xA1};
    const uint8_t gamma[] = {0xD0,0x0D,0x14,0x0B,0x0B,0x07,0x3A,
                             0x44,0x50,0x08,0x13,0x13,0x2D,0x32};
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
    GPIO_StructInit(&io);
    io.GPIO_Pin = SCL | SDA | RST | DC | CS | BL;
    io.GPIO_Mode = GPIO_Mode_OUT;
    io.GPIO_OType = GPIO_OType_PP;
    io.GPIO_Speed = GPIO_Speed_25MHz;
    io.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOB, &io);
    LOW(BL); HIGH(CS); LOW(SCL); LOW(SDA); HIGH(DC);
    LOW(RST); LCD_DelayMs(10);
    HIGH(RST); LCD_DelayMs(120);
    Command(0x11); LCD_DelayMs(120);
    /* Panel-specific voltage/gamma values preserved from supplier sample. */
    Register(0xB2, porch, sizeof(porch));
    Command(0x20);
    Command(0xB7); Data(0x56);
    Command(0xBB); Data(0x18);
    Command(0xC0); Data(0x2C);
    Command(0xC2); Data(0x01);
    Command(0xC3); Data(0x1F);
    Command(0xC4); Data(0x20);
    Command(0xC6); Data(0x0F);
    Register(0xD0, power, sizeof(power));
    Register(0xE0, gamma, sizeof(gamma));
    Register(0xE1, gamma, sizeof(gamma));
    Command(0x36); Data(0x00);
    Command(0x3A); Data(0x66);
    Command(0xE7); Data(0x00);
    Command(0x21);
    Command(0x29); LCD_DelayMs(20);
    LCD_Fill(0);
    HIGH(BL);
}

void LCD_Fill(uint32_t rgb)
{
    uint32_t n;
    Window();
    for (n = 0; n < (uint32_t)LCD_WIDTH * LCD_HEIGHT; n++) Pixel(rgb);
    HIGH(CS);
}

void LCD_TestPattern(uint32_t phase)
{
    uint16_t x, y;
    uint32_t color;
    static const uint32_t bars[] = {0xFF0000,0x00FF00,0x0000FF,
                                    0xFFFF00,0x00FFFF,0xFF00FF};
    Window();
    for (y = 0; y < LCD_HEIGHT; y++) {
        for (x = 0; x < LCD_WIDTH; x++) {
            color = bars[x / 40u];
            if (y >= 260u) color = (phase & 1u) ? 0xFFFFFFu : 0;
            if (x < 3u || x >= 237u || y < 3u || y >= 317u) color = 0xFFFFFFu;
            Pixel(color);
        }
    }
    HIGH(CS);
}

/* Reuse reference OLED font: 8 columns and two vertical 8-bit pages. */
void LCD_Text(uint16_t x,uint16_t y,const char *text,uint32_t fg,uint32_t bg,uint8_t scale)
{
    uint16_t row,col;
    uint8_t ch,sr,sc,on;
    if(scale<1||scale>2||y+16u*scale>LCD_HEIGHT) return;
    while(*text&&x+8u*scale<=LCD_WIDTH) {
        ch=(uint8_t)*text++; if(ch<32||ch>126) ch='?';
        Area(x,y,8u*scale,16u*scale);
        for(row=0;row<16u*scale;row++) {
            sr=row/scale;
            for(col=0;col<8u*scale;col++) {
                sc=col/scale;
                on=OLED_F8x16[ch-32][sc+(sr/8u)*8u]&(1u<<(sr%8u));
                Pixel(on?fg:bg);
            }
        }
        HIGH(CS); x+=8u*scale;
    }
}

uint16_t LCD_DecodeUTF8(const char **text)
{
    const uint8_t *p=(const uint8_t *)*text;
    uint16_t code;
    if(!p[0]) return 0;
    if(p[0]<0x80) { *text+=1; return p[0]; }
    if(p[0]>=0xC2 && p[0]<=0xDF && p[1] && (p[1]&0xC0)==0x80) {
        code=((p[0]&31)<<6)|(p[1]&63); *text+=2; return code;
    }
    if(p[0]>=0xE0 && p[0]<=0xEF && p[1] && p[2] &&
       (p[1]&0xC0)==0x80 && (p[2]&0xC0)==0x80) {
        code=((p[0]&15)<<12)|((p[1]&63)<<6)|(p[2]&63);
        *text+=3; return code;
    }
    *text+=1; return '?';
}

void LCD_Glyph16(uint16_t x,uint16_t y,uint16_t code,uint32_t fg,uint32_t bg)
{
    unsigned i,row,col;
    const uint8_t *bits=0;
    char ascii[2];
    if(code<128) {
        ascii[0]=(char)code; ascii[1]=0;
        LCD_Text(x,y,ascii,fg,bg,1); return;
    }
    if(x+16u>LCD_WIDTH || y+16u>LCD_HEIGHT) return;
    for(i=0;i<LCD_HZ_COUNT;i++) if(lcd_hz[i].code==code) { bits=lcd_hz[i].bits; break; }
    Area(x,y,16,16);
    for(row=0;row<16;row++) for(col=0;col<16;col++) {
        uint8_t on=bits?(bits[row*2+col/8]&(0x80u>>(col%8))):
                   (row==0 || row==15 || col==0 || col==15);
        Pixel(on?fg:bg);
    }
    HIGH(CS);
}
