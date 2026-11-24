#include "mpu6050.h"
#include "lcd.h"
/* PB11=SCL, PB12=SDA; open drain, external pullups to 3.3V. */
#define SCL GPIO_Pin_11
#define SDA GPIO_Pin_12
static uint8_t address=0x68,identity;
static void Wait(void) { volatile uint32_t i; for(i=0;i<40;i++) __NOP(); }
static void Data(uint8_t high)
{ GPIO_WriteBit(GPIOB,SDA,high?Bit_SET:Bit_RESET); Wait(); }
static uint8_t Clock(uint8_t high)
{
    uint32_t timeout=1000;
    GPIO_WriteBit(GPIOB,SCL,high?Bit_SET:Bit_RESET);
    if(high) while(!GPIO_ReadInputDataBit(GPIOB,SCL)) if(!--timeout) return 0;
    Wait(); return 1;
}
static void Stop(void) { Clock(0); Data(0); Clock(1); Data(1); }
static uint8_t Start(void)
{
    Data(1);
    if(!Clock(1)||!GPIO_ReadInputDataBit(GPIOB,SDA)) return 0;
    Data(0); Clock(0); return 1;
}
static uint8_t Send(uint8_t value)
{
    uint8_t i,ack;
    for(i=0;i<8;i++) {
        Data(value&0x80); if(!Clock(1)) return 0;
        Clock(0); value<<=1;
    }
    Data(1); if(!Clock(1)) return 0;
    ack=!GPIO_ReadInputDataBit(GPIOB,SDA); Clock(0); return ack;
}
static uint8_t Receive(uint8_t *value,uint8_t last)
{
    uint8_t i,v=0;
    Data(1);
    for(i=0;i<8;i++) {
        if(!Clock(1)) return 0;
        v=(uint8_t)((v<<1)|GPIO_ReadInputDataBit(GPIOB,SDA)); Clock(0);
    }
    Data(last); if(!Clock(1)) return 0;
    Clock(0); Data(1); *value=v; return 1;
}
static uint8_t WriteReg(uint8_t reg,uint8_t value)
{
    uint8_t ok=Start()&&Send(address<<1)&&Send(reg)&&Send(value);
    Stop(); return ok;
}
static uint8_t ReadRegs(uint8_t reg,uint8_t *buf,uint8_t n)
{
    uint8_t i,ok=Start()&&Send(address<<1)&&Send(reg)&&Start()&&Send((address<<1)|1);
    if(ok) for(i=0;i<n;i++) if(!Receive(&buf[i],i==n-1)) { ok=0; break; }
    Stop(); return ok;
}
uint8_t MPU6050_GetID(void) { return identity; }
uint8_t MPU6050_Init(void)
{
    GPIO_InitTypeDef io;
    uint8_t i,config[2];
    identity=0;
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB,ENABLE);
    GPIO_SetBits(GPIOB,SCL|SDA); GPIO_StructInit(&io);
    io.GPIO_Pin=SCL|SDA; io.GPIO_Mode=GPIO_Mode_OUT;
    io.GPIO_OType=GPIO_OType_OD; io.GPIO_PuPd=GPIO_PuPd_UP;
    io.GPIO_Speed=GPIO_Speed_25MHz; GPIO_Init(GPIOB,&io);
    /* Recover a slave left mid-byte after MCU reset; waits are bounded. */
    Data(1); for(i=0;i<9;i++) { Clock(0); if(!Clock(1)) return 0; }
    Stop(); LCD_DelayMs(100); address=0x68;
    if(!ReadRegs(0x75,&identity,1)) {
        address=0x69; if(!ReadRegs(0x75,&identity,1)) return 0;
    }
    /* WHO_AM_I: MPU6050=0x68; MPU6500=0x70. I2C address is separate. */
    if(identity!=0x68 && identity!=0x70) return 0;
    if(!WriteReg(0x6B,0x80)) return 0;
    LCD_DelayMs(100);
    /* Reference ranges: +/-16g (2048 LSB/g), +/-2000 dps (16.4 LSB/dps). */
    if(!WriteReg(0x6B,1)||!WriteReg(0x6C,0)||!WriteReg(0x19,9)||
       !WriteReg(0x1A,6)||!WriteReg(0x1B,0x18)||!WriteReg(0x1C,0x18)) return 0;
    /* MPU6500 has a separate accelerometer filter register (ACCEL_CONFIG2). */
    if(identity==0x70 && !WriteReg(0x1D,0x06)) return 0;
    LCD_DelayMs(50);
    if(identity==0x70 && (!ReadRegs(0x1D,config,1) || config[0]!=0x06)) return 0;
    return ReadRegs(0x1B,config,2)&&config[0]==0x18&&config[1]==0x18;
}
static int16_t Word(const uint8_t *p)
{ return (int16_t)(((uint16_t)p[0]<<8)|p[1]); }
uint8_t MPU6050_Read(MPU6050_Sample *s)
{
    uint8_t b[14];
    if(!ReadRegs(0x3B,b,14)) return 0;
    s->ax=Word(b); s->ay=Word(b+2); s->az=Word(b+4); s->temperature=Word(b+6);
    s->gx=Word(b+8); s->gy=Word(b+10); s->gz=Word(b+12); return 1;
}
