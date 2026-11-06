#define _GNU_SOURCE
#include "rear_native.h"
#include "music_settings.h"
#include "music_ui.h"
#include "pad_workflow_colour.h"
#include "step_native.h"
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <math.h>
#include <time.h>

#define FIELD(p,o,T) (*(T *)((char *)(p)+(o)))
#define FN(o,T) ((T)(image+(o)))
static uintptr_t image;
static void *roots[2];
static _Atomic unsigned dead[2], visible_roots, open_panel, fault;
static unsigned confirm, overwrite, panel_on_record;
static _Atomic unsigned waiting_right;
static _Atomic unsigned selected_mask;
static _Atomic uint64_t light_phase;
static _Atomic unsigned held[112];
/* One snapshot for RGB readers. High bits store bank+1, low bits store occupancy. */
static _Atomic unsigned bank_lights;
extern void rear_ui_wake(void);
extern unsigned phone_native_button_light(unsigned code);
static const char *const modes[]={"Stereo","Rear L","Rear R","L + R"};
static int visible(void *p){return p&&FN(0x112fd50,int(*)(void*))(p)&&!FN(0x112f6b0,int(*)(void*))(p);}
static void repaint(void){for(unsigned i=0;i<2;++i)if(roots[i]&&!atomic_load(&dead[i]))FN(0x1149880,void(*)(void*,uint64_t,uint64_t))(roots[i],0,FIELD(roots[i],0x28,uint64_t));}
static void rect(void *g,int x,int y,int w,int h,uint32_t colour){FN(0x102d5e0,void(*)(void*,uint32_t))(g,colour);FN(0x102e000,void(*)(void*,int,int,int,int))(g,x,y,w,h);}
static void text_aligned(void *g,const char *value,int x,int y,int w,int h,float height,uint32_t colour,int align){
    uint64_t s=0,font=0;FN(0xf535c0,void(*)(void*,const char*))(&s,value);FN(0x1082810,void(*)(void*,int,float))(&font,0,height);
    FN(0x102dca0,void(*)(void*,const void*))(g,&font);FN(0x102d5e0,void(*)(void*,uint32_t))(g,colour);
    FN(0x10817b0,void(*)(void*,const void*,int,int,int,int,int,int,float))(g,&s,x,y,w,h,align,1,.7f);
    FN(0x1070d60,void(*)(void*))(&font);FN(0xf0a290,void(*)(void*))(&s);
}
static void text(void *g,const char *value,int x,int y,int w,int h,float height,uint32_t colour){text_aligned(g,value,x,y,w,h,height,colour,0x24);}
static void left(void *g,const char *value,int x,int y,int w,int h,float height,uint32_t colour){text_aligned(g,value,x,y,w,h,height,colour,0x21);}
static void right(void *g,const char *value,int x,int y,int w,int h,float height,uint32_t colour){text_aligned(g,value,x,y,w,h,height,colour,0x22);}
#define REAR_WHITE 0xffffffffu
#define REAR_GREY 0xff8a8a8au
#define REAR_AMBER 0xffffc247u
#define REAR_RED 0xffff4040u
/* Stock K panel: label, value, thin value bar and the S square for Shift. */
static void panel(void *g,unsigned i,const char *label,const char *value,int badge,int shift,unsigned position,unsigned count){
    int x=i==2?213:(int)i*106,w=i==2?107:106;
    rect(g,x,198,w,42,0xff000000u);
    text(g,label,x+4,200,badge?w-24:w-8,13,12,REAR_WHITE);text(g,value,x+4,214,w-8,19,16,REAR_WHITE);
    if(badge){/* Native badge (values_ui.S): 10x12 frame with S, red while Shift is held. */
        int bx=(int)i*106+94;uint32_t c=shift?0xffff4747u:REAR_WHITE;
        rect(g,bx,200,10,1,c);rect(g,bx,211,10,1,c);rect(g,bx,200,1,12,c);rect(g,bx+9,200,1,12,c);text(g,"S",bx+1,201,8,10,10,c);}
    if(count>1){rect(g,x+6,235,w-12,2,0xff3b3b3bu);int span=w-12,seg=span/(int)count;if(seg<4)seg=4;
        int at=(int)position*(span-seg)/((int)count-1);rect(g,x+6+at,235,seg,2,REAR_WHITE);}
}
static void pad_name(char *s,unsigned pad){snprintf(s,8,"%c%02u",'A'+pad/16,pad%16+1);}
static unsigned mask(void){unsigned mode=rear_setting(0,REAR_MODE);return mode==REAR_DUAL?3:mode==REAR_LEFT?1:mode==REAR_RIGHT?2:0;}

