extern const char *input_fx_status(void) __attribute__((weak));
#include "mixer_view.h"
#include "rear_audio.h"
extern unsigned rear_setting(unsigned,unsigned) __attribute__((weak));
extern int rear_native_source(void) __attribute__((weak));
extern unsigned input_fx_mode(void) __attribute__((weak));
#include "fx_catalog.h"
#include "fx_own.h"
#include "compressor.h"
#include "shift_badge.h"
#include "spectrum_plot.h"
#include "mixer_sample_feedback.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define BLACK 0xff000000u
#define WHITE 0xffffffffu
#define GREY 0xff9ba5b0u
#define DARK 0xff28313au
#define PANEL 0xff11171du
#define SELECTED 0xff1c252eu
#define LINE 0xff36424du
#define WELL 0xff080d12u
#define BLUE 0xff71c1e8u
#define KEY 0xffb5a3ffu
#define SEND 0xffff4040u
#define AMBER 0xffffc247u
static void rect(const struct resample_painter*p,int x,int y,int w,int h,uint32_t c)
{
    if(w>0&&h>0)p->rect(p->user,x,y,w,h,c);
}
static void text(const struct resample_painter*p,const char*s,int x,int y,int w,int h,unsigned size,uint32_t c,int centre)
{
    p->text(p->user,s,x,y,w,h,size,c,centre);
}
static void track_title(const struct resample_painter*p,const char*name,int x,int y,int w,int h,
                        unsigned size,uint32_t ink,int centred,uint64_t elapsed)
{
    /* Draw only complete UTF-8 glyphs inside the viewport while scrolling. */
    const unsigned char*start[MIXER_NAME_BYTES+8];
    int offsets[MIXER_NAME_BYTES+9]={0};
    unsigned count=0;
    const unsigned char*next=(const unsigned char*)name;
    while(*next&&count<MIXER_NAME_BYTES+8){
        start[count]=next++;
        while((*next&0xc0)==0x80)++next;
        unsigned ch=*start[count];
        int advance=(int)(size*(ch<128&&strchr(" ilI.,'!",(int)ch)?.36:
                                      ch<128&&strchr("MWmw",(int)ch)?.82:.65));
        offsets[count+1]=offsets[count]+advance;
        ++count;
    }
    int length=offsets[count];
    if(length<=w){text(p,name,x,y,w,h,size,ink,centred);return;}
    uint64_t travel=(uint64_t)(length-w+5)*53;
    uint64_t phase=elapsed%(travel+2600);
    int scroll=phase<1700?0:phase>=1700+travel?length-w+5:(int)((phase-1700)/53);
    for(unsigned i=0;i<count;++i){
        int pos=offsets[i]-scroll,advance=offsets[i+1]-offsets[i];
        if(pos<0||pos+advance>w)continue;
        const unsigned char*end=i+1<count?start[i+1]:next;
        char glyph[5]={0};
        if(end-start[i]>4)continue;
        memcpy(glyph,start[i],(size_t)(end-start[i]));
        text(p,glyph,x+pos,y,advance,h,size,ink,1);
    }
}
static void border(const struct resample_painter*p,int x,int y,int w,int h,uint32_t c)
{
    rect(p,x,y,w,1,c);rect(p,x,y+h-1,w,1,c);rect(p,x,y,1,h,c);rect(p,x+w-1,y,1,h,c);
}
int mixer_view_gain_y(float db)
{
    if(!isfinite(db))return 177;
    if(db<-60)db=-60;
    if(db>6)db=6;
    return db>0?114-(int)lroundf(db*2):114+(int)lroundf(-db);
}
float mixer_view_gain_db(int y)
{
    if(y>=177)return -INFINITY;
    if(y<=102)return 6;
    if(y<=114)return (114-y)*.5f;
    return y>=174?-60.f:(float)(114-y);
}
static void pan(char*out,size_t size,float position)
{
    int percent=(int)lroundf(position*100);
    if(!percent)snprintf(out,size,"C");
    else snprintf(out,size,"%c%d",percent<0?'L':'R',percent<0?-percent:percent);
}
static int available(const struct mixer_view_state*s,unsigned bus)
{
    return s->meter&&s->meter->epoch==s->epoch&&s->now_ms>=s->meter_ms&&s->now_ms-s->meter_ms<=250&&
        (s->meter->valid_mask&(1u<<bus));
}
static void level_text(char*out,size_t size,float db)
{
    if(db==0)db=0.f;
    if(!isfinite(db))snprintf(out,size,"OFF");else snprintf(out,size,"%+.1f dB",(double)db);
}
/* Vertical stereo meter (two bars) in [x,x+w) x [y,y+h). */
static void vmeter(const struct mixer_view_state*s,unsigned bus,int x,int y,int w,int h,const struct resample_painter*p)
{
    rect(p,x,y,w,h,WELL);
    if(!available(s,bus)){text(p,"N/A",x-6,y+h/2-6,w+12,12,8,GREY,1);return;}
    int bar=(w-1)/2;
    for(unsigned c=0;c<2;++c){float db=mixer_meter_dbfs(s->meter->peak[bus][c]);
        int fill=isfinite(db)?(int)lroundf((fmaxf(-60,fminf(0,db))+60)*h/60):0;
        int left=x+(int)c*(bar+1);
        rect(p,left,y,bar,h,DARK);
        if(fill>0)rect(p,left,y+h-fill,bar,fill,(s->meter->clip_mask&(1u<<bus))?SEND:AMBER);
        for(int db=12;db<60;db+=12)rect(p,left,y+db*h/60,bar,1,WELL);}
}
extern void input_fx_peak(float*left,float*right) __attribute__((weak));
static void input_meter(int x,int y,int w,int h,const struct resample_painter*p)
{
    float peak[2]={0,0};
    if(input_fx_peak)input_fx_peak(peak,peak+1);
    rect(p,x,y,w,h,WELL);
    int bar=(w-1)/2;
    for(unsigned c=0;c<2;++c){
        float db=mixer_meter_dbfs(peak[c]);
        int fill=isfinite(db)?(int)lroundf((fmaxf(-60,fminf(0,db))+60)*h/60):0;
        int left=x+(int)c*(bar+1);
        rect(p,left,y,bar,h,DARK);
        if(fill>0)rect(p,left,y+h-fill,bar,fill,db>-1.f?SEND:AMBER);
        for(int d=12;d<60;d+=12)rect(p,left,y+d*h/60,bar,1,WELL);
    }
}
static void send_meter(const struct mixer_view_state*s,unsigned track,
                       int x,int y,int h,const struct resample_painter*p)
{
    if(track>=MIXER_TRACKS)return;
    int receiving=available(s,track)&&(s->meter->return_mask&(1u<<track));
    rect(p,x,y,2,h,receiving?0xff351619u:WELL);
    if(!receiving)return;
    float db=mixer_meter_dbfs(s->meter->return_peak[track]);
    if(!isfinite(db))return;
    int height=(int)lroundf(fmaxf(0,fminf(60,db+60))*h/60);
    if(height)rect(p,x,y+h-height,2,height,SEND);
}
static void pan_bar(float position,int x,int y,int w,const struct resample_painter*p)
{
    int centre=x+w/2;if(!(position>=-1))position=-1;if(position>1)position=1;
    int at=centre+(int)lroundf(position*(float)(w/2-1));
    rect(p,x,y,w,8,WELL);rect(p,x,y+3,w,2,DARK);rect(p,centre,y,1,8,GREY);
    if(at<centre)rect(p,at,y+2,centre-at,4,AMBER);else if(at>centre)rect(p,centre+1,y+2,at-centre,4,AMBER);
    rect(p,at-1,y,3,8,WHITE);
}
static int rail_y(float db,int top,int height)
{
    /* Same law as the classic strip: 0 dB at 1/6 from the top, +6 at top,
     * -60 near the bottom, Off at the bottom. */
    if(!isfinite(db))return top+height-1;
    if(db>6)db=6;
    if(db< -60)db=-60;
    int unity=top+height/6;
    if(db>=0)return unity-(int)lroundf(db*(float)(unity-top)/6.f);
    return unity+(int)lroundf(-db*(float)(top+height-3-unity)/60.f);
}
static void fader_rail(float db,int selected,int focused,int x,int y,int h,int cap,const struct resample_painter*p)
{
    static const float marks[]={6,0,-6,-12,-24,-36,-60};
    rect(p,x-2,y,5,h,0xff303236u);
    rect(p,x-1,y+1,3,h-2,0xff090a0bu);
    for(unsigned i=0;i<sizeof(marks)/sizeof(marks[0]);++i){
        int mark=rail_y(marks[i],y,h);
        rect(p,x-(i==1?10:8),mark,i==1?6:4,1,i==1?0xffa5a8adu:0xff44474cu);
    }

    int t=rail_y(db,y,h);
    if(t<y+4)t=y+4;
    if(t>y+h-5)t=y+h-5;
    int left=x-cap/2;
    rect(p,left,t-4,cap,9,0xff111214u);
    rect(p,left+1,t-3,cap-2,7,focused?AMBER:selected?0xffaa783bu:0xff787d84u);
    rect(p,left+1,t-3,cap-2,1,focused?0xffffe7bfu:selected?0xffffcf81u:0xffcdd0d4u);
    rect(p,left+2,t-1,cap-4,1,0xff414246u);
    rect(p,left+2,t+1,cap-4,1,0xff414246u);
    rect(p,left+2,t,cap-4,1,focused?0xff302412u:selected?0xffffe7bf:WHITE);
    rect(p,left+1,t+3,cap-2,1,0xff38393bu);
}
static void mixer_key(const struct resample_painter*p,const char*label,int x,int y,int w,int h,
                      unsigned size,uint32_t ink,int lit)
{
    if(!*label)return;
    rect(p,x,y,w,h,lit?SELECTED:0xff202020u);
    rect(p,x,y+h-1,w,1,lit?ink:LINE);
    text(p,label,x+2,y,w-4,h-1,size,ink,1);
}
static uint32_t fx_ink(const struct mixer_channel*c)
{
    if(!c->fx.count)return GREY;
    if(!c->fx_bypass){
        for(unsigned i=0;i<c->fx.count;++i)
            if(!c->fx.stage[i].bypass)return AMBER;
    }
    return 0xff000000u|MIXER_MUTE_RGB;
}
/* Grid cell of one Track: colour strip, Pan (top left), Fader (left),
 * meter (centre, full height), letter and S/M/fx (right). */
