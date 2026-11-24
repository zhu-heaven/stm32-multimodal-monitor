#include "cloud_command.h"
#include <string.h>
#if defined(__CC_ARM)
#pragma O2
#endif

static const char *White(const char *p)
{ while(*p==' '||*p=='\t'||*p=='\r'||*p=='\n') p++; return p; }
static int Digit(char c) { return c>='0' && c<='9'; }
static const char *StringEnd(const char *p)
{
    unsigned i;
    if(*p++!='"') return 0;
    while(*p && *p!='"') {
        if((unsigned char)*p<32) return 0;
        if(*p=='\\') {
            p++;
            if(*p=='u') {
                for(i=0;i<4;i++) {p++; if(!Digit(*p) && !(*p>='a'&&*p<='f') && !(*p>='A'&&*p<='F')) return 0;}
            } else if(!*p || !strchr("\"\\/bfnrt",*p)) return 0;
        }
        p++;
    }
    return *p=='"'?p+1:0;
}
static const char *ValueEnd(const char *p,unsigned depth)
{
    char close;
    if(depth>8) return 0;
    p=White(p);
    if(*p=='"') return StringEnd(p);
    if(*p=='{' || *p=='[') {
        close=*p=='{'?'}':']'; p=White(p+1);
        if(*p==close) return p+1;
        for(;;) {
            if(close=='}') {
                p=StringEnd(p); if(!p) return 0;
                p=White(p); if(*p++!=':') return 0;
            }
            p=ValueEnd(p,depth+1); if(!p) return 0;
            p=White(p); if(*p==close) return p+1;
            if(*p++!=',') return 0;
            p=White(p);
        }
    }
    if(!strncmp(p,"true",4)) return p+4;
    if(!strncmp(p,"false",5)) return p+5;
    if(!strncmp(p,"null",4)) return p+4;
    if(*p=='-') p++;
    if(*p=='0') p++;
    else { if(*p<'1'||*p>'9') return 0; while(Digit(*p)) p++; }
    if(*p=='.') {p++; if(!Digit(*p)) return 0; while(Digit(*p)) p++;}
    if(*p=='e'||*p=='E') {p++; if(*p=='+'||*p=='-') p++; if(!Digit(*p)) return 0; while(Digit(*p)) p++;}
    return p;
}
/* Object already validated; duplicate requested keys are rejected. */
static const char *Field(const char *object,const char *key,unsigned *count)
{
    const char *p=White(object+1),*end,*found=0;
    unsigned length=(unsigned)strlen(key);
    *count=0;
    while(*p!='}') {
        end=StringEnd(p);
        if(!end) return 0;
        (*count)++;
        if((unsigned)(end-p)==length+2 && !memcmp(p+1,key,length)) {
            if(found) {*count=65535u; return 0;}
            found=White(White(end)+1);
        }
        p=White(White(end)+1);
        p=White(ValueEnd(p,1));
        if(*p=='}') break;
        p=White(p+1);
    }
    return found;
}
int CloudCommand_Parse(const char *json,char id[14],unsigned char *on,char config[192])
{
    const char *root=White(json),*end,*p,*params,*value;
    unsigned n=0,count;
    id[0]=0;
    if(*root!='{') return 0;
    end=ValueEnd(root,0); if(!end || *White(end)) return 0;
    p=Field(root,"code",&count);
    if(p || count==65535u) return 0; /* property-post reply, never a command */
    p=Field(root,"version",&count);
    if(count==65535u || (p && strncmp(p,"\"1.0\"",5))) return 0;
    p=Field(root,"id",&count); if(!p || *p++!='"') return 0;
    while(Digit(*p) && n<13) id[n++]=*p++;
    if(!n || *p!='"') {id[0]=0; return 0;}
    id[n]=0;
    params=Field(root,"params",&count);
    if(!params || *params!='{') return 0;
    value=Field(params,"risk_config",&count);
    if(value && count==1 && *value=='"') {unsigned length=0;value++;while((*value>='0'&&*value<='9')||*value==','){if(length>=191)return -1;config[length++]=*value++;}config[length]=0;return *value=='"' && length?2:-1;}
    value=Field(params,"buzzer_switch",&count);
    if(!value || count!=1) return -1;
    end=ValueEnd(value,1);
    if(end-value==4 && !memcmp(value,"true",4)) {*on=1; return 1;}
    if(end-value==5 && !memcmp(value,"false",5)) {*on=0; return 1;}
    return -1;
}
