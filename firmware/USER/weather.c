#include "risk.h"
#include "weather.h"
#include "pc_serial.h"
#include "modem.h"
#include "key.h"
#include "display.h"
#include <stdio.h>
#include <string.h>
#pragma O2
#pragma Ospace

static struct {
    int temperature10,humidity,wind10,code;
    uint32_t observed,sent,received;
    uint8_t valid,dirty,cloud;
} weather;
static uint32_t nowMs,sequence;
static uint8_t selectedPage;
static struct {
    int total10,mean10;
    uint32_t issued,sent,received;
    uint8_t valid;
} rain3;

static const char *ConditionChinese(int code);
static uint8_t RainValid(void)
{
    uint32_t elapsed=(nowMs-rain3.received)/1000u;
    return rain3.valid && elapsed+rain3.sent-rain3.issued<=3600UL &&
           (rain3.sent+elapsed+28800UL)/86400UL==(rain3.issued+28800UL)/86400UL;
}
static void Tenths(char *out,int value)
{
    unsigned magnitude=(unsigned)(value<0?-value:value);
    sprintf(out,"%s%u.%u",value<0?"-":"",magnitude/10,magnitude%10);
}
static int HexDigit(char c){return c>='0'&&c<='9'?c-'0':c>='A'&&c<='F'?c-'A'+10:c>='a'&&c<='f'?c-'a'+10:-1;}
static int SixValues(const char *p,long v[6]){unsigned i,n;int sign;unsigned long a;for(i=0;i<6;i++){a=n=0;sign=1;if(*p=='-'){sign=-1;p++;}while(*p>='0'&&*p<='9'){if(++n>10||a>214748364UL||(a==214748364UL&&*p>'7'))return 0;a=a*10+(unsigned)(*p++-'0');}if(!n)return 0;v[i]=sign*(long)a;if(i<5){if(*p++!=',')return 0;}else if(*p)return 0;}return 1;}
static int Accept(char *text)
{
    char *star=strchr(text,'*');
    unsigned checksum=0,expected;
    unsigned long observed,sent;
    int temp,humidity,wind,code;
    long values[6];
    int h,l;
    char *p;
    if(!star || strlen(star)!=3) return 0; h=HexDigit(star[1]);l=HexDigit(star[2]);if(h<0||l<0)return 0;expected=(unsigned)(h*16+l);
    for(p=text;p<star;p++) checksum^=(unsigned char)*p;
    if(checksum!=expected) return 0;
    *star=0;
    if(!strncmp(text,"WX3C,",5)) {
        int a,b,c;
        unsigned long start;
        if(!SixValues(text+5,values))return 0; a=values[0];b=values[1];c=values[2];start=values[3];observed=values[4];sent=values[5];
        if(a<0 || a>20000 || b<0 || b>20000 || c<0 || c>20000 ||
           observed<1577836800UL || sent<observed || sent-observed>3600UL ||
           start!=((sent+28800UL)/86400UL-1)*86400UL-28800UL ||
           (observed+28800UL)/86400UL!=(sent+28800UL)/86400UL) return 0;
        rain3.total10=a+b+c; rain3.mean10=(rain3.total10+1)/3;
        rain3.issued=(uint32_t)observed; rain3.sent=(uint32_t)sent;
        rain3.received=nowMs; rain3.valid=1;
        weather.dirty=1;
        return 1;
    }
    if(strncmp(text,"WX,",3)||!SixValues(text+3,values))return 0;temp=values[0];humidity=values[1];wind=values[2];code=values[3];observed=values[4];sent=values[5];
    if(temp < -800 || temp>650 || humidity<0 || humidity>100 || wind<0 ||
       wind>1500 || code<0 || code>99 || observed<1577836800UL ||
       sent<observed || sent-observed>3600UL) return 0;
    if(!weather.valid || weather.temperature10!=temp || weather.humidity!=humidity ||
       weather.wind10!=wind || weather.code!=code || weather.observed!=observed)
        weather.dirty=1;
    weather.temperature10=temp; weather.humidity=humidity;
    weather.wind10=wind; weather.code=code;
    weather.observed=(uint32_t)observed; weather.sent=(uint32_t)sent;
    weather.received=nowMs; weather.valid=1;
    return 1;
}
static void Publish(void)
{
    static char payload[576];
    char temp[16],wind[16],when[24],*end;
    uint32_t local=weather.observed+8UL*3600UL;
    uint32_t days=local/86400UL,year=1970,month=1,span;
    static const uint8_t monthDays[]={31,28,31,30,31,30,31,31,30,31,30,31};
    /* Gregorian calendar, UTC+8; retain the Unix value for machine processing. */
    for(;;) {
        span=(year%4==0 && (year%100!=0 || year%400==0))?366:365;
        if(days<span) break;
        days-=span; year++;
    }
    for(;;) {
        span=monthDays[month-1];
        if(month==2 && year%4==0 && (year%100!=0 || year%400==0)) span++;
        if(days<span) break;
        days-=span; month++;
    }
    sprintf(when,"%04lu-%02lu-%02lu %02lu:%02lu:%02lu",
            (unsigned long)year,(unsigned long)month,(unsigned long)(days+1),
            (unsigned long)((local/3600)%24),(unsigned long)((local/60)%60),
            (unsigned long)(local%60));
    Tenths(temp,weather.temperature10); Tenths(wind,weather.wind10);
    sprintf(payload,"{\"id\":\"%lu\",\"version\":\"1.0\",\"params\":{"
            "\"weather_city\":{\"value\":\"\351\225\277\346\262\231\"}," /* \"weather_city\":{\"value\":\"长沙\"}, */
            "\"weather_text\":{\"value\":\"%s\"},"
            "\"weather_temp\":{\"value\":%s},"
            "\"weather_humidity\":{\"value\":%d},"
            "\"weather_wind\":{\"value\":%s},"
            "\"weather_code\":{\"value\":%d},"
            "\"weather_time_text\":{\"value\":\"%s\"}",
            (unsigned long)(1000000u+sequence++),ConditionChinese(weather.code),temp,weather.humidity,
            wind,weather.code,when);
    end=payload+strlen(payload);
    if(RainValid()) {
        Tenths(temp,rain3.total10); Tenths(wind,rain3.mean10);
        end+=sprintf(end,",\"forecast_rain_3d\":{\"value\":%s},\"forecast_rain_avg\":{\"value\":%s}",temp,wind);
    }
    sprintf(end,"}}");
    Modem_Send(payload);
    PCSerial_Send("WX_PUBLISHED\r\n");
    weather.dirty=0;
}
void Weather_Update(void)
{
    char input[96];
    nowMs=Key_Millis();
    while(PCSerial_ReadLine(input,sizeof(input))) {
        if(!strcmp(input,"WXCLOUD,1")) {
            weather.cloud=1; weather.dirty=1; PCSerial_Send("WX_CLOUD=1\r\n");
        } else if(!strcmp(input,"WXCLOUD,0")) {
            weather.cloud=0; PCSerial_Send("WX_CLOUD=0\r\n");
        } else if(!strncmp(input,"WX3C,",5)) {
            PCSerial_Send(Accept(input)?"WX3_OK\r\n":"WX3_REJECTED\r\n");
        } else if(!strncmp(input,"WX,",3)) {
            PCSerial_Send(Accept(input)?"WX_OK\r\n":"WX_REJECTED\r\n");
        }
    }
    /* Separate weather posts cannot block regular sensor property validation. */
    if(weather.valid && weather.cloud && weather.dirty &&
       (uint32_t)(nowMs-weather.received)<1200000u) Publish();
}
uint8_t Weather_Page(void)
{
    return selectedPage;
}
int32_t Weather_Rain3d10(void) { return RainValid()?rain3.total10:-1; }
void Weather_NextPage(void) { if(++selectedPage>=3) selectedPage=0; }
static const char *ConditionChinese(int code)
{
    if(code==0) return "\346\231\264" /* 晴 */;
    if(code<=3) return "\345\244\232\344\272\221" /* 多云 */;
    if(code==45 || code==48) return "\351\233\276" /* 雾 */;
    if(code>=51 && code<=57) return "\346\257\233\346\257\233\351\233\250" /* 毛毛雨 */;
    if(code>=61 && code<=67) return "\351\233\250" /* 雨 */;
    if(code>=71 && code<=77) return "\351\233\252" /* 雪 */;
    if(code>=80 && code<=82) return "\351\230\265\351\233\250" /* 阵雨 */;
    if(code==85 || code==86) return "\351\230\265\351\233\252" /* 阵雪 */;
    if(code>=95) return "\351\233\267\346\232\264" /* 雷暴 */;
    return "\346\234\252\347\237\245" /* 未知 */;
}
void Weather_Draw(void (*line)(uint16_t,const char *,uint32_t))
{
    char b[64],value[20];
    uint32_t age,local;
    uint8_t rainOk;
    line(0,"\351\225\277\346\262\231\345\244\251\346\260\224                   2/3" /* 长沙天气                   2/3 */,0x50D8FF);
    line(24,"\351\231\215\346\260\264\350\214\203\345\233\264\357\274\232\346\230\250\345\244\251\040\344\273\212\345\244\251\040\346\230\216\345\244\251" /* 降水范围：昨天 今天 明天 */,0x90A8B8);
    if(!weather.valid) {
        line(48,"\346\255\243\345\234\250\347\255\211\345\276\205\345\244\251\346\260\224\346\225\260\346\215\256" /* 正在等待天气数据 */,0xFFD060);
        line(72,"\350\257\267\345\220\257\345\212\250\347\224\265\350\204\221\345\244\251\346\260\224\350\275\254\345\217\221\345\267\245\345\205\267" /* 请启动电脑天气转发工具 */,0xD0E8FF);
        line(96,"\345\271\266\350\277\236\346\216\245 USB \350\275\254\344\270\262\345\217\243" /* 并连接 USB 转串口 */,0xD0E8FF);
        line(120,"\346\216\245\346\224\266\345\274\225\350\204\232 PA10" /* 接收引脚 PA10 */,0x90A8B8);
        line(144,"\344\270\262\345\217\243\345\217\202\346\225\260 115200 / 8N1" /* 串口参数 115200 / 8N1 */,0x90A8B8);
        line(168,"",0x90A8B8); line(192,"",0x90A8B8);
        line(216,"",0x90A8B8);
        line(240,"\346\235\245\346\272\220 Open-Meteo" /* 来源 Open-Meteo */,0x90A8B8);
    } else {
        age=(nowMs-weather.received)/1000u+weather.sent-weather.observed;
        local=weather.observed+8UL*3600UL;
        sprintf(b,"\351\225\277\346\262\231/%s" /* 长沙/%s */,ConditionChinese(weather.code));
        Display_Row(48,"\345\244\251\346\260\224" /* 天气 */,b,0xFFE080);
        Tenths(value,weather.temperature10); sprintf(b,"%s\342\204\203" /* %s℃ */,value);
        Display_Row(72,"\346\260\224\346\270\251" /* 气温 */,b,0xFFE080);
        sprintf(b,"%d%%",weather.humidity);
        Display_Row(96,"\347\251\272\346\260\224\346\271\277\345\272\246" /* 空气湿度 */,b,0x40D8FF);
        Tenths(value,weather.wind10); sprintf(b,"%s \347\261\263/\347\247\222" /* %s 米/秒 */,value);
        Display_Row(120,"\351\243\216\351\200\237" /* 风速 */,b,0x60E0A0);
        rainOk=RainValid();
        if(rainOk) { Tenths(value,rain3.total10); sprintf(b,"%s mm",value); }
        else sprintf(b,"--");
        Display_Row(144,"\344\270\211\346\227\245\347\264\257\350\256\241" /* 三日累计 */,b,0x40D8FF);
        if(rainOk) { Tenths(value,rain3.mean10); sprintf(b,"%s mm/\346\227\245" /* %s mm/日 */,value); }
        else sprintf(b,"--");
        Display_Row(168,"\346\227\245\345\235\207\351\231\215\346\260\264" /* 日均降水 */,b,0x60E0A0);
        sprintf(b,"%u%%",Risk_Weight(4));
        Display_Row(192,"\351\231\215\346\260\264\346\235\203\351\207\215" /* 降水权重 */,b,0xD0E8FF);
        if(age>1800) sprintf(b,"\345\267\262\350\277\207\346\234\237" /* 已过期 */);
        else sprintf(b,"%02lu:%02lu",(unsigned long)((local/3600)%24),(unsigned long)((local/60)%60));
        Display_Row(216,"\345\214\227\344\272\254\346\227\266\351\227\264" /* 北京时间 */,b,age>1800?0xFF7070:0x90A8B8);
        Display_Row(240,"\346\235\245\346\272\220" /* 来源 */,"Open-Meteo",0x90A8B8);
    }
    line(264,"\345\244\251\346\260\224\346\216\245\345\217\243\351\242\204\346\212\245 \351\235\236\347\216\260\345\234\272\345\256\236\346\265\213" /* 历史与预报混合 非现场实测 */,0x90A8B8);
    line(296,"K0 \345\210\207\346\215\242\351\241\265\351\235\242   K1 \350\234\202\351\270\243\345\231\250" /* K0 切换页面   K1 蜂鸣器 */,0x90A8B8);
}
