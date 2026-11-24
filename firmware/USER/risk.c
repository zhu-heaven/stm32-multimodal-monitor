#include "risk.h"
#include "stm32f4xx_flash.h"
#include <stdio.h>
#include <string.h>
#pragma O2
#pragma Ospace
static Risk_Result result;
static int32_t basePosition,baseAngle;
static uint32_t seenWindow;
static uint8_t candidate,confirmCount;
static int32_t Abs(int32_t v) { return v<0?-v:v; }
static uint8_t Points(int32_t v,int32_t a,int32_t b,int32_t c)
{ return v<a?0:v<b?1:v<c?2:3; }

/* Sector 7 is reserved on the 512KB STM32F407VE. Append-only journal:
 * no erase in the control path, CRC and magic commit written last. */
typedef struct {uint32_t revision;uint16_t w[5],edge[15];} Risk_Config;
typedef struct {Risk_Config c;uint32_t crc,magic;} Risk_Record;
static Risk_Config settings={0,{32,28,20,12,8},{50,200,500,300,600,800,200,500,800,20,50,150,200,500,1500}};
#define CONFIG_START 0x08060000u
#define CONFIG_END 0x08080000u
#define CONFIG_MAGIC 0x5249534bu
static uint32_t slot;
static uint32_t Hash(const Risk_Config *c){const uint8_t *p=(const uint8_t*)c;uint32_t h=2166136261u;unsigned i;for(i=0;i<sizeof(*c);i++)h=(h^p[i])*16777619u;return h;}
static int Valid(const Risk_Config *c){unsigned i,sum=0;static const uint16_t max[5]={7500,1000,1000,18000,60000};for(i=0;i<5;i++){sum+=c->w[i];if(c->w[i]>100||!c->edge[i*3]||c->edge[i*3]>=c->edge[i*3+1]||c->edge[i*3+1]>=c->edge[i*3+2]||c->edge[i*3+2]>max[i])return 0;}return sum==100 && c->revision<=2147483647u;}
void Risk_Init(void){const Risk_Record *r;unsigned i;for(slot=CONFIG_START;slot+sizeof(*r)<=CONFIG_END;slot+=sizeof(*r)){r=(const Risk_Record*)slot;for(i=0;i<sizeof(*r)/4;i++)if(((const uint32_t*)r)[i]!=0xffffffffu)break;if(i==sizeof(*r)/4)break;if(r->magic==CONFIG_MAGIC && r->crc==Hash(&r->c)&&Valid(&r->c))settings=r->c;}}
static int Number(const char **p,uint32_t *v){unsigned n=0;*v=0;while(**p>='0'&&**p<='9'){if(++n>10||*v>214748364u||(*v==214748364u&&**p>'7'))return 0;*v=*v*10+(unsigned)(*(*p)++-'0');}return n!=0;}
int Risk_ApplyConfig(const char *p){Risk_Record r;uint32_t n;unsigned i;const uint32_t *words=(const uint32_t*)&r;memset(&r,0,sizeof(r));if(!Number(&p,&n)||n!=1||*p++!=','||!Number(&p,&r.c.revision))return 0;for(i=0;i<20;i++){if(*p++!=','||!Number(&p,&n)||n>65535)return 0;if(i<5)r.c.w[i]=(uint16_t)n;else r.c.edge[i-5]=(uint16_t)n;}if(*p||!Valid(&r.c))return 0;if(r.c.revision==settings.revision)return !memcmp(&r.c,&settings,sizeof(settings));if(r.c.revision<settings.revision||slot+sizeof(r)>CONFIG_END)return 0;r.crc=Hash(&r.c);r.magic=CONFIG_MAGIC;FLASH_Unlock();FLASH_ClearFlag(FLASH_FLAG_EOP|FLASH_FLAG_OPERR|FLASH_FLAG_WRPERR|FLASH_FLAG_PGAERR|FLASH_FLAG_PGPERR|FLASH_FLAG_PGSERR);for(i=0;i<sizeof(r)/4;i++)if(FLASH_ProgramWord(slot+i*4,words[i])!=FLASH_COMPLETE)break;FLASH_Lock();if(i!=sizeof(r)/4||memcmp((const void*)slot,&r,sizeof(r))){slot+=sizeof(r);return 0;}slot+=sizeof(r);settings=r.c;result.valid=0;confirmCount=result.confirming=0;return 1;}
unsigned Risk_Weight(unsigned index){return index<5?settings.w[index]:0;}
void Risk_ConfigText(char *p){unsigned i;p+=sprintf(p,"1,%lu",(unsigned long)settings.revision);for(i=0;i<20;i++)p+=sprintf(p,",%u",i<5?settings.w[i]:settings.edge[i-5]);}
static uint8_t ConfigPoints(int32_t value,unsigned i){return Points(value,settings.edge[i*3],settings.edge[i*3+1],settings.edge[i*3+2]);}

const Risk_Result *Risk_Get(void) { return &result; }
uint8_t Risk_SetBaseline(const Tilt_Result *t)
{
    if(!t->valid || !t->rateValid) return 0;
    basePosition=t->positionMean100; baseAngle=t->angle100;
    result.baseline=1; result.valid=0; result.level=0;
    result.displacement100=result.angle100=0;
    candidate=confirmCount=result.confirming=0; seenWindow=t->windowId;
    return 1;
}
void Risk_Update(const Tilt_Result *t,int32_t soil10,int32_t rain10,int32_t forecast10)
{
    uint8_t d,a,target,complete;
    result.soil10=soil10; result.rain10=rain10; result.forecast10=forecast10;
    complete=t->valid && t->rateValid && soil10>=0 && rain10>=0 && forecast10>=0;
    if(!result.baseline || !t->valid || !t->rateValid) {
        result.valid=0; confirmCount=result.confirming=0; return;
    }
    /* Immediately remove stale/full-score validity, but only count a new
     * paired five-second window toward an alarm confirmation. */
    if(!complete) result.valid=0;
    if(t->windowId==seenWindow) return;
    seenWindow=t->windowId;
    result.displacement100=Abs(t->positionMean100-basePosition);
    result.angle100=Abs(t->angle100-baseAngle);
    d=ConfigPoints(result.displacement100,0);
    a=ConfigPoints(result.angle100,3);
    target=d==3 || a==3?2:d==2 || a==2?1:0;
    result.valid=complete;
    if(complete) {
        result.score100=settings.w[0]*d+settings.w[1]*ConfigPoints(soil10,1)+
            settings.w[2]*ConfigPoints(rain10,2)+settings.w[3]*a+settings.w[4]*ConfigPoints(forecast10,4);
        if(result.score100>=200) target=2;
        else if(result.score100>100 && target<1) target=1;
    } else {
        /* Missing environmental data cannot silently lower an alarm.
         * Valid displacement/angle can still escalate their own alarm. */
        if(target<result.level) target=result.level;
    }
    if(target==result.level) { confirmCount=result.confirming=0; return; }
    if(candidate!=target) { candidate=target; confirmCount=0; }
    result.confirming=1;
    /* Two consecutive complete windows for either direction (~10 seconds). */
    if(++confirmCount>=2) {
        result.level=target; confirmCount=result.confirming=0;
    }
}