static void cell(const struct mixer_view_state*s,unsigned track,int x,int y,const struct resample_painter*p)
{
    const int w=80,h=77;
    const struct mixer_channel*c=s->project->track+track;
    struct mixer_mix resolved[8];mixer_scene_resolve(s->project,s->part_id,resolved);const struct mixer_mix*m=resolved+track;
    int selected=!s->master&&(s->selected&(1u<<track));
    rect(p,x,y,w,h,BLACK);
    rect(p,x+2,y+1,w-4,h-2,selected?SELECTED:PANEL);
    if(selected)border(p,x+2,y+1,w-4,h-2,s->primary==track?AMBER:GREY);
    rect(p,x+4,y+3,w-8,2,0xff000000u|c->colour);
    pan_bar(m->pan,x+5,y+8,34,p);
    fader_rail(m->level_db,selected,selected&&s->cursor==PARAM_LEVEL&&!s->name_edit,x+14,y+20,h-26,14,p);
    vmeter(s,track,x+34,y+18,12,h-24,p);
    send_meter(s,track,x+22,y+20,h-26,p);
    char letter[2]={(char)('A'+track),0};
    text(p,letter,x+50,y+6,26,22,23,s->primary==track&&!s->master?AMBER:WHITE,1);
    mixer_key(p,"S",x+53,y+27,20,13,10,m->solo?(0xff000000u|MIXER_SOLO_RGB):GREY,m->solo);
    mixer_key(p,"M",x+53,y+40,20,13,10,m->mute?(0xff000000u|MIXER_MUTE_RGB):GREY,m->mute);
    mixer_key(p,"fx",x+53,y+53,20,13,10,fx_ink(c),c->fx.count!=0);
    unsigned destination=s->project->send_to[track];
    if(destination){
        char to[2]={(char)('A'+destination-1),0};
        text(p,to,x+52,y+65,22,11,10,m->send>0?AMBER:GREY,1);
    }
}
void mixer_view_channel_strip(const struct mixer_view_state*s,unsigned track,int x,int width,const struct resample_painter*p)
{
    /* Left area on PAD FX / KNOB FX: big name (A-H or MSTR), Fader, meter. */
    (void)width;
    const struct mixer_channel*c=track<8?s->project->track+track:s->master==2?&s->project->input:&s->project->master;
    struct mixer_mix resolved[8];const struct mixer_mix*m=track<8?(mixer_scene_resolve(s->project,s->part_id,resolved),resolved+track):s->master==2?&s->project->input.mix:&s->project->master.mix;
    rect(p,x,36,80,162,BLACK);
    rect(p,x+3,37,74,158,PANEL);
    rect(p,x+3,37,74,2,0xff000000u|c->colour);
    char name[8],stock[12];
    if(track<8)snprintf(name,sizeof(name),"%c",'A'+track);else snprintf(name,sizeof(name),s->master==2?"INPUT":"MSTR");
    snprintf(stock,sizeof(stock),"Track %c",'A'+track);
    int custom=track<8&&strcmp(c->name,stock)!=0;
    if(custom)text(p,name,x+7,41,16,12,10,0xff000000u|c->colour,1);
    track_title(p,custom?c->name:name,x+5,custom?53:43,70,custom?23:32,custom?19:track<8?34:21,WHITE,1,s->name_elapsed);
    int level_focus=s->page==MIXER_ASSIGN&&s->cursor==PARAM_LEVEL;
    char db[24];level_text(db,sizeof(db),m->level_db);text(p,db,x+2,76,76,14,11,level_focus?AMBER:WHITE,1);
    pan_bar(m->pan,x+8,93,64,p);
    fader_rail(m->level_db,1,level_focus,x+22,106,86,18,p);
    if(s->master!=2)vmeter(s,track<8?track:9,x+46,106,14,86,p);
    else input_meter(x+46,106,14,86,p);
    if(track<8)send_meter(s,track,x+32,106,86,p);
    if(track<8){
        mixer_key(p,"S",x+62,114,16,15,10,m->solo?(0xff000000u|MIXER_SOLO_RGB):GREY,m->solo);
        mixer_key(p,"M",x+62,134,16,15,10,m->mute?(0xff000000u|MIXER_MUTE_RGB):GREY,m->mute);
    }
    if(s->master==2)mixer_key(p,"M",x+62,134,16,15,10,m->mute?(0xff000000u|MIXER_MUTE_RGB):GREY,m->mute);
    mixer_key(p,"fx",x+62,154,16,15,10,fx_ink(c),c->fx.count!=0);
    unsigned destination=track<8?s->project->send_to[track]:0;
    if(destination){
        char to[2]={(char)('A'+destination-1),0};
        text(p,to,x+62,170,16,14,11,m->send>0?AMBER:GREY,1);
    }
}
void mixer_view_overview(const struct mixer_view_state*s,const struct resample_painter*p)
{
    /* Bottom row A-D, top row E-H. */
    for(unsigned i=0;i<8;++i)cell(s,i,(int)(i%4)*80,i<4?121:44,p);
}
static void master_fader(const struct mixer_view_state*s,const struct resample_painter*p,int x,int w)
{
    float db=s->project->master.mix.level_db;
    /* Match the physical fader taper: Off, -60 dB, unity, +6 dB. */
    int travel=w-8,unity=(int)lroundf(travel*103.f/127.f);
    int position=0;
    if(isfinite(db)){
        if(db<0)position=1+(int)lroundf((unity-1)*(db+60.f)/60.f);
        else position=unity+(int)lroundf((travel-unity)*db/6.f);
    }
    rect(p,x,38,w,2,0xff3b3b3bu);
    rect(p,x+4+unity,35,1,8,GREY);
    int left=x+position;
    rect(p,left,35,8,8,BLACK);
    uint32_t ink=s->master?(s->cursor==PARAM_LEVEL?AMBER:0xffaa783bu):0xff787d84u;
    rect(p,left+1,36,6,6,ink);
    rect(p,left+3,37,1,4,s->master&&s->cursor==PARAM_LEVEL?0xff302412u:WHITE);
}
static void header(const struct mixer_view_state*s,const struct resample_painter*p)
{
    rect(p,0,0,320,s->page==MIXER_OVERVIEW?44:36,BLACK);
    /* B1 B2 B3 functions on top (no "B1" text in the names). */
    int sub=s->page==MIXER_ASSIGN||s->page==MIXER_INSPECT||s->page==MIXER_KNOB_FX;
    int soloed=0;struct mixer_mix all[8];mixer_scene_resolve(s->project,s->part_id,all);
    if(!s->master)for(unsigned i=0;i<8;++i)if(s->selected&(1u<<i))soloed|=all[i].solo;
    const char*names[3]={sub?"Back":"Sample",s->page==MIXER_OVERVIEW?"Route":"","Solo"};
    int lit[3]={0,0,soloed},off=s->page==MIXER_INSPECT||s->page==MIXER_KNOB_FX;
    if(s->page==MIXER_KNOB_FX&&s->fx_editing){
        names[1]=s->shifted?"Paste":"Bypass";
        const struct mixer_channel*ch=mixer_view_selected(s);
        lit[1]=!s->shifted&&(s->fx_slot<ch->fx.count?ch->fx.stage[s->fx_slot].bypass:s->fx_slot==MIXER_COMP_SLOT&&s->comp.bypass);
        names[2]=s->shifted?"Chain Bypass":"Solo";
        lit[2]=s->shifted?mixer_view_selected(s)->fx_bypass!=0:soloed;
        off=0;
    }
    if(s->page==MIXER_INSPECT){
        static const char *previous[]={"< Sidechain","< Pads","< Sends"};
        static const char *next[]={"Sends >","Sidechain >","Pads >"};
        unsigned tab=s->route_tab<MIXER_ROUTE_TABS?s->route_tab:0;
        names[1]=previous[tab];names[2]=next[tab];lit[2]=0;off=0;
    }
    if(s->page==MIXER_ASSIGN){
        names[1]=s->shifted?"":s->pad_lock?"Pad Lock: On":"Pad Lock: Off";
    }
    if(s->page==MIXER_KNOB_FX&&s->fx_parameters){
        names[0]="FX List";names[1]="Bypass";
        const struct mixer_channel*ch=mixer_view_selected(s);
        lit[1]=s->fx_slot<ch->fx.count&&ch->fx.stage[s->fx_slot].bypass;
        if(s->shifted){names[2]="Chain Bypass";lit[2]=ch->fx_bypass!=0;}

    }
    if(s->page==MIXER_KNOB_FX&&s->fx_parameters&&s->master&&s->fx_slot==MIXER_COMP_SLOT&&s->comp.visible){
        names[1]="Bypass";lit[1]=s->comp.bypass!=0;
        names[2]=s->comp.type?"Type: Opto":"Type: Duck";lit[2]=0;off=0;
    }
    if(s->page==MIXER_KNOB_FX&&s->fx_catalog){names[0]="FX List";names[1]="Search";names[2]="";lit[1]=lit[2]=0;}
    if(s->page==MIXER_KNOB_FX&&s->shifted){
        names[1]="Paste";names[2]="Chain Bypass";
        lit[1]=0;lit[2]=mixer_view_selected(s)->fx_bypass!=0;off=0;
    }
    int input_tabs=s->master==2&&s->page==MIXER_KNOB_FX&&!s->fx_parameters&&!s->fx_catalog&&!s->fx_delete&&!s->name_edit;
    if(input_tabs){
        const char*tab[3]={"Back",s->shifted?"Paste":"FX",s->shifted?"Chain Bypass":"ROUTE"};
        for(unsigned i=0;i<3;++i){int x=i==2?213:(int)i*106,w=i==0?106:107;
            if(i==2&&s->sequence_muted)w-=25;
            int on=s->shifted?(i==2&&s->project->input.fx_bypass!=0):(i>0&&s->input_tab==i);
            uint32_t ink=s->shifted?(on?SEND:i==0?WHITE:GREY):on?AMBER:WHITE;
            mixer_key(p,tab[i],x+2,1,w-4,18,strlen(tab[i])>10?(i==2&&s->sequence_muted?10:12):14,ink,on);}
    }
    if(s->name_edit){names[0]="Cancel";names[1]=s->shifted?"Clear":"Erase";names[2]="Save";lit[1]=lit[2]=off=0;}
    else if(s->shifted&&s->page==MIXER_OVERVIEW){names[1]="Rename";names[2]="Clear Solo";lit[2]=0;}
    for(unsigned i=0;i<3&&!input_tabs;++i){int x=i==2?213:(int)i*106,w=i==0?106:107;
        if(i==2&&s->sequence_muted)w-=25;
        uint32_t active=i==1?(0xff000000u|MIXER_MUTE_RGB):(s->shifted&&s->page==MIXER_KNOB_FX?SEND:0xff000000u|MIXER_SOLO_RGB);
        int gates=!s->name_edit&&!s->shifted&&(s->page==MIXER_OVERVIEW||s->page==MIXER_ASSIGN||s->page==MIXER_KNOB_FX);
        uint32_t ink=i&&(off||(i==2&&(s->master&&gates)))?GREY:lit[i]?active:i==2&&gates?GREY:WHITE;
        mixer_key(p,names[i],x+2,1,w-4,18,strlen(names[i])>10?(i==2&&s->sequence_muted?10:12):14,ink,lit[i]);}
    if(s->sequence_muted)text(p,"SM",294,1,24,15,12,0xffff4747u,1);
    unsigned group=0;for(unsigned i=0;i<8;++i)if(s->selected&(1u<<i))++group;
    int overview=s->page==MIXER_OVERVIEW&&!s->name_edit;
    if(overview){
        char db[24];
        const struct mixer_mix*m=s->master?&s->project->master.mix:all+s->primary;
        const char*name=s->master?s->project->master.name:s->project->track[s->primary].name;
        level_text(db,sizeof(db),m->level_db);
        const unsigned heading_size=19;
        rect(p,4,23,2,17,0xff000000u|(s->master?s->project->master.colour:s->project->track[s->primary].colour));
        if(s->message&&*s->message)track_title(p,s->message,10,20,120,24,12,AMBER,0,s->name_elapsed);
        else{
            track_title(p,name,10,20,group>1?93:120,24,heading_size,WHITE,0,s->name_elapsed);
            if(group>1){
                char count[]="×0";count[sizeof(count)-2]=(char)('0'+group);
                rect(p,105,24,25,16,SELECTED);
                text(p,count,105,24,25,16,10,AMBER,1);
            }
        }
        text(p,db,133,20,73,24,heading_size,s->cursor==PARAM_LEVEL?AMBER:WHITE,1);
        rect(p,208,22,1,17,DARK);
    }
    if(s->page==MIXER_KNOB_FX&&s->fx_parameters){
        char db[24];
        level_text(db,sizeof(db),s->master?s->project->master.mix.level_db:all[s->primary].level_db);
        text(p,s->master==2?"INPUT":"FADER",8,20,45,15,10,GREY,0);
        text(p,s->master==2?"Mic / USB / Rear":db,57,20,94,15,s->master==2?9:13,WHITE,0);
    }
    int x=overview?245:204,w=overview?71:112;
    text(p,"MSTR",overview?211:160,20,overview?33:40,15,11,s->master?AMBER:GREY,1);
    rect(p,x,22,w,11,BLACK);
    if(available(s,9))for(unsigned c=0;c<2;++c){
        float v=mixer_meter_dbfs(s->meter->peak[9][c]);
        int fill=isfinite(v)?(int)lroundf((fmaxf(-60,fminf(0,v))+60)*w/60):0;
        rect(p,x,22+(int)c*6,w,5,DARK);
        if(fill>0)rect(p,x,22+(int)c*6,fill,5,(s->meter->clip_mask&(1u<<9))?SEND:AMBER);
    }else text(p,"N/A",x,21,w,13,8,GREY,1);
    if(overview){
        /* FX lamp under MSTR, same as the Track cells: lit when the Master chain holds an effect. */
        const struct mixer_channel*master=&s->project->master;
        mixer_key(p,"fx",215,31,24,12,10,fx_ink(master),master->fx.count!=0);
        master_fader(s,p,x,w);
    }
}
static void knob_cell(const struct resample_painter*p,unsigned column,const char*label,
                      const char*value,int focused,unsigned alternate)
{
    int x=(int)column*106,w=column==2?108:106;
    uint32_t ink=focused?AMBER:*label?WHITE:GREY;
    rect(p,x+2,198,w-4,40,focused?SELECTED:PANEL);
    rect(p,x+2,198,w-4,1,focused?AMBER:LINE);
    text(p,label,x+4,200,alternate?w-22:w-8,12,strlen(label)>14?9:strlen(label)>11?10:12,focused?AMBER:GREY,1);
    if(alternate)SHIFT_BADGE_COLOURED(p->rect,p->user,(int)column*106+94,200,
                                       alternate==2?0xffff4747u:WHITE);
    text(p,value,x+4,214,w-8,19,16,ink,1);
}
static void knob_scale(const struct resample_painter*p,unsigned column,double value,int bipolar)
{
    int x=column==2?217:4+(int)column*106,w=column==2?99:98;
    int start=bipolar?w/2:0;
    double position=fmax(0,fmin(1,value));
    int end=(int)lround(position*w);
    rect(p,x,235,w,2,0xff3b3b3bu);
    if(end<start)rect(p,x+end,235,start-end,2,AMBER);
    if(end>start)rect(p,x+start,235,end-start,2,AMBER);
    if(bipolar)rect(p,x+start,234,1,4,WHITE);
}
static void knob_steps(const struct resample_painter*p,unsigned column,int selected,
                       unsigned count,uint32_t ink)
{
    int x=column==2?217:4+(int)column*106,w=column==2?99:98;
    for(unsigned i=0;i<count;++i){
        int a=(int)i*w/(int)count,b=(int)(i+1)*w/(int)count;
        rect(p,x+a,235,b-a-1,2,(int)i==selected?ink:0xff3b3b3bu);
    }
}
static void input_route_rows(const struct mixer_view_state*,const struct resample_painter*);
static void rec_values(unsigned*point,unsigned*track,unsigned*wet);
static void route_row(const struct mixer_view_state*s,unsigned row,char*lab,unsigned labn,char*val,unsigned valn);
static int tuner_read(char*out,unsigned n,double*cents);
static void footer(const struct mixer_view_state*s,const struct resample_painter*p)
{
    rect(p,0,198,320,42,BLACK);
    struct mixer_mix resolved[8];
    mixer_scene_resolve(s->project,s->part_id,resolved);
    const struct mixer_mix*m=s->master?&s->project->master.mix:resolved+s->primary;
    char value[32];
    pan(value,sizeof(value),m->pan);
    knob_cell(p,0,"Pan",value,s->cursor==PARAM_PAN,0);
    knob_scale(p,0,(m->pan+1)*.5,1);
    if(s->master)knob_cell(p,1,"","",0,0);
    else if(s->shifted){
        unsigned destination=s->project->send_to[s->primary];
        if(destination)snprintf(value,sizeof(value),"Track %c",'A'+destination-1);
        else snprintf(value,sizeof(value),"Off");
        knob_cell(p,1,"Send To",value,s->cursor==PARAM_SEND,2);
        knob_steps(p,1,(int)mixer_routing_index(mixer_send_excluded(s->project,s->selected),destination),mixer_routing_choice_count(mixer_send_excluded(s->project,s->selected)),AMBER);
    }else{
        snprintf(value,sizeof(value),"%.0f%%",m->send*100.f);
        knob_cell(p,1,"Send",value,s->cursor==PARAM_SEND,1);
        knob_scale(p,1,m->send,0);
    }
    const char*name=s->master?"Green":mixer_colour_name(s->project->track[s->primary].colour);
    knob_cell(p,2,"Colour",name?name:"Custom",s->cursor==PARAM_COLOUR,0);
    if(!s->master){
        uint32_t colour=s->project->track[s->primary].colour;
        int index=mixer_colour_index(colour);
        if(index>=0)knob_steps(p,2,index,mixer_colour_count(),0xff000000u|colour);
        else rect(p,217,235,99,2,0xff000000u|colour);
    }
}
static void plot_sample(const struct resample_painter*p,int x,int y,int previous,uint32_t ink)
{
    int top=y<previous?y:previous;
    rect(p,x,top,2,abs(y-previous)+1,ink);
}
static void fx_spectrum(const struct resample_painter*p,const float*pcm,double rate,
                        int left,int top,int width,int height,uint32_t ink,int fill)
{
    float re[MIXER_FX_MONITOR_FRAMES],im[MIXER_FX_MONITOR_FRAMES]={0};
    for(unsigned i=0;i<MIXER_FX_MONITOR_FRAMES;++i){
        float w=.5f-.5f*(float)cos(6.283185307179586*i/(MIXER_FX_MONITOR_FRAMES-1));
        re[i]=pcm[i]*w;
    }
    spectrum_fft(re,im,MIXER_FX_MONITOR_FRAMES);
    for(int x=0;x<width;++x){
        float db=spectrum_column(re,im,MIXER_FX_MONITOR_FRAMES,rate,x,width);
        int h=(int)((fmaxf(-72,fminf(db,0))+72)*height/72);
        if(h)rect(p,left+x,top+height-h,1,fill?h:1,ink);
    }
}
static void eq_plot(const struct mixer_view_state*s,const struct resample_painter*p,const struct fx_eq_config*eq)
{
    const double rate=s->rate?s->rate:48000;
    const int left=42,top=80,width=260,height=93;
    const double span=log(1000.);
    const struct mixer_channel*channel=mixer_view_selected(s);
    int bypass=eq->bypass||channel->fx_bypass||(s->fx_slot<channel->fx.count&&channel->fx.stage[s->fx_slot].bypass);
    if(!bypass&&s->fx_signal)fx_spectrum(p,s->fx_signal->pcm,rate,left,top,width,height,0xe05a4419u,1);
    for(int db=-18;db<=18;db+=9){
        int y=top+(18-db)*height/36;
        char label[16];snprintf(label,sizeof(label),"%+d",db);
        rect(p,left,y,width,1,db?DARK:0xff666666u);
        text(p,label,16,y-6,23,13,10,GREY,1);
    }
    int previous=top+height/2;
    for(int x=0;x<width;++x){
        double hz=20*exp(span*x/(width-1));
        double db=fx_eq_response_db(eq,rate,fmin(hz,rate*.49));
        int y=top+(int)lround((18-fmax(-18,fmin(18,db)))*height/36);
        int start=x?(y<previous?y:previous):y;
        rect(p,left+x,start,1,x?abs(y-previous)+1:1,bypass?GREY:AMBER);
        previous=y;
    }
    for(unsigned i=0;i<eq->count;++i){
        const struct fx_eq_band*b=eq->band+i;
        int x=left+(int)lround((width-1)*fmax(0,fmin(1,log(b->hz/20)/span)));
        double db=fx_eq_response_db(eq,rate,b->hz);
        int y=top+(int)lround((18-fmax(-18,fmin(18,db)))*height/36);
        rect(p,x-2,y-2,5,5,i==s->parameter_page?WHITE:b->enabled?AMBER:GREY);
    }
    text(p,"20",left,176,30,10,10,GREY,0);
    int mid=left+(int)lround((width-1)*log(1000./20)/span);
    text(p,"1k",mid-15,176,30,10,10,GREY,1);
    text(p,"20k",270,177,32,10,10,GREY,0);
}
static int signal_width(float db)
{
    return isfinite(db)?(int)lroundf(228*fmaxf(0,fminf(1,(db+60)/60))):0;
}
static void signal_meter(const struct resample_painter*p,const char*label,float peak,int y,int height,
                         uint32_t ink,float threshold)
{
    rect(p,65,y,228,height,WELL);
    int width=signal_width(mixer_meter_dbfs(peak));
    if(width)rect(p,65,y,width,height,ink);
    for(int x=103;x<293;x+=38)rect(p,x,y,1,height,WELL);
    if(isfinite(threshold)){
        int marker=signal_width(threshold);
        rect(p,65+(marker<228?marker:227),y-2,1,height+4,WHITE);
    }
    text(p,label,15,y-3,48,12,10,WHITE,0);
}
static void effect_send_meter(const struct mixer_view_state*s,const struct resample_painter*p)
{
    unsigned track=s->primary;
    rect(p,10,186,288,12,BLACK);
    if(s->master||!available(s,track)||!(s->meter->return_mask&(1u<<track)))return;
    signal_meter(p,"Send In",s->meter->return_peak[track],189,5,SEND,NAN);
}
static void fx_trace(const struct resample_painter*p,const float*pcm,uint32_t ink,int envelope)
{
    const int left=16,width=288,centre=111;
    int previous=centre;
    for(int x=0;x<width;x+=2){
        unsigned start=(unsigned)x*MIXER_FX_MONITOR_FRAMES/width;
        float v=pcm[start];
        if(envelope){
            unsigned end=(unsigned)(x+2)*MIXER_FX_MONITOR_FRAMES/width;
            for(unsigned i=start;i<end;++i)v=fmaxf(fabsf(v),fabsf(pcm[i]));
        }
        int y=centre-(int)lroundf(19*fmaxf(-1,fminf(1,v)));
        plot_sample(p,left+x,y,x?previous:y,ink);
        previous=y;
    }
}
static void fx_dynamics(const struct resample_painter*p,const struct fx_catalog_entry*e,
                        const struct fx_stage*stage,const struct mixer_fx_monitor*m)
{
    char value[64];
    if(stage->kind==FX_STAGE_SIDECHAIN){
        unsigned source=(unsigned)fx_sidechain_value(0,stage->parameter[0]);
        float threshold=fx_sidechain_value(1,stage->parameter[1]);
        if(source)snprintf(value,sizeof(value),"Key %c (detector)  /  Threshold %.1f dBFS",'A'+source-1,threshold);
        else snprintf(value,sizeof(value),"Key off  /  Threshold %.1f dBFS",threshold);
        text(p,value,15,76,285,15,10,GREY,0);
        signal_meter(p,"Key",m?m->key_peak:0,94,8,KEY,threshold);
        signal_meter(p,"Input",m?m->input_peak:0,112,8,BLUE,NAN);
        signal_meter(p,"Output",m?m->output_peak:0,130,8,AMBER,NAN);
        float reduction=m?m->gain_reduction_db:0;
        int width=(int)lroundf(228*fminf(24,reduction)/24);
        rect(p,65,148,228,8,WELL);
        if(width)rect(p,293-width,148,width,8,0xffffa16cu);
        for(int tick=65;tick<293;tick+=57)rect(p,tick,148,1,8,LINE);
        text(p,"GR",15,145,48,12,10,WHITE,0);
        snprintf(value,sizeof(value),"-%.1f dB",reduction);
        text(p,m&&reduction>0?value:m?"0.0 dB":"--",65,158,228,12,10,GREY,1);
        return;
    }
    text(p,"Signal levels  ·  dBFS",15,76,270,15,10,GREY,0);
    signal_meter(p,"Input",m?m->input_peak:0,103,9,BLUE,NAN);
    signal_meter(p,"Output",m?m->output_peak:0,124,9,AMBER,NAN);
    /* Native plug-ins provide normalized values through the Mixer backend. */
    for(unsigned i=0;i<e->count;++i){
        const char*label=e->parameter[i];
        if(strcmp(label,"Thresh")&&strcmp(label,"Threshold")&&strcmp(label,"Ceiling"))continue;
        int offset=(int)lroundf(227*stage->parameter[i]);
        rect(p,65,148,228,5,DARK);
        rect(p,65+offset,146,1,9,WHITE);
        snprintf(value,sizeof(value),"%s %.0f%%",!strcmp(label,"Ceiling")?"Ceiling":"Threshold",stage->parameter[i]*100.f);
        text(p,value,64,157,229,13,10,WHITE,0);
        break;
    }
}
static void eq_knob_bars(const struct mixer_view_state*s,const struct resample_painter*p,
                         const struct fx_eq_config*eq)
{
    const struct fx_eq_band*b=eq->band+(s->parameter_page<eq->count?s->parameter_page:0);
    double value[3]={log(b->hz/20)/log(1000.),(b->gain_db+18)/36.,0};
    if(s->shifted){
        value[0]=b->enabled;value[1]=log(b->q/.2)/log(80.);
        value[2]=(eq->output_db+24)/36.;
    }
    for(unsigned k=0;k<3;++k){
        int x=k==2?217:4+(int)k*106,w=k==2?99:98;
        if(k==2&&!s->shifted){
            for(unsigned type=0;type<6;++type){
                int a=(int)type*w/6,end=(int)(type+1)*w/6;
                rect(p,x+a,235,end-a-1,2,type==b->type?AMBER:0xff3b3b3bu);
            }
        }else{
            rect(p,x,235,w,2,0xff3b3b3bu);
            if(k!=1||s->shifted||b->type<FX_EQ_HIGH_PASS){
                int fill=(int)lround(w*fmax(0,fmin(1,value[k])));
                rect(p,x,235,fill,2,AMBER);
            }
        }
    }
}
#include "fx_visual.inc"
static void effect_editor(const struct mixer_view_state*s,const struct resample_painter*p,const struct fx_stage*stage)
{
    const struct fx_catalog_entry*e=fx_catalog_find(stage->kind==FX_STAGE_EQ?0:stage->native_id);
    const char*label[3]={"","",""};
    char value[3][20]={{0}},status[64];
    unsigned alternate[3]={0};
    const struct mixer_channel*ch=mixer_view_selected(s);
    rect(p,0,36,320,204,BLACK);
    uint32_t accent=0xff000000u|mixer_fx_slot_rgb(s->fx_slot);
    char slot[8];snprintf(slot,sizeof(slot),"FX %u",s->fx_slot+1);
    rect(p,6,37,308,34,PANEL);
    rect(p,6,37,2,34,accent);
    text(p,slot,13,39,37,12,10,accent,0);
    int is_eq_message=stage->kind==FX_STAGE_EQ&&s->message&&*s->message;
    track_title(p,ch->name,52,39,is_eq_message?94:183,12,10,GREY,0,s->name_elapsed);
    if(is_eq_message)text(p,s->message,148,39,158,12,10,AMBER,1);
    else if(stage->bypass||ch->fx_bypass)
        text(p,ch->fx_bypass?"CHAIN OFF":"BYPASS",237,39,69,12,10,SEND,1);
    if(stage->native_id==0x61464153u&&fx_amp_model_index(stage)){
        char title[48];snprintf(title,sizeof title,"Amp Sim / %s",fx_amp_model_name(fx_amp_model_index(stage)));
        text(p,title,13,50,229,20,17,WHITE,0);
    }else text(p,fx_catalog_stage_name(stage),13,50,229,20,18,WHITE,0);
    int is_eq=stage->kind==FX_STAGE_EQ;
    rect(p,6,72,308,112,PANEL);
    if(is_eq&&stage->eq.count){
        const struct fx_eq_config*eq=&stage->eq;
        unsigned index=s->parameter_page<eq->count?s->parameter_page:0;
        const struct fx_eq_band*b=eq->band+index;
        eq_plot(s,p,eq);
        snprintf(status,sizeof(status),"Point %u/%u",index+1,eq->count);
        text(p,status,239,51,68,18,11,accent,1);
        static const char*types[]={"Bell","Low Shelf","High Shelf","High Pass","Low Pass","Notch"};
        for(unsigned k=0;k<3;++k)alternate[k]=s->shifted?2:1;
        if(s->shifted){
            label[0]="Point";label[1]="Q";label[2]="Output";
            snprintf(value[0],sizeof(value[0]),"%s",b->enabled?"On":"Off");
            snprintf(value[1],sizeof(value[1]),"%.2f",b->q);
            snprintf(value[2],sizeof(value[2]),"%+.1f dB",eq->output_db);
        }else{
            label[0]="Freq";label[1]="Gain";label[2]="Type";
            snprintf(value[0],sizeof(value[0]),"%.0f Hz",b->hz);
            if(b->type==FX_EQ_HIGH_PASS||b->type==FX_EQ_LOW_PASS||b->type==FX_EQ_NOTCH)
                snprintf(value[1],sizeof(value[1]),"--");
            else snprintf(value[1],sizeof(value[1]),"%+.1f dB",b->gain_db);
            snprintf(value[2],sizeof(value[2]),"%s",types[b->type]);
        }
    }else if(is_eq){
        eq_plot(s,p,&stage->eq);
        text(p,"0 points",239,51,68,18,11,accent,1);
        if(s->shifted){
            alternate[2]=2;label[2]="Output";
            snprintf(value[2],sizeof(value[2]),"%+.1f dB",stage->eq.output_db);
        }
    }else{
        unsigned pages=fx_catalog_page_count(e);
        if(!pages)pages=1;
        if(pages>1){
            snprintf(status,sizeof(status),"%u / %u",s->parameter_page+1,pages);
            text(p,status,249,51,57,18,11,accent,1);
        }
        struct fx_stage visual_stage=*stage;visual_stage.bypass|=ch->fx_bypass;
        const struct mixer_fx_monitor*m=visual_stage.bypass?NULL:s->fx_signal;
        fxv_page=s->parameter_page;
        if(s->fx_illustrated&&fx_visual_paint(p,e,&visual_stage,m,s->rate?s->rate:48000,
                (e?e->id:0)^(s->fx_slot<<28)^((s->master?8u:s->primary)<<24),s->now_ms)){
            /* Illustrated view drawn above; knobs and status follow. */
        }else if(e&&!strcmp(e->category,"Dynamics")){
            fx_dynamics(p,e,stage,m);
        }else{
            int filter=e&&!strcmp(e->category,"Filter");
            int envelope=e&&(!strcmp(e->category,"Delay")||!strcmp(e->category,"Reverb"));
            text(p,filter?"Spectrum":envelope?"Envelope":"Waveform",15,74,142,12,10,GREY,0);
            text(p,"Input",202,74,48,12,10,BLUE,1);
            text(p,"Output",253,74,50,12,10,AMBER,1);
            rect(p,15,87,290,43,WELL);
            if(filter){
                double rate=s->rate?s->rate:48000;
                if(m){
                    fx_spectrum(p,m->input_pcm,rate,16,87,288,31,0xff386477u,1);
                    fx_spectrum(p,m->pcm,rate,16,87,288,31,AMBER,0);
                }
                text(p,"20 Hz",16,118,54,11,9,GREY,0);
                text(p,"1 kHz",154,118,50,11,9,GREY,1);
                text(p,"20 kHz",250,118,53,11,9,GREY,1);
            }else{
                rect(p,16,111,288,1,DARK);
                if(m){
                    fx_trace(p,m->input_pcm,BLUE,envelope);
                    fx_trace(p,m->pcm,AMBER,envelope);
                }
            }
            signal_meter(p,"Input",m?m->input_peak:0,138,8,BLUE,NAN);
            signal_meter(p,"Output",m?m->output_peak:0,155,8,AMBER,NAN);
        }
        int layer=s->shifted&&fx_catalog_shift_layer(e,s->parameter_page);
        for(unsigned knob=0;knob<3;++knob){
            unsigned position=s->parameter_page*6+knob;
            if(fx_catalog_parameter_index(e,position+3)>=0)alternate[knob]=layer?2:1;
            int index=fx_catalog_parameter_index(e,position+(layer?3:0));
            if(index<0)continue;
            label[knob]=fx_catalog_parameter_label(e,(unsigned)index);
            if(stage->kind==FX_STAGE_SIDECHAIN){
                float v=fx_sidechain_value((unsigned)index,stage->parameter[index]);
                const char*formats[]={"","%.1f dB","%.1f:1","%.1f ms","%.0f ms","%.0f%%"};
                if(index==0){
                    if(v)snprintf(value[knob],sizeof(value[knob]),"Track %c",'A'+(int)v-1);
                    else snprintf(value[knob],sizeof(value[knob]),"Off");
                }else snprintf(value[knob],sizeof(value[knob]),formats[index],v);
            }else if(e&&e->id==0x6153436c&&index==0)
                snprintf(value[knob],sizeof(value[knob]),"%+.1f dB",80.f*stage->parameter[0]-40.f);
            else if(fx_catalog_choice_count(e,(unsigned)index))
                snprintf(value[knob],sizeof(value[knob]),"%s",fx_catalog_choice_name(e,(unsigned)index,fx_catalog_parameter_value(stage,(unsigned)index)));
            else snprintf(value[knob],sizeof(value[knob]),"%.0f%%",fx_catalog_parameter_value(stage,(unsigned)index)*100.f);
        }
        if(pages>1)snprintf(status,sizeof(status),"%s | DATA: page%s",fx_catalog_page_name(e,s->parameter_page),layer?" | Shift":"");
        else snprintf(status,sizeof(status),"%s",layer?"Shift layer":fx_catalog_shift_layer(e,0)?"Hold Shift: more controls":"");
    }
    if(!is_eq)text(p,s->message&&*s->message?s->message:status,10,171,300,13,10,GREY,1);
    else if(s->shifted)text(p,"Shift+DATA: +",84,176,86,10,8,GREY,1);
    for(unsigned i=0;i<3;++i)knob_cell(p,i,label[i],value[i],0,alternate[i]);
    if(is_eq){
        if(stage->eq.count)eq_knob_bars(s,p,&stage->eq);
        else if(s->shifted)knob_scale(p,2,(stage->eq.output_db+24)/36,0);
    }else for(unsigned k=0;k<3;++k){
        int index=fx_catalog_parameter_index(e,s->parameter_page*6+(s->shifted&&fx_catalog_shift_layer(e,s->parameter_page)?3:0)+k);
        if(index<0)continue;
        if(stage->kind==FX_STAGE_SIDECHAIN&&index==0)
            knob_steps(p,k,(int)mixer_routing_index(s->master?0:1u<<s->primary,(unsigned)fx_sidechain_value(0,stage->parameter[0])),mixer_routing_choice_count(s->master?0:1u<<s->primary),AMBER);
        else if(fx_catalog_choice_count(e,(unsigned)index)){
            unsigned count=fx_catalog_choice_count(e,(unsigned)index);
            knob_steps(p,k,(int)lroundf(fx_catalog_parameter_value(stage,(unsigned)index)*(count-1)),count,AMBER);
        }else knob_scale(p,k,fx_catalog_parameter_value(stage,(unsigned)index),0);
    }
    mixer_view_meters(s,1u<<(s->master?8:s->primary),p);
}
static void fx_hint(const struct resample_painter*p,const char*label,const char*action,int y)
{
    text(p,label,92,y,67,15,10,AMBER,0);
    text(p,action,162,y,151,15,10,WHITE,0);
}
/* Parameter page of the pinned Sample compressor: the same settings as the Shift+Pad 5 screen. */
static void comp_editor(const struct mixer_view_state*s,const struct resample_painter*p)
{
    const struct mixer_sample_comp*k=&s->comp;
    const struct mixer_channel*ch=&s->project->master;
    rect(p,0,36,320,204,BLACK);
    rect(p,6,37,308,34,PANEL);rect(p,6,37,2,34,AMBER);
    text(p,"SAMPLE",13,39,45,12,10,AMBER,0);
    track_title(p,ch->name,60,39,175,12,10,GREY,0,s->name_elapsed);
    if(!k->available)text(p,"N/A",237,39,69,12,10,GREY,1);
    else if(k->bypass||ch->fx_bypass)text(p,"BYPASS",237,39,69,12,10,SEND,1);
    text(p,"Sample Comp",13,50,229,20,18,WHITE,0);
    text(p,k->type?"OptoCat":"Duck",249,51,57,18,11,AMBER,1);
    rect(p,6,72,308,112,PANEL);
    struct fx_stage st;memset(&st,0,sizeof(st));st.kind=FX_STAGE_NATIVE;st.native_id=FX_OWN_OPTO;
    st.parameter[0]=(k->threshold+48.f)/48.f;st.parameter[1]=(k->ratio-1.f)/7.f;st.parameter[2]=(k->gain+6.f)/12.f;
    st.parameter[3]=logf(fmaxf(1.f,k->attack))/logf(100.f);st.parameter[4]=logf(fmaxf(100.f,k->release)/100.f)/logf(40.f);
    st.parameter[5]=k->knee/12.f;st.parameter[6]=k->warm?1.f:0.f;
    struct mixer_fx_monitor m;memset(&m,0,sizeof(m));m.gain_reduction_db=k->bypass?0.f:k->reduction;
    m.input_peak=powf(10.f,fminf(0.f,fmaxf(-90.f,k->level))/20.f);m.output_peak=m.input_peak*powf(10.f,(k->gain-m.gain_reduction_db)/20.f);
    struct comp_config cfg={k->threshold,k->ratio,k->gain,k->bypass||ch->fx_bypass||!k->available,k->attack,k->release,k->knee,k->warm};
    fxv_idle=!k->available||k->bypass||ch->fx_bypass;fxv_now=fxv_idle?0:s->now_ms;
    fxv_history_bypass=(unsigned)fxv_idle;
    if(fxv_idle){memset(&m,0,sizeof m);}
    fxv_history_push(0x5343u,s->now_ms,&m);
    {
        struct resample_painter dim={(void*)p,fxv_dim_rect,fxv_dim_text};
        fxv_opto_draw(fxv_idle?&dim:p,&cfg,fxv_idle?-60:k->level,m.gain_reduction_db,&m);
    }
    fxv_idle=0;fxv_history_bypass=0;
    const char*label[3];char value[3][20];unsigned layer=s->shifted?2:1;
    if(s->shifted){
        label[0]="Attack";label[1]="Release";label[2]="Knee";
        snprintf(value[0],sizeof(value[0]),"%.0f ms",(double)k->attack);
        snprintf(value[1],sizeof(value[1]),"%.0f ms",(double)k->release);
        snprintf(value[2],sizeof(value[2]),"%.1f dB",(double)k->knee);
    }else{
        label[0]="Threshold";label[1]="Ratio";label[2]="Gain";
        snprintf(value[0],sizeof(value[0]),"%.1f dB",(double)k->threshold);
        snprintf(value[1],sizeof(value[1]),"%.1f:1",(double)k->ratio);
        snprintf(value[2],sizeof(value[2]),"%+.1f dB",(double)k->gain);
    }
    text(p,s->message&&*s->message?s->message:k->available?(s->shifted?(k->warm?"Shift+DATA: Warm On":"Shift+DATA: Warm Off"):"Hold Shift: Attack / Release / Knee"):"Open the compressor once (Shift+Pad 5)",10,171,300,13,10,GREY,1);
    for(unsigned i=0;i<3;++i)knob_cell(p,i,label[i],value[i],0,layer);
    for(unsigned i=0;i<3;++i){
        double v=st.parameter[s->shifted?3+i:i];
        knob_scale(p,i,fmax(0.0,fmin(1.0,v)),0);
    }
    mixer_view_meters(s,1u<<8,p);
}
extern int mixer_stock_icon(void*,unsigned,int,int,int) __attribute__((weak));
static void fx_stock_icon(const struct resample_painter*p,const struct fx_catalog_entry*e,int x,int y){
 if(!mixer_stock_icon||!p->user||!e)return;
 unsigned icon=3;
 if(!strcmp(e->category,"Modulation"))icon=4;
 else if(!strcmp(e->category,"Drive"))icon=5;
 else if(!strcmp(e->category,"Filter")||!strcmp(e->category,"EQ"))icon=6;
 else if(!strcmp(e->category,"Dynamics"))icon=7;
 else if(!strcmp(e->category,"Emulator"))icon=8;
 mixer_stock_icon(p->user,icon,x,y,18);
}
static void track_fx(const struct mixer_view_state*s,const struct resample_painter*p)
{
    const struct mixer_channel*ch=mixer_view_selected(s);
    const struct fx_stage*stage=s->fx_slot<ch->fx.count?ch->fx.stage+s->fx_slot:NULL;
    rect(p,0,198,320,42,BLACK);
    char counter[16];
    snprintf(counter,sizeof(counter),"%u / %u",ch->fx.count,MIXER_FX_SLOTS);
    rect(p,85,37,231,21,PANEL);
    {char title[24]="";const char*q=fx_mixer_search_get();
    if(s->fx_catalog&&*q)snprintf(title,sizeof(title),"Find: %.14s",q);
    text(p,s->fx_catalog?(*q?title:"Add Effect"):"FX Chain",92,37,143,21,15,WHITE,0);}
    if(s->fx_catalog)text(p,fx_mixer_category_name(s->catalog_filter),240,38,69,19,10,AMBER,1);
    else text(p,ch->fx_bypass?"CHAIN OFF":counter,240,38,69,19,10,ch->fx_bypass?SEND:GREY,1);
    if(s->fx_catalog){
        unsigned total=fx_mixer_filtered_count(s->catalog_filter);
        unsigned first=s->cursor>2?s->cursor-2:0;
        if(first+6>total)first=total>6?total-6:0;
        for(unsigned row=0;row<6&&first+row<total;++row){
            unsigned index=first+row;
            int y=61+(int)row*22;
            int selected=index==s->cursor;
            const struct fx_catalog_entry*entry=fx_mixer_catalog_at(fx_mixer_filtered_index(s->catalog_filter,index));
            rect(p,85,y,228,20,selected?SELECTED:PANEL);
            if(selected)rect(p,85,y,2,20,AMBER);
            fx_stock_icon(p,entry,92,y+1);
            text(p,entry->name,114,y,131,20,strlen(entry->name)>18?11:14,selected?AMBER:WHITE,0);
            text(p,entry->category,247,y,63,20,9,GREY,1);
        }
        rect(p,315,61,2,130,DARK);
        rect(p,315,61+(int)(first*130/total),2,(int)(6*130/total),GREY);
        rect(p,85,202,124,32,PANEL);
        text(p,"TURN DATA",92,202,52,15,10,AMBER,0);text(p,"Browse",146,202,60,15,10,WHITE,0);
        text(p,"PRESS DATA",92,217,52,15,10,AMBER,0);text(p,"Add effect",146,217,60,15,10,WHITE,0);
        knob_cell(p,2,"K3  Filter",fx_mixer_category_name(s->catalog_filter),1,0);
        return;
    }
    const int input_route=s->master==2&&s->input_tab==2;
    if(input_route)input_route_rows(s,p);
    const int comp_row=s->master==1&&s->comp.visible;
    const int pitch=comp_row?22:27,row_h=pitch-3;
    for(unsigned i=0;i<MIXER_FX_SLOTS&&!input_route;++i){
        const struct fx_stage*effect=i<ch->fx.count?ch->fx.stage+i:NULL;
        int y=62+(int)i*pitch;
        int selected=i==s->fx_slot;
        uint32_t ink=0xff000000u|mixer_fx_slot_rgb(i);
        char number[2]={(char)('1'+i),0};
        int badge=(row_h-16)/2;
        rect(p,85,y,231,row_h,selected?SELECTED:PANEL);
        if(selected)rect(p,85,y,2,row_h,ink);
        rect(p,92,y+badge,19,16,WELL);
        text(p,number,92,y+badge,19,16,12,ink,1);
        const char*name=effect?fx_catalog_stage_name(effect):"Empty";
        int width=effect&&!effect->bypass?170:120;
        if(effect)fx_stock_icon(p,fx_catalog_find(effect->kind==FX_STAGE_EQ?0:effect->native_id),117,y+(row_h-18)/2);
        text(p,name,effect?140:117,y,width,row_h,strlen(name)>18&&width<160?11:14,effect?WHITE:GREY,0);
        if(effect&&effect->bypass)text(p,"BYPASS",263,y,48,row_h,9,SEND,1);
        else if(!effect&&i==ch->fx.count)text(p,"+ ADD",263,y,48,row_h,9,ink,1);
    }
    if(comp_row){
        /* Pinned Master row: the Sample-mode compressor (the same one as Shift+Pad 5). */
        const struct mixer_sample_comp*k=&s->comp;
        int y=62+(int)MIXER_FX_SLOTS*pitch,selected=s->fx_slot==MIXER_COMP_SLOT;
        rect(p,85,y,231,row_h,selected?SELECTED:PANEL);
        if(selected)rect(p,85,y,2,row_h,AMBER);
        rect(p,92,y+(row_h-16)/2,19,16,WELL);
        text(p,"S",92,y+(row_h-16)/2,19,16,12,AMBER,1);
        text(p,"Sample Comp",117,y,110,row_h,14,k->available?WHITE:GREY,0);
        char tag[16];
        if(!k->available)snprintf(tag,sizeof(tag),"N/A");
        else if(k->bypass)snprintf(tag,sizeof(tag),"BYPASS");
        else snprintf(tag,sizeof(tag),"%s",k->type?"OPTO":"DUCK");
        text(p,tag,255,y,56,row_h,9,k->bypass||!k->available?(k->bypass?SEND:GREY):AMBER,1);
        if(k->available&&!k->bypass){char gr[16];snprintf(gr,sizeof(gr),"-%.1f",(double)k->reduction);text(p,gr,225,y,32,row_h,9,GREY,1);}
    }
    if(s->message&&*s->message)text(p,s->message,89,174,222,20,11,AMBER,0);
    else if(!(s->master==2&&s->input_tab==2))text(p,s->master==2?(input_fx_status?input_fx_status():"Input FX: Mic / USB / Rear"):s->master&&s->comp.visible?"S = Sample compressor (Shift+Pad 5)":"Pad 15: Input FX  /  9–12: FX",89,174,222,20,10,GREY,0);
    if(s->master==2&&s->input_tab==2){
        int rear=rear_setting&&rear_native_source&&rear_native_source()==1;
        unsigned rows=rear?6u:5u,row=s->cursor<rows?s->cursor:0,rv[3];
        rec_values(rv,rv+1,rv+2);
        char lab[24],val[24];route_row(s,row,lab,sizeof lab,val,sizeof val);
        knob_cell(p,0,lab,val,s->input_edit!=0,0);
        if(row==0){double cents;char tb[24];tuner_read(tb,sizeof tb,&cents);knob_scale(p,0,(cents+50.)/100.,1);}
        else if(row==2)knob_steps(p,0,(int)rv[0],3,AMBER);
        else if(row==3)knob_steps(p,0,(int)rv[1],8,AMBER);
        else if(row==4)knob_steps(p,0,(int)rv[2],2,AMBER);
        else if(row==5){unsigned mode=rear_setting?rear_setting(0,REAR_MODE):0;if(mode>REAR_MIX)mode=0;knob_steps(p,0,mode?(int)mode-1:-1,4,AMBER);}
        if(!rear){ /* Mic / USB: K2 is the Input channel Pan */
            char pv[24];pan(pv,sizeof pv,s->project->input.mix.pan);
            knob_cell(p,1,"Pan",pv,0,0);knob_scale(p,1,(s->project->input.mix.pan+1)*.5,1);
            knob_cell(p,2,"","",0,0);
        }else for(unsigned ch=0;ch<2;++ch){
            char label[16],value[24];snprintf(label,sizeof label,s->shifted?"Level %c":"Pan %c",ch?'R':'L');
            unsigned n=rear?rear_setting(ch,s->shifted?REAR_LEVEL:REAR_PAN):0;
            if(!rear)snprintf(value,sizeof value,"--");
            else if(s->shifted)snprintf(value,sizeof value,"%u %%",n);
            else if(n==50)snprintf(value,sizeof value,"C");
            else snprintf(value,sizeof value,"%c%u",n<50?'L':'R',n<50?50-n:n-50);
            knob_cell(p,1+ch,label,value,0,s->shifted?2:1);
            if(rear)knob_scale(p,1+ch,n/100.,!s->shifted);
        }
        return;
    }
    char slot_label[16];
    snprintf(slot_label,sizeof slot_label,"SLOT %u",s->fx_slot+1);
    text(p,slot_label,5,204,70,12,9,GREY,1);
    const char*status=ch->fx_bypass?"CHAIN OFF":stage?(stage->bypass?"BYPASS":"ACTIVE"):
        s->fx_slot==MIXER_COMP_SLOT?(s->comp.bypass?"BYPASS":"ACTIVE"):"EMPTY";
    text(p,status,5,220,70,13,10,stage&&!stage->bypass&&!ch->fx_bypass?AMBER:GREY,1);
    rect(p,85,202,231,32,PANEL);
    if(s->shifted){
        fx_hint(p,"SHIFT + DATA","Turn move / Press bypass",202);
    }else{
        fx_hint(p,"DATA",stage||s->fx_slot==MIXER_COMP_SLOT?"Turn select / Press open":"Turn select / Press add",202);
    }
    if(stage)fx_hint(p,"ERASE","Remove effect",217);
}
static void route_connections(const struct mixer_view_state*s,const struct resample_painter*p)
{
    rect(p,0,36,320,204,BLACK);
    const struct mixer_channel *ch=mixer_view_selected(s);
    char label[64],value[32];
    int sends=s->route_tab==MIXER_ROUTE_SENDS;
    text(p,sends?"Routing / Sends":"Routing / Sidechain",8,38,193,22,17,WHITE,0);
    text(p,ch->name,204,39,108,20,12,AMBER,1);
    if(sends){
        text(p,"FROM",8,59,101,12,10,GREY,0);
        text(p,"TO",153,59,96,12,10,GREY,0);
        text(p,"SEND",262,59,50,12,10,GREY,1);
        for(unsigned i=0;i<MIXER_TRACKS;++i){
            int y=73+(int)i*14,active=!s->master&&i==s->primary;
            unsigned to=s->project->send_to[i];
            float amount=s->project->track[i].mix.send;
            uint32_t ink=to&&amount>0?SEND:GREY;
            rect(p,4,y,312,13,active?SELECTED:PANEL);
            rect(p,6,y+2,2,9,0xff000000u|s->project->track[i].colour);
            snprintf(label,sizeof(label),"%c  %s",'A'+i,s->project->track[i].name);
            track_title(p,label,12,y,96,13,11,active?AMBER:WHITE,0,s->name_elapsed);
            rect(p,113,y+6,29,1,ink);
            text(p,">",139,y,10,13,11,ink,1);
            if(to)snprintf(label,sizeof(label),"%c  %s",'A'+to-1,s->project->track[to-1].name);
            else snprintf(label,sizeof(label),"Off");
            track_title(p,label,154,y,100,13,11,to?WHITE:GREY,0,s->name_elapsed);
            snprintf(value,sizeof(value),"%.0f%%",amount*100);
            text(p,value,263,y,49,13,11,to?WHITE:GREY,1);
        }
        char from[12],to[12],amount[16];
        snprintf(from,sizeof(from),s->master?"Master":"Track %c",'A'+s->primary);
        unsigned destination=s->project->send_to[s->primary];
        if(s->master)snprintf(to,sizeof(to),"--");
        else if(destination)snprintf(to,sizeof(to),"Track %c",'A'+destination-1);
        else snprintf(to,sizeof(to),"Off");
        if(s->master)snprintf(amount,sizeof(amount),"--");
        else snprintf(amount,sizeof(amount),"%.0f%%",ch->mix.send*100);
        knob_cell(p,0,"From",from,s->route_focus==0,0);
        knob_cell(p,1,"To",to,s->route_focus==1,0);
        knob_cell(p,2,"Send",amount,s->route_focus==2,0);
        knob_steps(p,0,s->master?8:(int)s->primary,9,AMBER);
        knob_steps(p,1,s->master?-1:(int)mixer_routing_index(mixer_send_excluded(s->project,1u<<s->primary),destination),mixer_routing_choice_count(mixer_send_excluded(s->project,1u<<s->primary)),AMBER);
        knob_scale(p,2,s->master?0:ch->mix.send,0);
    }else{
        text(p,"KEY SOURCE",8,61,96,12,10,KEY,0);
        text(p,"TARGET FX SLOT",136,61,176,12,10,GREY,0);
        for(unsigned slot=0;slot<MIXER_FX_SLOTS;++slot){
            int y=77+(int)slot*26,active=slot==s->fx_slot;
            const struct fx_stage *stage=slot<ch->fx.count?ch->fx.stage+slot:NULL;
            int sidechain=stage&&stage->kind==FX_STAGE_SIDECHAIN;
            unsigned key=sidechain?(unsigned)fx_sidechain_value(0,stage->parameter[0]):0;
            uint32_t ink=sidechain&&key?KEY:GREY;
            rect(p,4,y,312,24,active?SELECTED:PANEL);
            if(active)rect(p,4,y,2,24,AMBER);
            if(key)snprintf(label,sizeof(label),"%c  %s",'A'+key-1,s->project->track[key-1].name);
            else snprintf(label,sizeof(label),sidechain?"Off":"--");
            track_title(p,label,11,y,96,24,12,ink,0,s->name_elapsed);
            for(int x=112;x<137;x+=6)rect(p,x,y+11,3,1,ink);
            text(p,">",137,y,10,24,12,ink,1);
            snprintf(label,sizeof(label),"%u  %s",slot+1,sidechain?"Sidechain":stage?fx_catalog_stage_name(stage):"Empty");
            text(p,label,152,y,158,13,12,stage?WHITE:GREY,0);
            if(sidechain)snprintf(label,sizeof(label),stage->bypass||ch->fx_bypass?"Bypassed":"Threshold %.1f dB",fx_sidechain_value(1,stage->parameter[1]));
            else snprintf(label,sizeof(label),stage?"No external key input":"Press DATA: add sidechain");
            text(p,label,152,y+13,158,11,9,sidechain&&(stage->bypass||ch->fx_bypass)?SEND:GREY,0);
        }
        const struct fx_stage *stage=s->fx_slot<ch->fx.count?ch->fx.stage+s->fx_slot:NULL;
        int sidechain=stage&&stage->kind==FX_STAGE_SIDECHAIN;
        unsigned key=sidechain?(unsigned)fx_sidechain_value(0,stage->parameter[0]):0;
        if(!sidechain)snprintf(value,sizeof(value),"--");
        else if(key)snprintf(value,sizeof(value),"Track %c",'A'+key-1);
        else snprintf(value,sizeof(value),"Off");
        knob_cell(p,0,"Key",value,0,0);
        snprintf(value,sizeof(value),"Slot %u",s->fx_slot+1);knob_cell(p,1,"FX",value,1,0);
        snprintf(value,sizeof(value),s->master?"Master":"Track %c",'A'+s->primary);
        knob_cell(p,2,"To",value,0,0);
        knob_steps(p,0,sidechain?(int)mixer_routing_index(s->master?0:1u<<s->primary,key):-1,mixer_routing_choice_count(s->master?0:1u<<s->primary),KEY);
        knob_steps(p,1,(int)s->fx_slot,4,AMBER);
        knob_steps(p,2,s->master?8:(int)s->primary,9,AMBER);
    }
    text(p,s->message&&*s->message?s->message:sends?
         "Turn DATA: change / Press: focus":"DATA: FX slot / Press: add or edit",
         8,185,304,12,10,s->message&&*s->message?AMBER:GREY,0);
}
extern unsigned music_rec_get(unsigned field) __attribute__((weak));
static void rec_values(unsigned*point,unsigned*track,unsigned*wet)
{
    *point=music_rec_get?music_rec_get(0):0;*track=music_rec_get?music_rec_get(1):0;*wet=music_rec_get?music_rec_get(2):1;
    if(*point>2){*point=2;}
    if(*track>7){*track=0;}
    if(*wet>1){*wet=1;}
}
extern int rear_native_monitor(void) __attribute__((weak));
extern int input_fx_pitch(float*hz) __attribute__((weak));
/* Tuner text "A4 +3c" for the detected input pitch; 0 when there is none. */
static int tuner_read(char*out,unsigned n,double*cents)
{
    static const char*const note[12]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
    float hz=0;
    *cents=0;
    if(!input_fx_pitch||!input_fx_pitch(&hz)||hz<20.f){snprintf(out,n,"--");return 0;}
    double m=69+12*log2((double)hz/440.0);
    long r=lround(m);
    double c=(m-(double)r)*100;
    int index=(int)(((r%12)+12)%12),octave=(int)(r>=0?r/12:-1)-1;
    snprintf(out,n,"%s%d %+.0fc",note[index],octave,c);
    *cents=c;return 1;
}
static void route_row(const struct mixer_view_state*s,unsigned row,char*lab,unsigned labn,char*val,unsigned valn)
{
    (void)s;
    static const char*const via[]={"Input","Track","Master"};
    static const char*const modes[]={"Stereo","Rear L","Rear R","L+R Split","L+R Mix"};
    unsigned point,track,wet;rec_values(&point,&track,&wet);
    switch(row){
    case 0:{double cents;snprintf(lab,labn,"Tuner >");tuner_read(val,valn,&cents);break;}
    case 1:{int m=rear_native_monitor?rear_native_monitor():0;
        snprintf(lab,labn,"Monitor");snprintf(val,valn,"%s",m==2?"On":m==1?"Auto":"Off");break;}
    case 2:snprintf(lab,labn,"Record via");snprintf(val,valn,"%s",via[point]);break;
    case 3:snprintf(lab,labn,"Record track");
        if(point)snprintf(val,valn,"Track %c",'A'+track);else snprintf(val,valn,"--");break;
    case 4:snprintf(lab,labn,"Record FX");snprintf(val,valn,"%s",wet?"Wet":"Dry");break;
    case 5:{unsigned m=input_fx_mode?input_fx_mode():1;snprintf(lab,labn,"Input");snprintf(val,valn,"%s",m==0?"Clean":m==2?"Synth":"Track");break;}
    default:{unsigned mode=rear_setting?rear_setting(0,REAR_MODE):0;if(mode>REAR_MIX)mode=0;
        snprintf(lab,labn,"Rear mode");snprintf(val,valn,"%s",modes[mode]);}
    }
}
static void input_route_rows(const struct mixer_view_state*s,const struct resample_painter*p)
{
    int rear=rear_native_source&&rear_native_source()==1;
    rect(p,85,37,231,21,PANEL);
    unsigned point,track,wet;rec_values(&point,&track,&wet);
    if(s->message&&*s->message)text(p,s->message,92,37,220,21,11,AMBER,0);
    else{
        text(p,"Input routing",92,37,152,21,15,WHITE,0);
        text(p,rear?"Rear":"Mic / USB",245,38,64,19,10,GREY,1);
    }
    unsigned rows=rear?7u:6u;
    for(unsigned i=0;i<rows;++i){
        char lab[24],val[24],shown[32];
        route_row(s,i,lab,sizeof lab,val,sizeof val);
        int y=60+(int)i*19,on=i==s->cursor,edit=on&&s->input_edit,locked=i==3&&point==0;
        rect(p,85,y,231,18,on?SELECTED:PANEL);
        if(on)rect(p,85,y,2,18,AMBER);
        if(edit)rect(p,85,y+17,231,1,AMBER);
        text(p,lab,92,y,100,18,13,locked?GREY:on?AMBER:WHITE,0);
        if(edit)snprintf(shown,sizeof shown,"< %s >",val);else snprintf(shown,sizeof shown,"%s",val);
        text(p,shown,196,y,114,18,13,locked?GREY:on?AMBER:WHITE,1);
    }
}
void mixer_view_context(const struct mixer_view_state*s,const struct resample_painter*p)
{
    if(s->page==MIXER_INSPECT&&s->route_tab){route_connections(s,p);return;}
    rect(p,80,36,240,162,BLACK);
    if(s->page==MIXER_ASSIGN||s->page==MIXER_INSPECT){
        unsigned target=s->master?0:s->primary+1;
        int inspect=s->page==MIXER_INSPECT;
        const struct mixer_channel*ch=mixer_view_selected(s);
        uint32_t hue=0xff000000u|ch->colour;
        rect(p,85,37,231,21,PANEL);
        text(p,inspect?"Pad Routing":"Assign Pads",92,37,152,21,15,WHITE,0);
        text(p,inspect?"VIEW ONLY":"PAD FX",245,38,64,19,10,GREY,1);
        for(unsigned bank=0;bank<8;++bank){
            unsigned count=0;
            for(unsigned pad=0;pad<16;++pad){
                unsigned sound=bank*16+pad;
                if((s->present[sound/64]&(UINT64_C(1)<<(sound%64)))&&s->project->output[sound]==target)++count;
            }
            int x=85+(int)bank*29;
            char letter[2]={(char)('A'+bank),0};
            rect(p,x,62,27,17,bank==s->bank?SELECTED:PANEL);
            if(bank==s->bank)rect(p,x,62,27,1,AMBER);
            text(p,letter,x,63,27,15,11,bank==s->bank?AMBER:count?WHITE:GREY,1);
            if(count)rect(p,x+20,74,3,2,hue);
        }
        for(unsigned pad=0;pad<16;++pad){
            unsigned sound=s->bank*16+pad;
            int loaded=(s->present[sound/64]&(UINT64_C(1)<<(sound%64)))!=0;
            int routed=loaded&&s->project->output[sound]==target;
            int x=85+(int)(pad%4)*58,y=84+(3-(int)(pad/4))*22;
            rect(p,x,y,55,20,routed?SELECTED:loaded?PANEL:WELL);
            if(routed)rect(p,x,y,2,20,hue);
            if(sound==s->sound)border(p,x,y,55,20,WHITE);
            char number[4];snprintf(number,sizeof(number),"%u",pad+1);
            text(p,number,x+3,y,49,20,12,routed?WHITE:GREY,1);
        }
        rect(p,85,175,3,8,hue);
        text(p,"Routed here",92,173,92,13,10,GREY,0);
        text(p,inspect?"+/− or K3: bank":"PAD BANK: bank",192,173,122,13,10,GREY,1);
        text(p,s->message&&*s->message?s->message:inspect?"PAD BANK: track  /  Turn DATA: pad":"+ / −: track   /   Press pad: assign",
             86,187,230,11,10,s->message&&*s->message?AMBER:GREY,0);
        rect(p,0,198,320,42,BLACK);
        if(!inspect)footer(s,p);
    }else if(s->fx_editing){
        track_fx(s,p);
    }else{
        text(p,"Track FX",90,70,220,22,16,WHITE,0);
        text(p,"Coming later",90,100,220,20,13,GREY,0);
        rect(p,0,198,320,42,BLACK);
    }
}
void mixer_view_meters(const struct mixer_view_state*s,unsigned mask,const struct resample_painter*p)
{
    if(!s||!s->project||!p||!p->rect||!p->text)return;

    if(s->page==MIXER_INSPECT&&s->route_tab)return;
    if(s->page==MIXER_OVERVIEW){
        for(unsigned i=0;i<8;++i)if(mask&(1u<<i)){
            int x=(int)(i%4)*80,y=i<4?121:44;
            vmeter(s,i,x+34,y+18,12,77-24,p);
            send_meter(s,i,x+22,y+20,77-26,p);
        }
    }else{
        unsigned track=s->master?8:s->primary;
        if((mask&(1u<<track))||(s->master==2&&!s->fx_parameters&&!s->fx_catalog)){
            if(s->page==MIXER_KNOB_FX&&s->fx_parameters){
                effect_send_meter(s,p);
            }else{
                if(s->master!=2)vmeter(s,track==8?9:track,46,106,14,86,p);
                else input_meter(46,106,14,86,p);
                if(track<MIXER_TRACKS)send_meter(s,track,32,106,86,p);
            }
        }
    }
}
static void fx_delete_confirm(const struct mixer_view_state*s,const struct resample_painter*p)
{
    const struct mixer_channel*ch=mixer_view_selected(s);
    rect(p,0,0,320,36,PANEL);
    rect(p,0,36,320,204,BLACK);
    mixer_key(p,"Cancel",2,1,102,18,14,WHITE,0);
    mixer_key(p,"Delete",215,1,103,18,14,SEND,1);
    int point=s->fx_delete==MIXER_DELETE_POINT;
    text(p,point?"Graphic EQ":"FX Chain",108,1,104,17,12,GREY,1);
    rect(p,0,35,320,1,LINE);
    rect(p,36,68,248,104,PANEL);
    rect(p,36,68,248,2,SEND);
    text(p,point?"Remove EQ point?":"Remove effect?",44,83,232,29,20,WHITE,1);
    if(point&&s->fx_slot<ch->fx.count){
        const struct fx_eq_config *eq=&ch->fx.stage[s->fx_slot].eq;
        if(s->parameter_page<eq->count){
            char detail[40];
            snprintf(detail,sizeof(detail),"Point %u  /  %.0f Hz",
                s->parameter_page+1,eq->band[s->parameter_page].hz);
            text(p,detail,44,122,232,24,16,GREY,1);
        }
    }else if(s->fx_slot<ch->fx.count)
        text(p,fx_catalog_stage_name(ch->fx.stage+s->fx_slot),44,122,232,24,16,GREY,1);
}


