#include "stm32f4xx.h"
#include "lcd.h"
#include "mpu6050.h"
#include "position.h"
#include "modem.h"
#include "pc_serial.h"
#include "weather.h"
#include "key.h"
#include "buzzer.h"
#include "tilt.h"
#include "display.h"
#include "soil.h"
#include "risk.h"
#include <stdio.h>
/* Compact application formatting/control code for the 32KB Keil limit.
 * GPIO timing and peripheral drivers are separate translation units. */
#pragma O2
#pragma Ospace

/* Provisional dry references from user's photo; actual dry state unconfirmed.
 * Wet endpoints still from reference example. Requires two-point calibration. */
#define RAIN_ADC_DRY 4080L
#define RAIN_ADC_WET 500L
#if RAIN_ADC_DRY == RAIN_ADC_WET
#error Dry and wet calibration values must differ
#endif

/* Hardware-timed upload period; serviced at the next completed display loop. */
#define REPORT_MS 1000u
static uint32_t reportSeq;

static int16_t temperature10;
static uint16_t positionRaw;
static int32_t position100=-1;
static int32_t rainPercent10=-1;


static void FormatAngle(char *text,int32_t value,uint8_t valid)
{
    uint32_t a=(uint32_t)(value<0?-value:value);
    if(valid) sprintf(text,"%s%lu.%02lu\302\260" /* %s%lu.%02lu° */,value<0?"-":"",(unsigned long)(a/100),(unsigned long)(a%100));
    else sprintf(text,"\347\255\211\345\276\205\346\234\211\346\225\210\346\225\260\346\215\256" /* 等待有效数据 */);
}
static long WetPercent10(uint16_t raw,long dry,long wet)
{
    long value=((long)raw-dry)*1000L/(wet-dry);
    if(value<0) value=0;
    if(value>1000) value=1000;
    return value;
}
static const char *BuzzerText(void)
{
    uint8_t mode=Buzzer_Mode();
    return mode==2?"\346\214\201\347\273\255\345\223\215" /* 持续响 */:mode==1?"\351\227\264\346\255\207\345\223\215" /* 间歇响 */:"\351\235\231\351\237\263" /* 静音 */;
}
static const char *RiskGrade(const Risk_Result *r)
{
    return !r->baseline?"\345\276\205\350\256\276\345\237\272\345\207\206" /* 待设基准 */:r->level==2?"\345\215\261\351\231\251" /* 危险 */:r->level==1?"\351\242\204\350\255\246" /* 预警 */:
           !r->valid?"\346\225\260\346\215\256\344\270\215\350\266\263" /* 数据不足 */:r->confirming?"\347\241\256\350\256\244\344\270\255" /* 确认中 */:"\345\256\211\345\205\250" /* 安全 */;
}
static const char *RiskData(const Risk_Result *r)
{
    return r->valid?"\345\256\214\346\225\264" /* 完整 */:!r->baseline?"\346\214\211K-UP\350\256\276\345\237\272\345\207\206" /* 按K-UP设基准 */:"\347\255\211\345\276\205\346\234\211\346\225\210\346\225\260\346\215\256" /* 等待有效数据 */;
}
static void SensorDisplay(uint8_t online)
{
    const Tilt_Result *v=Tilt_Get();
    uint32_t value,a;
    long percent;
    char b[64];
    Display_Line(0,"\344\274\240\346\204\237\345\231\250\347\233\221\346\265\213                 1/3" /* 传感器监测                 1/3 */,0x50D8FF);
    sprintf(b,"4G %lu\345\270\247 \345\247\277\346\200\201%s /5\347\247\222" /* 4G %lu帧 姿态%s /5秒 */,(unsigned long)reportSeq,
            online && v->valid?"\346\234\211\346\225\210" /* 有效 */:"\346\227\240\346\225\210" /* 无效 */);
    Display_Line(24,b,0x90A8B8);
    if(rainPercent10>=0) {
        percent=rainPercent10;
        sprintf(b,"%ld.%ld%%",percent/10,percent%10);
    } else sprintf(b,"\351\207\207\346\240\267\345\244\261\350\264\245" /* 采样失败 */);
    Display_Row(48,"\351\233\250\346\273\264\346\271\277\346\266\246\345\272\246" /* 雨滴湿润度 */,b,0x40D8FF);
    percent=Soil_Percent10();
    if(percent>=0) {
        sprintf(b,"%ld.%ld%%",percent/10,percent%10);
    } else sprintf(b,"\351\207\207\346\240\267\345\244\261\350\264\245" /* 采样失败 */);
    Display_Row(72,"\345\234\237\345\243\244\347\233\270\345\257\271\346\271\277\345\272\246" /* 土壤相对湿度 */,b,0x60E0A0);
    if(position100>=0) {
        value=(uint32_t)position100;
        sprintf(b,"%lu.%02lu mm",(unsigned long)(value/100),(unsigned long)(value%100));
    } else sprintf(b,"\351\207\207\346\240\267\345\244\261\350\264\245" /* 采样失败 */);
    Display_Row(96,"\344\275\215\347\247\273" /* 位移 */,b,0xFFE080);
    FormatAngle(b,v->angle100,online && v->valid);
    Display_Row(120,"\345\220\210\346\210\220\345\200\276\350\247\222" /* 合成倾角 */,b,0xFFE080);
    FormatAngle(b,v->roll100,online && v->valid);
    Display_Row(144,"\346\250\252\346\273\232\350\247\222" /* 横滚角 */,b,0xD0E8FF);
    FormatAngle(b,v->pitch100,online && v->valid);
    Display_Row(168,"\344\277\257\344\273\260\350\247\222" /* 俯仰角 */,b,0xD0E8FF);
    a=(uint32_t)(v->rate1000<0?-v->rate1000:v->rate1000);
    if(online && v->valid && v->rateValid)
        sprintf(b,"%c%lu.%02lu\302\260/\345\210\206" /* %c%lu.%02lu°/分 */,v->rate1000<0?'-':'+',
                (unsigned long)(a/1000),(unsigned long)(a%1000/10));
    else sprintf(b,"\347\255\211\345\276\2055\347\247\222" /* 等待5秒 */);
    Display_Row(192,"\345\200\276\350\247\222\345\217\230\345\214\226\347\216\207" /* 倾角变化率 */,b,0xFFD060);
    a=(uint32_t)(temperature10<0?-temperature10:temperature10);
    if(online) sprintf(b,"%s%lu.%lu\342\204\203",temperature10<0?"-":"",
                       (unsigned long)(a/10),(unsigned long)(a%10));
    else sprintf(b,"--");
    Display_Row(216,"\350\212\257\347\211\207\346\270\251\345\272\246" /* 芯片温度 */,b,0xD0E8FF);
    a=(uint32_t)(v->positionRate100<0?-v->positionRate100:v->positionRate100);
    if(online && v->valid && v->rateValid)
        sprintf(b,"%c%lu.%02lu mm/\345\210\206" /* %c%lu.%02lu mm/分 */,v->positionRate100<0?'-':'+',
                (unsigned long)(a/100),(unsigned long)(a%100));
    else sprintf(b,"\347\255\211\345\276\2055\347\247\222" /* 等待5秒 */);
    Display_Row(240,"\344\275\215\347\247\273\345\217\230\345\214\226\347\216\207" /* 位移变化率 */,b,0x60E0A0);
    Display_Row(264,"\350\234\202\351\270\243\345\231\250" /* 蜂鸣器 */,BuzzerText(),0x90A8B8);
    Display_Line(296,"K0 \345\210\207\346\215\242\351\241\265\351\235\242   K1 \350\234\202\351\270\243\345\231\250" /* K0 切换页面   K1 蜂鸣器 */,0x90A8B8);
}
/* ---- OneJSON reporting (integer-only formatting, no %f) ---- */