static void cancel_choice(void){waiting_right=confirm=overwrite=0;atomic_store(&selected_mask,0);repaint();}
static void close_panel(void){atomic_store(&open_panel,0);cancel_choice();}
static void start_recording(void){
    unsigned selected=mask(),occupied=0;
    for(unsigned ch=0;ch<2;++ch)if(selected&(1u<<ch)){
        unsigned target=rear_setting(ch,REAR_TARGET);
        if(rear_native_bank_occupied(target/16)&(1u<<(target%16)))occupied|=1u<<ch;
    }
    waiting_right=0;
    if(occupied){overwrite=occupied;confirm=1;repaint();return;}
    rear_native_start(0);repaint();
}
static void meter(void *g,unsigned ch,int y){
    float levels[2];rear_audio_meters(levels);
    float level=levels[ch]>0?20.f*log10f(levels[ch]):-60.f;
    int width=(int)((level+60.f)*144.f/60.f);if(width<0)width=0;if(width>144)width=144;
    int x=ch?168:8;
    rect(g,x,y,144,4,0xff242424u);if(width)rect(g,x,y,width,4,level>=-.1f?REAR_RED:0xff1fd8a0u);
}
static void pan_name(char *s,unsigned ch){int p=(int)rear_setting(ch,REAR_PAN)-50;if(!p)snprintf(s,24,"C");else snprintf(s,24,"%c%d",p<0?'L':'R',p<0?-p:p);}
static void paint(void *p,void *g){
    unsigned kind=FIELD(p,0,uintptr_t)==image+0x6e22970?1:0;
    rear_ui_wake();roots[kind]=p;atomic_store(&dead[kind],0);
    if(!visible(p)||music_ui_repeat_active()||music_ui_knob_fx_active())return;
    if(!atomic_load(&open_panel)){
        if(rear_native_source()!=1)return;
        rect(g,107,0,105,35,0xff000000u);
        text(g,"Routing",107,kind?5:2,105,kind?23:17,14,0xffffffffu);
        if(!kind)text(g,"Shift+B2",107,19,105,13,10,0xffbbbbbb);
        return;
    }
    if(kind!=panel_on_record)return;
    rect(g,0,0,320,240,0xff000000u);
    if(confirm){
        /* Stock confirmation: framed box, B1 Cancel and a red destructive B3. */
        text(g,"Cancel",0,5,106,23,14,REAR_WHITE);text(g,"Replace",214,5,106,23,14,REAR_RED);
        rect(g,6,51,308,145,REAR_WHITE);rect(g,7,52,306,143,0xff000000u);
        left(g,"Replace pads",24,66,272,26,19,REAR_WHITE);rect(g,24,100,272,1,REAR_WHITE);
        char s[80],l[8],r[8];pad_name(l,rear_setting(0,REAR_TARGET));pad_name(r,rear_setting(1,REAR_TARGET));
        snprintf(s,sizeof(s),"%s%s%s",overwrite&1?l:"",overwrite==3?" and ":"",overwrite&2?r:"");
        left(g,s,24,112,272,24,19,REAR_AMBER);
        left(g,overwrite==3?"Both pads will be overwritten.":"The pad will be overwritten.",24,144,272,20,13,REAR_WHITE);
        return;
    }
    unsigned busy=rear_native_busy(),selected=kind?mask():0,shift=music_ui_shift_active()!=0;
    text(g,waiting_right?"Cancel":"Back",0,5,106,23,14,REAR_WHITE);
    for(unsigned ch=0;ch<2;++ch){
        unsigned on=rear_setting(ch,REAR_MONITOR);int x=ch?214:107,w=ch?106:105;
        text(g,ch?"Mon R":"Mon L",x,5,w,23,14,on?REAR_WHITE:REAR_GREY);
        if(on)rect(g,x+w/2-18,29,36,2,REAR_AMBER);
    }
    left(g,"Rear Routing",8,34,180,22,15,REAR_AMBER);
    right(g,kind?"Record":"Monitor",190,34,122,22,12,REAR_GREY);
    for(unsigned ch=0;ch<2;++ch){
        char s[40],pan[24];int x=ch?168:8;pan_name(pan,ch);
        left(g,ch?"Rear R":"Rear L",x,60,144,20,15,REAR_WHITE);
        if(kind){
            unsigned used=(selected>>ch)&1u,target=rear_setting(ch,REAR_TARGET);char pad[8];pad_name(pad,target);
            if(!used)right(g,"not recorded",x,60,144,20,11,REAR_GREY);
            else if((atomic_load(&selected_mask)>>ch)&1u){snprintf(s,sizeof(s),"to %s",pad);right(g,s,x,60,144,20,13,REAR_AMBER);}
            else right(g,"choose pad",x,60,144,20,11,REAR_GREY);
        }
        meter(g,ch,84);
        snprintf(s,sizeof(s),"Pan %s",pan);left(g,s,x,94,64,18,12,REAR_GREY);
        snprintf(s,sizeof(s),"Level %u%%",rear_setting(ch,REAR_LEVEL));right(g,s,x+64,94,80,18,12,REAR_GREY);
    }
    rect(g,8,120,304,1,0xff3b3b3bu);
    char status[96];unsigned state=rear_record_state();uint32_t ink=REAR_WHITE;
    if(busy){snprintf(status,sizeof(status),"%s  %.1f s",state==REAR_RECORDING?"Recording":state==REAR_WAITING?"Waiting for input":"Saving",(double)rear_record_frames()/44100.);ink=state==REAR_RECORDING?REAR_RED:REAR_WHITE;}
    else if(waiting_right)snprintf(status,sizeof(status),"Now press the pad for R");
    else if(rear_native_status())snprintf(status,sizeof(status),"%s",rear_native_message());
    else snprintf(status,sizeof(status),"%s",!kind?"Sample Record > B2 to record":selected==3?"Press the pad for L":"Press a pad to record");
    text(g,status,8,130,304,26,17,ink);
    const char *hint=busy?"STOP: finish recording":waiting_right?"B1 or STOP: cancel":kind?"Pad Bank + pads 1-8: bank":"B2 / B3: monitor on or off";
    text(g,hint,8,160,304,18,12,REAR_GREY);
    char value[24];
    if(kind)panel(g,0,"Record",modes[rear_setting(0,REAR_MODE)],0,0,rear_setting(0,REAR_MODE)?rear_setting(0,REAR_MODE)-1:0,3);
    else panel(g,0,"","",0,0,0,0);
    for(unsigned ch=0;ch<2;++ch){
        char label[16];snprintf(label,sizeof(label),shift?"Level %c":"Pan %c",ch?'R':'L');
        if(shift){snprintf(value,sizeof(value),"%u %%",rear_setting(ch,REAR_LEVEL));panel(g,1+ch,label,value,1,1,rear_setting(ch,REAR_LEVEL),101);}
        else{pan_name(value,ch);panel(g,1+ch,label,value,1,0,rear_setting(ch,REAR_PAN),101);}
    }
}
static void destroy(void *p){unsigned i=FIELD(p,0,uintptr_t)==image+0x6e22970;atomic_store(&dead[i],1);FN(i?0x1682430:0x16248e0,void(*)(void*))(p);}
static void deleting(void *p){unsigned i=FIELD(p,0,uintptr_t)==image+0x6e22970;atomic_store(&dead[i],1);FN(i?0x1682d70:0x1624d50,void(*)(void*))(p);}
static void secondary_destroy_record(void *p){atomic_store(&dead[1],1);FN(0x1682430,void(*)(void*))((char*)p-0xb0);}
static void secondary_delete_record(void *p){atomic_store(&dead[1],1);FN(0x1682d70,void(*)(void*))((char*)p-0xb0);}
static void secondary_destroy_config(void *p){atomic_store(&dead[0],1);FN(0x16248e0,void(*)(void*))((char*)p-0xb0);}
static void secondary_delete_config(void *p){atomic_store(&dead[0],1);FN(0x1624d50,void(*)(void*))((char*)p-0xb0);}
int rear_ui_install(uintptr_t base){
    image=base;
    const uintptr_t offsets[]={0x6e22970+0xd8,0x6e1dd80+0xd8,0x6e22970,0x6e22978,0x6e1dd80,0x6e1dd88,0x6e22ae8,0x6e22af0,0x6e1def8,0x6e1df00};
    const uintptr_t expected[]={0x10e2e10,0x10e2e10,0x1682430,0x1682d70,0x16248e0,0x1624d50,0x1682d60,0x1682da0,0x1624d40,0x1624d80};
    uintptr_t callbacks[]={(uintptr_t)paint,(uintptr_t)paint,(uintptr_t)destroy,(uintptr_t)deleting,(uintptr_t)destroy,(uintptr_t)deleting,(uintptr_t)secondary_destroy_record,(uintptr_t)secondary_delete_record,(uintptr_t)secondary_destroy_config,(uintptr_t)secondary_delete_config};
    for(unsigned n=0;n<10;++n)if(*(uintptr_t*)(base+offsets[n])!=base+expected[n])return 0;
    for(unsigned n=0;n<10;++n){void *p=(void*)((base+offsets[n])&~(uintptr_t)4095);if(mprotect(p,4096,PROT_READ|PROT_WRITE))return 0;*(uintptr_t*)(base+offsets[n])=callbacks[n];if(mprotect(p,4096,PROT_READ))_exit(121);}
    return 1;
}
int rear_ui_tick(void){
    if(atomic_exchange(&fault,0))close_panel();
    unsigned visible_mask=0;
    for(unsigned i=0;i<2;++i){if(atomic_exchange(&dead[i],0)){roots[i]=NULL;close_panel();}if(visible(roots[i]))visible_mask|=1u<<i;}
    atomic_store(&visible_roots,visible_mask);
    rear_native_tick((visible_mask&2)!=0);
    static int last_rear;
    int rear=rear_native_source()==1;
    if(atomic_load(&open_panel)&&(!rear||!(visible_mask&(1u<<panel_on_record)))){
        if(rear_native_busy())rear_record_cancel();
        close_panel();
    }
    if(rear!=last_rear){last_rear=rear;if(visible_mask)repaint();}
    int bank=visible_mask?step_native_browser_bank(-1):-1;
    unsigned occupied=bank>=0&&atomic_load(&open_panel)?rear_native_bank_occupied((unsigned)bank):0;
    atomic_store(&bank_lights,((unsigned)(bank+1)<<16)|occupied);
    if(!visible_mask){if(atomic_load(&open_panel))close_panel();if(rear_native_busy())rear_record_cancel();return rear_native_busy()||rear_setting(0,REAR_ENABLED);}
    struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);atomic_store(&light_phase,(uint64_t)t.tv_sec*1000+(uint64_t)t.tv_nsec/1000000);
    if(atomic_load(&open_panel)||rear_native_busy())repaint();
    return 1;
}