void mixer_view_tuner_ex(const struct resample_painter*p,const char*tag,int have,float hz,const char*idle,const char*hint)
{
    static const char*const names[12]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
    static long last_note=-1000;
    static double shown;
    int ok=have&&hz>=20.f;
    long note=0;
    double cents=0;
    char line[48];
    const int cx=160;
    const uint32_t TUNE_CYAN=0xff00c8ffu,TUNE_GREEN=0xff1fd8a0u;
    if(ok){
        double m=69+12*log2((double)hz/440.0);
        note=lround(m);
        cents=(m-(double)note)*100;
        if(note!=last_note){last_note=note;shown=cents;}
        else shown+=(cents-shown)*.45;
    }else last_note=-1000;
    rect(p,0,0,320,240,BLACK);
    rect(p,2,2,102,26,0xff242424u);
    text(p,"Back",2,2,102,25,13,WHITE,1);
    text(p,"Tuner",108,2,102,25,13,AMBER,1);
    int index=ok?(int)(((note%12)+12)%12):-1;
    for(int i=0;i<12;++i){
        int x=10+i*25;
        if(i==index){
            text(p,names[i],x-6,36,37,40,30,TUNE_CYAN,1);
            rect(p,x+2,78,21,2,TUNE_CYAN);
        }else text(p,names[i],x-2,46,29,26,16,GREY,1);
    }
    /* Straight scale: 2.8 px per cent, 0 in the middle. */
    rect(p,20,142,281,1,GREY);
    for(int c=-50;c<=50;c+=5){
        int x=cx+(int)lround(c*2.8),h=c==0?44:c%10==0?18:9;
        rect(p,x,142-h,1,h,c==0?TUNE_CYAN:c%50==0?WHITE:GREY);
    }
    text(p,"-50 c",8,150,60,16,11,GREY,0);
    text(p,"0",cx-20,150,40,16,11,GREY,1);
    text(p,"+50 c",252,150,60,16,11,GREY,0);
    if(ok){
        double v=shown<-50?-50:shown>50?50:shown;
        int x=cx+(int)lround(v*2.8);
        uint32_t ink=fabs(cents)<=5.0?TUNE_GREEN:AMBER;
        rect(p,x-1,92,3,56,ink);
        snprintf(line,sizeof line,"%.1f Hz",(double)hz);
        text(p,line,12,176,130,28,20,WHITE,0);
        snprintf(line,sizeof line,"%s%ld",names[index],(note>=0?note/12:-1)-1);
        text(p,line,150,170,70,36,28,TUNE_CYAN,1);
        snprintf(line,sizeof line,"%+.0f c",cents);
        text(p,line,228,176,80,28,20,ink,0);
    }else{
        text(p,idle,12,178,296,26,17,GREY,1);
    }
    if(hint&&*hint){rect(p,214,2,105,26,0xff242424u);text(p,hint,214,2,105,25,13,WHITE,1);}
    text(p,tag,180,214,132,16,12,GREY,1);
}
void mixer_view_tuner(const struct resample_painter*p)
{
    float hz=0;
    int have=input_fx_pitch&&input_fx_pitch(&hz);
    mixer_view_tuner_ex(p,"Input",have,hz,"Play or sing a note","");
}
extern unsigned am_synth_param_get(unsigned) __attribute__((weak));
struct synth_live{float peak,env_level,bend,mod;unsigned env_stage,voices;unsigned notes[4];};
extern int am_synth_peek(struct synth_live*) __attribute__((weak));
extern uint64_t am_synth_pad_info(unsigned,unsigned*) __attribute__((weak));
extern unsigned music_light_colour_count(void) __attribute__((weak));
extern unsigned music_light_colour_choice(unsigned) __attribute__((weak));
extern unsigned music_light_colour_index(unsigned) __attribute__((weak));
extern const char *music_light_colour_name(unsigned) __attribute__((weak));
/* UI slot id -> engine parameter (0-9 direct, 17-27 = 11-21), -1 for the 16 Levels key settings */
static int synth_pidx(int id){return id>=0&&id<10?id:id>=17&&id<=27?id-6:-1;}
static void synth_text(const struct mixer_view_state*s,int id,char*name,size_t nn,char*val,size_t vn)
{
    static const char*const names[28]={"Wave","Cutoff","Attack","Decay","Sustain","Release","Level","MIDI channel","Mic","Fader","Layout","Key","Scale","Octave","Transpose","Colour","Role","Filter","Resonance","Env > Cutoff","LFO Rate","LFO Depth","LFO Target","Unison","Detune","Spread","Glide","Velocity"};
    static const char*const rolen[4][5]={{"Original","","","",""},{"Original","","","",""},{"Tonic","Scale","Other","Original",""},{"I","Key notes","IV","V","Original"}};
    static const char*const waves[4]={"Sine","Saw","Square","Triangle"};
    static const char*const lay[4]={"Monochrome","Piano","Scale","Degrees"};
    static const char*const root[13]={"Project","C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
    static const char*const scl[9]={"Project","Major","Minor","Dorian","Phrygian","Lydian","Mixolydian","Locrian","Chromatic"};
    if(id<0||id>27){name[0]=val[0]=0;return;}
    snprintf(name,nn,"%s",names[id]);
    unsigned v=am_synth_param_get&&synth_pidx(id)>=0?am_synth_param_get((unsigned)synth_pidx(id)):0;
    switch(id){
        case 0:snprintf(val,vn,"%s",waves[v%4]);break;
        case 7:snprintf(val,vn,"%u",v+1);break;
        case 8:snprintf(val,vn,"%s",v?"On":"Off");break;
        case 9:snprintf(val,vn,"%s",v==1?"Mod":v==2?"Pitch":"Off");break;
        case 10:snprintf(val,vn,"%s",lay[s->synth_layout%4]);break;
        case 11:snprintf(val,vn,"%s",root[s->synth_root%13]);break;
        case 12:snprintf(val,vn,"%s",scl[s->synth_scale%9]);break;
        case 13:snprintf(val,vn,"%u",s->synth_octave);break;
        case 14:snprintf(val,vn,s->synth_layout==3?"%+d steps":"%+d st",(int)s->synth_transpose-24);break;
        case 15:snprintf(val,vn,"%s",music_light_colour_name?music_light_colour_name(s->synth_cols[mixer_view_synth_field(s->synth_layout,s->synth_role)]):"-");break;
        case 16:snprintf(val,vn,"%s",rolen[s->synth_layout%4][s->synth_role%5]);break;
        case 17:snprintf(val,vn,"%s",v%3==0?"Low pass":v%3==1?"High pass":"Band pass");break;
        case 20:snprintf(val,vn,"%.1f Hz",0.1*pow(200.0,v/127.0));break;
        case 22:snprintf(val,vn,"%s",v%3==0?"Pitch":v%3==1?"Filter":"Amp");break;
        case 23:snprintf(val,vn,"%u voice%s",v%4+1,v%4?"s":"");break;
        case 24:snprintf(val,vn,"%u ct",v*50/127);break;
        case 26:if(v)snprintf(val,vn,"%.0f ms",5.0*pow(200.0,v/127.0));else snprintf(val,vn,"Off");break;
        case 18:case 19:case 21:case 25:case 27:snprintf(val,vn,"%u%%",v*100/127);break;
        default:snprintf(val,vn,"%u",v);
    }
}
static float synth_wave_at(unsigned w,float ph)
{
    return w==0?sinf(6.2831853f*ph):w==1?2.0f*ph-1.0f:w==2?(ph<0.5f?1.0f:-1.0f):(ph<0.5f?4.0f*ph-1.0f:3.0f-4.0f*ph);
}
static float synth_frac(const struct mixer_view_state*s,int id)
{
    unsigned v=am_synth_param_get&&synth_pidx(id)>=0?am_synth_param_get((unsigned)synth_pidx(id)):0;
    switch(id){
        case 17:case 22:return (float)(v%3)/2.0f;
        case 23:return (float)(v%4)/3.0f;
        case 0:return (float)(v%4)/3.0f;
        case 7:return (float)v/15.0f;
        case 8:return v?1.0f:0.0f;
        case 9:return (float)(v%3)/2.0f;
        case 10:return (float)(s->synth_layout%4)/3.0f;
        case 11:return (float)(s->synth_root%13)/12.0f;
        case 12:return (float)(s->synth_scale%9)/8.0f;
        case 13:return (float)(s->synth_octave%8)/7.0f;
        case 14:return (float)(s->synth_transpose%49)/48.0f;
        case 15:{unsigned n=music_light_colour_count?music_light_colour_count():2;return n>1&&music_light_colour_index?(float)music_light_colour_index(s->synth_cols[mixer_view_synth_field(s->synth_layout,s->synth_role)])/(float)(n-1):0.0f;}
        case 16:return mixer_view_synth_roles(s->synth_layout)>1?(float)(s->synth_role%5)/(float)(mixer_view_synth_roles(s->synth_layout)-1):0.0f;
        default:return id<0?0.0f:(float)v/127.0f;
    }
}
static void synth_adsr_mini(const struct resample_painter*p,int x0,int y0,int w,int h,int selected)
{
    unsigned a=am_synth_param_get?am_synth_param_get(2):0,d=am_synth_param_get?am_synth_param_get(3):0,su=am_synth_param_get?am_synth_param_get(4):0,r=am_synth_param_get?am_synth_param_get(5):0;
    float wa=4.0f+(float)a/127.0f*14.0f,wd=4.0f+(float)d/127.0f*14.0f,ws=14.0f,wr=4.0f+(float)r/127.0f*14.0f,tot=wa+wd+ws+wr;
    float sus=(float)su/127.0f;
    for(int i=0;i<w;++i){
        float t=(float)i/(float)w*tot,y;
        if(t<wa)y=t/wa;else if(t<wa+wd)y=1.0f-(1.0f-sus)*(t-wa)/wd;else if(t<wa+wd+ws)y=sus;else y=sus*(1.0f-(t-wa-wd-ws)/wr);
        int hh=(int)(y*(float)h);if(hh<1)hh=1;
        rect(p,x0+i,y0+h-hh,1,hh,selected?AMBER:0xff806018u);
    }
}
void mixer_view_synth(const struct mixer_view_state*s,const struct resample_painter*p)
{
    unsigned page=s->synth_cursor%5;
    rect(p,0,0,320,240,BLACK);
    rect(p,2,2,102,26,0xff242424u);text(p,"Back",2,2,102,25,13,WHITE,1);
    rect(p,108,2,102,26,page==2?0xff4a3a10u:0xff242424u);text(p,"16 Levels",108,2,102,25,13,s->synth_keys?AMBER:WHITE,1);
    {char hold[16]="Hold note";
    if(s->synth_hold){static const char*const nn[12]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
        unsigned n=s->synth_hold_note;snprintf(hold,sizeof(hold),"Hold %s%d",nn[n%12],(int)(n/12)-1);}
    rect(p,214,2,104,26,s->synth_hold?0xff4a3a10u:0xff242424u);text(p,hold,214,2,104,25,13,s->synth_hold?AMBER:WHITE,1);}
    struct synth_live live;int playing=am_synth_peek&&am_synth_peek(&live);if(!playing)memset(&live,0,sizeof live);
    /* top left: oscilloscope (Keys page: the 16 Levels pad grid) */
    rect(p,4,32,152,96,0xff141414u);
    if(page!=2){
        unsigned w=am_synth_param_get?am_synth_param_get(0):0,cut=am_synth_param_get?am_synth_param_get(1):100;
        float amp=0.15f+0.85f*(live.peak*5.0f>1.0f?1.0f:live.peak*5.0f);
        float phase=(float)(s->now_ms%1000u)/1000.0f*(1.0f+live.bend*0.5f);
        float wob=live.mod*0.05f*sinf((float)(s->now_ms%1000u)/1000.0f*6.2831853f*5.5f);
        rect(p,6,80,148,1,0xff303030u);
        for(unsigned i=0;i<36;++i){
            float ph=(float)i/18.0f+phase+wob;ph-=(float)(int)ph;if(ph<0.0f)ph+=1.0f;
            float x=synth_wave_at(w,ph);
            int h=(int)(x*42.0f*amp*(0.35f+0.65f*(float)cut/127.0f));
            uint32_t col=playing?AMBER:0xff806018u;
            if(h>=0)rect(p,6+(int)i*4,80-h,3,h+1,col);else rect(p,6+(int)i*4,80,3,-h+1,col);
        }
        char info[24];snprintf(info,sizeof info,"Voices %u",live.voices);text(p,info,8,33,70,11,10,GREY,0);
    }else{
        static const char*const nm[12]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
        for(unsigned row=0;row<4;++row)for(unsigned col=0;col<4;++col){
            unsigned pad=(3-row)*4+col;char n[16];
            unsigned note=pad;uint64_t lt=am_synth_pad_info?am_synth_pad_info(pad,&note):0;
            snprintf(n,sizeof n,"%s%d",nm[note%12],(int)(note/12)-1);
            int cx=6+(int)col*37,cy=35+(int)row*23;
            int on=playing&&((live.notes[note>>5]>>(note&31))&1u);
            float br;{uint32_t bb=(uint32_t)(lt>>32);memcpy(&br,&bb,sizeof br);}
            br=br*1.4f+0.05f;if(br>1.0f)br=1.0f;
            uint32_t rc=(uint32_t)lt,lr=(unsigned)((rc>>16)&255u)*br,lg=(unsigned)((rc>>8)&255u)*br,lb=(unsigned)(rc&255u)*br;
            rect(p,cx,cy,35,21,on?AMBER:(0xff000000u|lr<<16|lg<<8|lb));text(p,n,cx,cy,35,20,11,on?BLACK:WHITE,1);
        }
    }
    /* top right: module scheme (pages): Tone, Envelope, Filter + LFO, Voices, Keys */
    static const char*const mods[5]={"Tone","Envelope","Filter + LFO","Voices","Keys"};
    static const unsigned char order[5]={0,1,3,4,2};
    for(unsigned m=0;m<5;++m){
        unsigned pg=order[m];int y=32+(int)m*19,sel=pg==page;
        char sum[64],a1[24],b1[24],c1[24],d1[24],e1[24],f1[24];sum[0]=0;
        if(pg==0){synth_text(s,0,a1,sizeof a1,b1,sizeof b1);snprintf(sum,sizeof sum,"%s  Cut %u",b1,am_synth_param_get?am_synth_param_get(1):0);}
        else if(pg==3){synth_text(s,17,a1,sizeof a1,b1,sizeof b1);synth_text(s,22,c1,sizeof c1,d1,sizeof d1);synth_text(s,21,e1,sizeof e1,f1,sizeof f1);
            snprintf(sum,sizeof sum,"%.4s  %s %s",b1,d1,f1);}
        else if(pg==4){synth_text(s,23,a1,sizeof a1,b1,sizeof b1);synth_text(s,24,c1,sizeof c1,d1,sizeof d1);snprintf(sum,sizeof sum,"%s  %s",b1,d1);}
        else if(pg==2){char sl[24],sv[24],l[24],lv[24],r[24],rv[24];synth_text(s,10,l,sizeof l,lv,sizeof lv);synth_text(s,11,r,sizeof r,rv,sizeof rv);synth_text(s,12,sl,sizeof sl,sv,sizeof sv);snprintf(sum,sizeof sum,"%.5s %s %.5s",lv,rv,sv);}
        rect(p,164,y,152,17,sel?0xff4a3a10u:0xff1c1c1cu);
        if(sel)rect(p,164,y,3,17,AMBER);
        text(p,mods[m],170,y+1,74,15,11,sel?AMBER:WHITE,0);
        if(pg==1)synth_adsr_mini(p,246,y+3,64,11,sel);
        else text(p,sum,246,y+2,68,13,9,GREY,0);
    }
    text(p,s->synth_pick?"Pads 1-5: Tone/Env/Keys/Filter/Voices":"Loop: pages   B2: 16 Levels setup   DATA: page",8,131,304,11,10,s->synth_pick?AMBER:GREY,0);
    /* bottom: knobs of the selected page, direct + Shift */
    for(unsigned k=0;k<3;++k){
        int x=(int)k*106,w=k==2?105:102;
        int idd=mixer_view_synth_slot(page,k,0),ids=mixer_view_synth_slot(page,k,1);
        int idm=s->synth_shift?ids:idd,ido=s->synth_shift?idd:ids;
        char nn[24],vv[16],on[24],ov[16];
        synth_text(s,idm,nn,sizeof nn,vv,sizeof vv);synth_text(s,ido,on,sizeof on,ov,sizeof ov);
        rect(p,x+2,144,w,94,0xff202020u);
        text(p,nn[0]?nn:"-",x+4,146,w-4,12,11,GREY,1);
        text(p,vv,x+4,160,w-4,20,17,AMBER,1);
        rect(p,x+8,186,w-12,6,0xff383838u);
        int fw=(int)(synth_frac(s,idm)*(float)(w-12));if(fw>0)rect(p,x+8,186,fw,6,AMBER);
        char line[96];if(ido>=0)snprintf(line,sizeof line,"%s: %s %s",s->synth_shift?"Direct":"Shift",on,ov);else snprintf(line,sizeof line,"%s: -",s->synth_shift?"Direct":"Shift");
        text(p,line,x+4,200,w-4,12,10,GREY,1);
        rect(p,x+8,218,w-12,1,0xff303030u);
    }
}
void mixer_view_paint(const struct mixer_view_state*s,const struct resample_painter*p)
{
    if(!s||!s->project||!p||!p->rect||!p->text||s->primary>=8||s->bank>=8||s->page>MIXER_INSPECT)return;
    if(s->synth_page){mixer_view_synth(s,p);return;}
    if(s->master==2&&s->page==MIXER_KNOB_FX&&s->input_tuner){mixer_view_tuner(p);return;}
    if(s->page==MIXER_KNOB_FX&&s->fx_delete){
        fx_delete_confirm(s,p);
        return;
    }
    header(s,p);
    if(s->name_edit){
        rect(p,0,36,320,204,BLACK);
        rect(p,12,48,296,125,PANEL);
        rect(p,12,48,296,2,0xff000000u|(s->master?s->project->master.colour:s->project->track[s->primary].colour));
        text(p,s->name_title?s->name_title:"Track name",22,57,276,25,19,WHITE,0);
        rect(p,22,91,276,47,WELL);
        text(p,s->name_edit,30,94,260,39,19,AMBER,0);
        text(p,s->name_pads?"Pads type   DATA moves":"DATA edits   Press to advance",22,145,276,19,13,GREY,0);
        if(s->message&&*s->message)text(p,s->message,12,176,296,17,11,AMBER,0);
        knob_cell(p,0,"Input",s->name_pads?"Pads":"Encoder",0,0);
        return;
    }
    if(s->page==MIXER_KNOB_FX&&s->fx_parameters){
        const struct mixer_channel*ch=mixer_view_selected(s);
        if(s->master&&s->fx_slot==MIXER_COMP_SLOT&&s->comp.visible){comp_editor(s,p);return;}
        if(s->fx_slot<ch->fx.count){effect_editor(s,p,ch->fx.stage+s->fx_slot);return;}
    }
    if(s->page==MIXER_ASSIGN||s->page==MIXER_INSPECT||s->page==MIXER_KNOB_FX){
        if(s->page!=MIXER_INSPECT||!s->route_tab)mixer_view_channel_strip(s,s->master?8:s->primary,0,80,p);
        mixer_view_context(s,p);}
    else {mixer_view_overview(s,p);footer(s,p);}
}