/* Append a signed fixed-point with 'digits' decimal digits.
 * v is the value scaled by 10^digits (e.g. 123 for 12.3 when digits=1). */
static char *PutFixed(char *p,int32_t v,uint8_t digits)
{
    uint32_t scale=1,a;
    char sign;
    while(digits--) scale*=10u;
    sign=v<0?'-':0; a=v<0?(uint32_t)(-(int64_t)v):(uint32_t)v;
    if(sign) *p++=sign;
    if(scale>1) {
        p+=sprintf(p,"%lu.%0*lu",(unsigned long)(a/scale),
                   (int)((scale==10)?1:(scale==100)?2:(scale==1000)?3:4),
                   (unsigned long)(a%scale));
    } else {
        p+=sprintf(p,"%lu",(unsigned long)a);
    }
    return p;
}

/* Assemble and send one OneJSON property-post payload to the DTU over USART2. */
static void ReportUpload(void)
{
    MPU6050_Sample s;
    static char buf[1536]; /* Main-loop only; keep the larger packet off the 1KB stack. */
    char *p=buf;
    const Tilt_Result *tilt=Tilt_Get();
    const Risk_Result *risk=Risk_Get();
    uint8_t imuReadOk;
    int32_t rain10,soil10,disp100,accx,accy,accz,gyrx10,gyry10,gyrz10,temp100;
    int32_t posRaw=-1;

    /* Acquire every sensor; a failed channel contributes a safe fallback. */
    rain10=rainPercent10;
    soil10=Soil_Percent10();
    disp100=position100;
    if(position100>=0) posRaw=positionRaw;

    imuReadOk=MPU6050_Read(&s);
    if(!imuReadOk) {
        accx=accy=accz=0; gyrx10=gyry10=gyrz10=0; temp100=-1;
    } else {
        accx=(int32_t)s.ax*1000/2048;
        accy=(int32_t)s.ay*1000/2048;
        accz=(int32_t)s.az*1000/2048;
        gyrx10=(int32_t)s.gx*100/164;
        gyry10=(int32_t)s.gy*100/164;
        gyrz10=(int32_t)s.gz*100/164;
        if(MPU6050_GetID()==0x70) temp100=(int32_t)s.temperature*10000/33387+2100;
        else temp100=(int32_t)s.temperature*100/340+3653;
    }

    /* Build OneJSON. All values are integer-scaled so sprintf never needs %f. */
    p+=sprintf(p,"{\"id\":\"%lu\",\"version\":\"1.0\",\"params\":{",(unsigned long)reportSeq);
    p+=sprintf(p,"\"rain_wetness\":{\"value\":"); p=PutFixed(p,rain10,1);
    p+=sprintf(p,"},\"soil_moisture\":{\"value\":"); p=PutFixed(p,soil10,1);
    p+=sprintf(p,"},\"displacement_mm\":{\"value\":"); p=PutFixed(p,disp100,2);
    if(tilt->valid && tilt->rateValid) {
        p+=sprintf(p,"},\"displacement_avg\":{\"value\":"); p=PutFixed(p,tilt->positionMean100,2);
    }
    p+=sprintf(p,"},\"accel_x\":{\"value\":%ld}",(long)accx);
    p+=sprintf(p,",\"accel_y\":{\"value\":%ld}",(long)accy);
    p+=sprintf(p,",\"accel_z\":{\"value\":%ld}",(long)accz);
    p+=sprintf(p,",\"gyro_x\":{\"value\":"); p=PutFixed(p,gyrx10,1);
    p+=sprintf(p,"},\"gyro_y\":{\"value\":"); p=PutFixed(p,gyry10,1);
    p+=sprintf(p,"},\"gyro_z\":{\"value\":"); p=PutFixed(p,gyrz10,1);
    /* OneNET chip_temp has a 0.1 C step; finer values reject the whole post. */
    p+=sprintf(p,"},\"chip_temp\":{\"value\":"); p=PutFixed(p,temp100/10,1);
    if(imuReadOk && tilt->valid) {
        p+=sprintf(p,"},\"tilt_angle\":{\"value\":"); p=PutFixed(p,tilt->angle100,2);
        p+=sprintf(p,"},\"roll_angle\":{\"value\":"); p=PutFixed(p,tilt->roll100,2);
        p+=sprintf(p,"},\"pitch_angle\":{\"value\":"); p=PutFixed(p,tilt->pitch100,2);
        if(tilt->rateValid) {
            p+=sprintf(p,"},\"tilt_rate\":{\"value\":"); p=PutFixed(p,tilt->rate1000/10,2);
        }
    }
    p+=sprintf(p,"},\"buzzer_switch\":{\"value\":%s}",Buzzer_Mode()?"true":"false");
    p+=sprintf(p,",\"risk_config\":{\"value\":\""); Risk_ConfigText(p); while(*p) p++; p+=sprintf(p,"\"}");
    p+=sprintf(p,",\"tilt_status\":{\"value\":\"%s\"}",!imuReadOk?"\344\274\240\346\204\237\345\231\250\346\234\252\350\277\236\346\216\245" /* 传感器未连接 */:tilt->valid?"\346\225\260\346\215\256\346\234\211\346\225\210" /* 数据有效 */:"\350\277\220\345\212\250\345\271\262\346\211\260" /* 运动干扰 */);
    p+=sprintf(p,",\"tilt_rate_status\":{\"value\":\"%s\"}",imuReadOk && tilt->valid && tilt->rateValid?"\346\225\260\346\215\256\346\234\211\346\225\210" /* 数据有效 */:"\347\255\211\345\276\205\346\234\211\346\225\210\346\225\260\346\215\256" /* 等待有效数据 */);
    if(risk->baseline && tilt->valid && tilt->rateValid) {
        p+=sprintf(p,",\"displacement_change\":{\"value\":"); p=PutFixed(p,risk->displacement100,2);
        p+=sprintf(p,"},\"tilt_change\":{\"value\":"); p=PutFixed(p,risk->angle100,2); p+=sprintf(p,"}");
    }
    if(risk->valid) {
        p+=sprintf(p,",\"risk_score\":{\"value\":"); p=PutFixed(p,risk->score100,2); p+=sprintf(p,"}");
    }
    p+=sprintf(p,",\"risk_level\":{\"value\":\"%s\"},\"risk_data_status\":{\"value\":\"%s\"},\"buzzer_mode\":{\"value\":\"%s\"}}}",
               RiskGrade(risk),RiskData(risk),BuzzerText());
    *p=0;

    Modem_Send(buf);
    /* Reuse the payload buffer for readable PC output of this same sample. */
    p=buf;
    p+=sprintf(p,"SEQ=%lu  RAIN=",(unsigned long)reportSeq);
    p=PutFixed(p,rain10,1);
    p+=sprintf(p,"%%  SOIL="); p=PutFixed(p,soil10,1);
    p+=sprintf(p,"%%  POS="); p=PutFixed(p,disp100,2);
    p+=sprintf(p," mm  POS_ADC=%ld\r\nACC(mg): X=%ld Y=%ld Z=%ld\r\n",
               (long)posRaw,(long)accx,(long)accy,(long)accz);
    p+=sprintf(p,"GYRO(deg/s): X="); p=PutFixed(p,gyrx10,1);
    p+=sprintf(p," Y="); p=PutFixed(p,gyry10,1);
    p+=sprintf(p," Z="); p=PutFixed(p,gyrz10,1);
    p+=sprintf(p,"\r\nIMU_TEMP="); p=PutFixed(p,temp100/10,1);
    p+=sprintf(p," C\r\nK1_RAW=%u BUZZER=%u PA8_OUT=%u\r\n\r\n",
               GPIO_ReadInputDataBit(GPIOE,GPIO_Pin_3),Buzzer_IsOn(),
               GPIO_ReadOutputDataBit(GPIOA,GPIO_Pin_8));
    PCSerial_Send(buf);
    p=buf;
    if(Tilt_Get()->valid) {
        p+=sprintf(p,"TILT(deg)="); p=PutFixed(p,Tilt_Get()->angle100,2);
        p+=sprintf(p," ROLL="); p=PutFixed(p,Tilt_Get()->roll100,2);
        p+=sprintf(p," PITCH="); p=PutFixed(p,Tilt_Get()->pitch100,2);
        if(Tilt_Get()->rateValid) {
            p+=sprintf(p," RATE(deg/min)="); p=PutFixed(p,Tilt_Get()->rate1000,3);
        } else p+=sprintf(p," RATE=WAIT_5S");
        p+=sprintf(p,"\r\n");
    } else sprintf(buf,"TILT=INVALID (motion or sensor error)\r\n");
    PCSerial_Send(buf);
    if(tilt->valid && tilt->rateValid) {
        p=buf; p+=sprintf(p,"POS_RATE(mm/min)=");
        p=PutFixed(p,tilt->positionRate100,2);
        sprintf(p," WINDOW=5S\r\n");
    } else sprintf(buf,"POS_RATE=WAIT_5S\r\n");
    PCSerial_Send(buf);
    {
        const Risk_Result *r=Risk_Get();
        sprintf(buf,"RISK LEVEL=%u VALID=%u BASE=%u CONFIRM=%u SCORE100=%u D100=%ld A100=%ld\r\n",
                r->level,r->valid,r->baseline,r->confirming,r->score100,(long)r->displacement100,(long)r->angle100);
        PCSerial_Send(buf);
    }
    reportSeq++;
}