int rear_ui_owns(unsigned kind,unsigned code){
    if(kind==1)return atomic_load(&open_panel)&&atomic_load(&visible_roots)&&code!=0x13&&!music_ui_repeat_active()&&!music_ui_knob_fx_active();
    if(kind!=2)return 0;
    if(code<112&&held[code])return 1;
    if(music_ui_repeat_active()||music_ui_knob_fx_active())return 0;
    if(!atomic_load(&visible_roots))return 0;
    unsigned panel=atomic_load(&open_panel);
    if(code==0x0d||code==0x6c)return panel||(rear_native_source()==1&&((atomic_load(&visible_roots)&2)||music_ui_shift_active()));
    if(!panel)return 0;
    return code==0x51||code==0x49||code==0x6f||code==0x0c||code==0x0e||code==0x6b||code==0x6d;
}
int rear_ui_button(void *event){
    unsigned code=FIELD(event,8,unsigned char),down=FIELD(event,9,unsigned char)!=0;
    if(code>=112||!rear_ui_owns(2,code))return 0;
    if(!down){held[code]=0;return 1;}if(held[code])return 1;held[code]=1;
    if(code==0x51||code==0x49){if(rear_native_busy())rear_native_stop();else cancel_choice();return 1;}
    if(rear_native_busy()&&code!=0x0d&&code!=0x0e&&code!=0x6c&&code!=0x6d)return 1;
    if(!atomic_load(&open_panel)){
        panel_on_record=(atomic_load(&visible_roots)&2)!=0;cancel_choice();
        if(!rear_setting(0,REAR_MODE))rear_set_setting(0,REAR_MODE,REAR_LEFT);
        rear_native_clear();atomic_store(&open_panel,1);repaint();return 1;
    }
    unsigned button=code>=0x6b&&code<=0x6d?code-0x6b:code>=0x0c&&code<=0x0e?code-0x0c:3;
    if(confirm){if(button==0)cancel_choice();else if(button==2){confirm=0;rear_native_start(overwrite);}repaint();return 1;}
    if(button==0){if(waiting_right)cancel_choice();else close_panel();return 1;}
    if(button==1||button==2){
        unsigned ch=button-1,on=!rear_setting(ch,REAR_MONITOR);
        rear_set_setting(ch,REAR_MONITOR,on);rear_set_setting(0,REAR_ENABLED,1);
        if(on)rear_native_monitor_on();
    }
    repaint();return 1;
}
static unsigned adjusted(unsigned value,unsigned max){unsigned n=value*(max+1)/128;return n>max?max:n;}
/* Pad Bank + K: Rear Routing K2/K3 back to the defaults of rear_audio.cc
 * (Pan: Rear L hard left 0, Rear R hard right 100; Shift Level 100 %).
 * K1 (record mode) is a choice, not a parameter: consumed, unchanged. */