static void RiskRow(uint16_t y,const char *label,int32_t value,uint8_t digits,const char *unit)
{
    char b[64],*p=b;
    if(value<0) sprintf(b,"--");
    else { p=PutFixed(p,value,digits); sprintf(p,"%s",unit); }
    Display_Row(y,label,b,0xD0E8FF);
}
static void RiskDisplay(void)
{
    const Risk_Result *r=Risk_Get();
    uint8_t paired=Tilt_Get()->valid && Tilt_Get()->rateValid && r->baseline;
    uint32_t color=r->level==2?0xFF6060:r->level==1?0xFFD060:0x60E0A0;
    const char *grade=RiskGrade(r);
    Display_Line(0,"\345\256\236\351\252\214\350\257\204\345\210\206                  3/3" /* 实验评分                  3/3 */,0x50D8FF);
    Display_Line(24,"\345\271\263\345\235\207\344\275\215\347\247\273/\345\220\210\346\210\220\345\200\276\350\247\222\345\257\271\346\257\224\345\237\272\345\207\206" /* 平均位移/合成倾角对比基准 */,0x90A8B8);
    RiskRow(48,"\344\275\215\347\247\273\347\233\270\345\257\271\345\217\230\345\214\226" /* 位移相对变化 */,paired?r->displacement100:-1,2," mm");
    RiskRow(72,"\345\234\237\345\243\244\347\233\270\345\257\271\346\271\277\345\272\246" /* 土壤相对湿度 */,r->soil10,1,"%");
    RiskRow(96,"\351\233\250\346\273\264\346\235\277\346\271\277\346\266\246\345\272\246" /* 雨滴板湿润度 */,r->rain10,1,"%");
    RiskRow(120,"\345\200\276\350\247\222\347\233\270\345\257\271\345\217\230\345\214\226" /* 倾角相对变化 */,paired?r->angle100:-1,2,"\302\260" /* ° */);
    RiskRow(144,"\344\270\211\346\227\245\347\264\257\350\256\241\351\231\215\346\260\264" /* 三日累计降水 */,r->forecast10,1," mm");
    RiskRow(168,"\347\273\274\345\220\210\345\276\227\345\210\206" /* 综合得分 */,r->valid?r->score100:-1,2," /3");
    Display_Row(192,"\350\257\204\345\210\244\347\255\211\347\272\247" /* 评判等级 */,grade,color);
    Display_Row(216,"\346\225\260\346\215\256\347\212\266\346\200\201" /* 数据状态 */,RiskData(r),0x90A8B8);
    Display_Row(240,"\350\234\202\351\270\243\345\231\250" /* 蜂鸣器 */,BuzzerText(),color);
    Display_Line(264,"\345\256\236\351\252\214\351\230\210\345\200\274 \351\235\236\347\216\260\345\234\272\345\256\211\345\205\250\347\273\223\350\256\272" /* 实验阈值 非现场安全结论 */,0x90A8B8);
    Display_Line(296,"K0\345\210\207\351\241\265 K-UP\350\256\276\345\237\272\345\207\206" /* K0切页 K-UP设基准 */,0x90A8B8);
}
int main(void)
{
    MPU6050_Sample s;

    uint8_t weatherPage;
    char b[64]; uint8_t online=0,bootReported=0;
    uint32_t lastReportMs;
    uint16_t rainRaw;
    long t;
    RCC_DeInit();
    SystemCoreClockUpdate();
    Buzzer_Init();
    Risk_Init();
    PCSerial_Init();
    PCSerial_Send("BOOT_START\r\n");
    Key_Init();
    PCSerial_Send("BOOT_KEY_OK\r\n");
    LCD_Init();
    sprintf(b,"BOOT_LCD_MS=%lu K0=PE4 ACTIVE_LOW\r\n",(unsigned long)Key_Millis());
    PCSerial_Send(b);
    Position_Init();
    Modem_Init();
    PCSerial_Send("STM32 SENSORS R12 | USART1 PA9/PA10 | 115200 8N1\r\n"
                  "RAIN/SOIL: relative %, POS: mm, ACC: mg, GYRO: deg/s\r\n\r\n");
    Display_Line(0,"\345\244\232\346\250\241\346\200\201\345\276\256\345\275\242\345\217\230\347\233\221\346\265\213" /* 多模态微形变监测 */,0x50D8FF);
    Display_Line(24,"\346\255\243\345\234\250\345\220\257\345\212\250\344\274\240\346\204\237\345\231\250" /* 正在启动传感器 */,0x90A8B8);
    lastReportMs=Key_Millis();
    while (1) {
        while(Key1_TakePress()) {
            if(Buzzer_AlarmLevel()) PCSerial_Send("MANUAL_BLOCKED\r\n");
            else {
                Buzzer_Toggle();
                PCSerial_Send(Buzzer_Mode()==1?"K1 BUZZER=PULSE\r\n":Buzzer_Mode()==2?"K1 BUZZER=ON\r\n":"K1 BUZZER=OFF\r\n");
            }
        }
        while(Key_TakePress()) {
            Weather_NextPage();
            PCSerial_Send(Weather_Page()==2?"K0 PAGE=RISK\r\n":Weather_Page()?"K0 PAGE=WEATHER\r\n":"K0 PAGE=SENSORS\r\n");
        }
        while(KeyUp_TakePress()) {
            PCSerial_Send(Risk_SetBaseline(Tilt_Get())?"BASELINE_OK\r\n":"BASELINE_WAIT\r\n");
        }
        Weather_Update(); weatherPage=Weather_Page();
        Soil_Update(Key_Millis());
        rainPercent10=Analog_ReadChannel(11,&rainRaw)?WetPercent10(rainRaw,RAIN_ADC_DRY,RAIN_ADC_WET):-1;
        Modem_Update();
        if(Modem_TakeControlChanged()) lastReportMs=Key_Millis()-REPORT_MS;
        if(!online) {
            online=MPU6050_Init();
            if(!online) {
                Risk_Update(Tilt_Get(),Soil_Percent10(),rainPercent10,Weather_Rain3d10());
                if(weatherPage==2) RiskDisplay(); else if(weatherPage) Weather_Draw(Display_Line); else SensorDisplay(0);
                LCD_DelayMs(500); continue;
            }
        }
        if(Position_Read(&positionRaw)) position100=(int32_t)Position_CentiMM(positionRaw);
        else position100=-1;
        if(!MPU6050_Read(&s)) { Tilt_Invalidate(); online=0;
            Risk_Update(Tilt_Get(),Soil_Percent10(),rainPercent10,Weather_Rain3d10());
            if(weatherPage==2) RiskDisplay(); else if(weatherPage) Weather_Draw(Display_Line); else SensorDisplay(0);
            continue; }
        Tilt_Update(s.ax,s.ay,s.az,Key_Millis(),position100);
        Risk_Update(Tilt_Get(),Soil_Percent10(),rainPercent10,Weather_Rain3d10());
        Buzzer_SetAlarm(Risk_Get()->level);

        if(MPU6050_GetID()==0x70) t=(long)s.temperature*10000/33387+2100;
        else t=(long)s.temperature*100/340+3653;
        temperature10=(int16_t)(t/10);
        if(weatherPage==2) RiskDisplay(); else if(weatherPage) Weather_Draw(Display_Line); else SensorDisplay(1);

        /* Unsigned subtraction remains valid across the millisecond wrap. */
        if((uint32_t)(Key_Millis()-lastReportMs)>=REPORT_MS) {
            lastReportMs=Key_Millis();
            ReportUpload();
        }
        if(!bootReported) {
            sprintf(b,"BOOT_FIRST_FRAME_MS=%lu\r\n",(unsigned long)Key_Millis());
            PCSerial_Send(b); bootReported=1;
        }
        LCD_DelayMs(10);
    }
}