int rear_ui_reset_knob(unsigned column,unsigned shift){
    if(column>2||!rear_ui_owns(1,0x10+column))return 0;
    if(confirm||column==0)return 1;
    unsigned ch=column-1;
    rear_set_setting(ch,shift?REAR_LEVEL:REAR_PAN,shift?100:(ch?100:0));
    repaint();return 1;
}
int rear_ui_control(void *event){
    unsigned cc=FIELD(event,8,unsigned char),value=FIELD(event,9,unsigned char);
    if(!rear_ui_owns(1,cc))return 0;
    if(cc==0x13)return 0;
    if(confirm||(rear_native_busy()&&cc==0x10))return 1;
    if(cc==0x10&&panel_on_record){unsigned mode=1+adjusted(value,2);if(mode!=rear_setting(0,REAR_MODE)){cancel_choice();rear_set_setting(0,REAR_MODE,mode);rear_native_clear();}}
    else if(cc==0x11||cc==0x12){rear_set_setting(cc-0x11,music_ui_shift_active()?REAR_LEVEL:REAR_PAN,adjusted(value,100));rear_set_setting(0,REAR_ENABLED,1);}
    repaint();return 1;
}
int rear_ui_pad_owns(void){return !music_ui_repeat_active()&&!music_ui_knob_fx_active()&&atomic_load(&open_panel);}

void rear_ui_pad(unsigned pad,int down){
    if(!down||pad>=16||!atomic_load(&open_panel)||!panel_on_record||confirm||rear_native_busy())return;
    int bank=step_native_browser_bank(-1);if(bank<0||bank>=8)return;
    unsigned target=(unsigned)bank*16+pad,mode=rear_setting(0,REAR_MODE);
    rear_native_clear();
    if(mode==REAR_DUAL){
        if(!waiting_right){rear_set_setting(0,REAR_TARGET,target);waiting_right=1;atomic_store(&selected_mask,1);repaint();return;}
        if(target==rear_setting(0,REAR_TARGET)){cancel_choice();return;}
        rear_set_setting(1,REAR_TARGET,target);atomic_store(&selected_mask,3);
    }else{
        rear_set_setting(mode==REAR_RIGHT,REAR_TARGET,target);atomic_store(&selected_mask,mask());
    }
    start_recording();
}
int rear_ui_light(unsigned pad,uint64_t *out){
    if(pad>=16||!rear_ui_pad_owns())return 0;
    unsigned lights=atomic_load(&bank_lights), bank=(lights>>16)-1;
    if(bank>=8)return 0;
    unsigned target=bank*16+pad;
    for(unsigned ch=0;ch<2;++ch)if((atomic_load(&selected_mask)&(1u<<ch))&&target==rear_setting(ch,REAR_TARGET)){
        if(waiting_right){*out=pad_light(atomic_load(&light_phase)%600<300?PAD_SELECT:0,1.f);return 1;}
        *out=pad_light(music_service_rgb((lights&(1u<<pad))?MUSIC_SERVICE_DANGER:MUSIC_SERVICE_FREE),1.f);
        return 1;
    }
    *out=pad_target_colour(target,(lights&(1u<<pad))!=0,0,0,0);
    return 1;
}
void rear_ui_fault(void){rear_record_cancel();atomic_store(&fault,1);}
