#include "mixer_view.h"
#include "fx_catalog.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#define BLACK 0xff000000u
#define WHITE 0xffffffffu
#define GREY 0xff777777u
#define DARK 0xff252525u
#define AMBER 0xffffc247u
static void rect(const struct resample_painter*p,int x,int y,int w,int h,uint32_t c)
{
    if(w>0&&h>0)p->rect(p->user,x,y,w,h,c);
}
static void text(const struct resample_painter*p,const char*s,int x,int y,int w,int h,unsigned size,uint32_t c,int centre)
{
    p->text(p->user,s,x,y,w,h,size,c,centre);
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
    rect(p,x,y,w,h,BLACK);
    if(!available(s,bus)){text(p,"N/A",x-6,y+h/2-6,w+12,12,8,GREY,1);return;}
    int bar=(w-1)/2;
    for(unsigned c=0;c<2;++c){float db=mixer_meter_dbfs(s->meter->peak[bus][c]);
        int fill=isfinite(db)?(int)lroundf((fmaxf(-60,fminf(0,db))+60)*h/60):0;
        rect(p,x+(int)c*(bar+1),y,bar,h,DARK);
        if(fill>0)rect(p,x+(int)c*(bar+1),y+h-fill,bar,fill,(s->meter->clip_mask&(1u<<bus))?0xffff4747u:AMBER);}
}
static void pan_bar(float position,int x,int y,int w,const struct resample_painter*p)
{
    int centre=x+w/2;if(!(position>=-1))position=-1;if(position>1)position=1;
    int at=centre+(int)lroundf(position*(float)(w/2-1));
    rect(p,x,y,w,8,BLACK);rect(p,x,y+3,w,2,DARK);rect(p,centre,y,1,8,GREY);
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
static void fader_rail(float db,int selected,int x,int y,int h,const struct resample_painter*p)
{
    rect(p,x-6,y,14,h,BLACK);rect(p,x,y,2,h,GREY);rect(p,x-4,y+h/6,10,1,WHITE);
    int t=rail_y(db,y,h);if(t<y+2)t=y+2;if(t>y+h-3)t=y+h-3;
    rect(p,x-5,t-2,12,5,selected?AMBER:WHITE);
}
/* Grid cell of one Track: colour strip, Pan (top left), Fader (left),
 * meter (centre, full height), letter and S/M/fx (right). */
static void cell(const struct mixer_view_state*s,unsigned track,int x,int y,const struct resample_painter*p)
{
    const int w=80,h=81;
    const struct mixer_channel*c=s->project->track+track;
    struct mixer_mix resolved[8];mixer_scene_resolve(s->project,s->part_id,resolved);const struct mixer_mix*m=resolved+track;
    int selected=!s->master&&(s->selected&(1u<<track));
    rect(p,x,y,w,h,BLACK);rect(p,x+w-1,y,1,h,DARK);rect(p,x,y+h-1,w,1,DARK);
    rect(p,x+2,y+1,w-4,3,0xff000000u|(c->colour&0xffffff));
    if(selected)border(p,x+1,y+5,w-3,h-7,s->primary==track?AMBER:WHITE);
    pan_bar(m->pan,x+5,y+8,34,p);
    fader_rail(m->level_db,selected,x+14,y+20,h-26,p);
    vmeter(s,track,x+34,y+18,12,h-24,p);
    char letter[2]={(char)('A'+track),0};
    text(p,letter,x+50,y+6,26,22,20,s->primary==track&&!s->master?AMBER:WHITE,1);
    text(p,"S",x+52,y+31,22,14,11,m->solo?AMBER:GREY,1);
    text(p,"M",x+52,y+47,22,14,11,m->mute?AMBER:GREY,1);
    text(p,"fx",x+52,y+63,22,14,10,GREY,1);
}
void mixer_view_channel_strip(const struct mixer_view_state*s,unsigned track,int x,int width,const struct resample_painter*p)
{
    /* Left area on PAD FX / KNOB FX: big name (A-H or MSTR), Fader, meter. */
    (void)width;
    const struct mixer_channel*c=track<8?s->project->track+track:&s->project->master;
    struct mixer_mix resolved[8];const struct mixer_mix*m=track<8?(mixer_scene_resolve(s->project,s->part_id,resolved),resolved+track):&s->project->master.mix;
    rect(p,x,36,80,162,BLACK);rect(p,x+79,36,1,162,GREY);
    rect(p,x+2,37,76,4,0xff000000u|(c->colour&0xffffff));
    char name[8];if(track<8)snprintf(name,sizeof(name),"%c",'A'+track);else snprintf(name,sizeof(name),"MSTR");
    text(p,name,x+2,43,76,32,track<8?30:18,AMBER,1);
    char db[24];level_text(db,sizeof(db),m->level_db);text(p,db,x+2,76,76,14,11,WHITE,1);
    pan_bar(m->pan,x+8,93,64,p);
    fader_rail(m->level_db,1,x+22,106,86,p);
    vmeter(s,track<8?track:9,x+46,106,14,86,p);
    if(track<8){text(p,"S",x+62,114,16,14,11,m->solo?AMBER:GREY,1);text(p,"M",x+62,134,16,14,11,m->mute?AMBER:GREY,1);}
}
void mixer_view_overview(const struct mixer_view_state*s,const struct resample_painter*p)
{
    /* Bottom row A-D, top row E-H. */
    for(unsigned i=0;i<8;++i)cell(s,i,(int)(i%4)*80,i<4?117:36,p);
}
static void header(const struct mixer_view_state*s,const struct resample_painter*p)
{
    rect(p,0,0,320,36,BLACK);
    /* B1 B2 B3 functions on top (no "B1" text in the names). */
    int sub=s->page==MIXER_ASSIGN||s->page==MIXER_INSPECT||s->page==MIXER_KNOB_FX;
    int muted=0,soloed=0;struct mixer_mix all[8];mixer_scene_resolve(s->project,s->part_id,all);
    if(!s->master)for(unsigned i=0;i<8;++i)if(s->selected&(1u<<i)){muted|=all[i].mute;soloed|=all[i].solo;}
    const char*names[3]={sub?"Back":"Sample","Mute","Solo"};
    int lit[3]={0,muted,soloed},off=s->master||s->page==MIXER_INSPECT||s->page==MIXER_KNOB_FX;
    for(unsigned i=0;i<3;++i){int x=i==2?213:(int)i*106,w=i==0?106:107;
        uint32_t ink=i&&off?GREY:lit[i]?AMBER:WHITE;
        text(p,names[i],x,1,w,17,12,ink,1);rect(p,x+6,18,w-12,1,lit[i]?AMBER:DARK);}
    unsigned group=0;for(unsigned i=0;i<8;++i)if(s->selected&(1u<<i))++group;
    char title[64],db[24];
    const struct mixer_mix*m=s->master?&s->project->master.mix:all+s->primary;
    level_text(db,sizeof(db),m->level_db);
    if(s->master)snprintf(title,sizeof(title),"MSTR  %s",db);
    else if(group>1)snprintf(title,sizeof(title),"%c +%u  %s",'A'+s->primary,group-1,db);
    else snprintf(title,sizeof(title),"%c  %s",'A'+s->primary,db);
    text(p,title,4,20,150,15,11,s->pending?GREY:WHITE,0);
    /* Master: name and a horizontal stereo meter. */
    text(p,"MSTR",160,20,40,15,11,s->master?0xff00c700u:WHITE,1);
    rect(p,204,22,112,11,BLACK);
    if(available(s,9))for(unsigned c=0;c<2;++c){float v=mixer_meter_dbfs(s->meter->peak[9][c]);
        int fill=isfinite(v)?(int)lroundf((fmaxf(-60,fminf(0,v))+60)*112/60):0;
        rect(p,204,22+(int)c*6,112,5,DARK);if(fill>0)rect(p,204,22+(int)c*6,fill,5,0xff00c700u);}
    else text(p,"N/A",204,21,112,13,8,GREY,1);
}
static void footer(const struct mixer_view_state*s,const struct resample_painter*p)
{
    rect(p,0,198,320,42,BLACK);
    const struct mixer_mix*m;struct mixer_mix resolved[8];
    if(s->master)m=&s->project->master.mix;else{mixer_scene_resolve(s->project,s->part_id,resolved);m=resolved+s->primary;}
    char value[32];pan(value,sizeof(value),m->pan);
    text(p,"Pan",4,200,98,12,11,s->cursor==1?AMBER:GREY,1);text(p,value,4,215,98,18,14,s->cursor==1?AMBER:WHITE,1);
    text(p,"Level",110,200,98,12,11,s->cursor==0?AMBER:GREY,1);
    level_text(value,sizeof(value),m->level_db);text(p,value,110,215,98,18,13,s->cursor==0?AMBER:WHITE,1);
    text(p,"Colour",217,200,99,12,11,s->cursor==2?AMBER:GREY,1);
    const char*name=s->master?"Green":mixer_colour_name(s->project->track[s->primary].colour);
    text(p,name?name:"Custom",217,215,99,18,13,s->cursor==2?AMBER:WHITE,1);
    if(s->message&&*s->message){rect(p,0,186,320,12,BLACK);text(p,s->message,4,186,312,12,9,AMBER,1);}
}
void mixer_view_context(const struct mixer_view_state*s,const struct resample_painter*p)
{
    rect(p,80,36,240,162,BLACK);
    if(s->page==MIXER_ASSIGN||s->page==MIXER_INSPECT){
        /* "Select pads for track X": every bank with the pads on this
         * Track (Master: pads not on any Track), as compact ranges. */
        unsigned target=s->master?0:s->primary+1;
        char title[48];
        if(s->master)snprintf(title,sizeof(title),"Select pads for MSTR");
        else snprintf(title,sizeof(title),"Select pads for track %c",'A'+s->primary);
        text(p,s->page==MIXER_INSPECT?"Pads on this track":title,86,38,230,18,13,AMBER,0);
        for(unsigned bank=0;bank<8;++bank){
            char line[96];int n=snprintf(line,sizeof(line),"%c:",'A'+bank),any=0;
            for(unsigned pad=0;pad<16;){
                if(s->project->output[bank*16+pad]!=target){++pad;continue;}
                unsigned end=pad;while(end+1<16&&s->project->output[bank*16+end+1]==target)++end;
                if(n<(int)sizeof(line)-12){
                    if(end>pad+1)n+=snprintf(line+n,sizeof(line)-(size_t)n,"%s %u-%u",any?",":"",pad+1,end+1);
                    else if(end==pad+1)n+=snprintf(line+n,sizeof(line)-(size_t)n,"%s %u, %u",any?",":"",pad+1,end+1);
                    else n+=snprintf(line+n,sizeof(line)-(size_t)n,"%s %u",any?",":"",pad+1);
                }
                any=1;pad=end+1;
            }
            if(!any)snprintf(line+n,sizeof(line)-(size_t)n," -");
            int y=58+(int)bank*16;
            if(bank==s->bank)rect(p,84,y-1,232,16,DARK);
            text(p,line,90,y,226,15,11,bank==s->bank?WHITE:GREY,0);
        }
        if(s->message&&*s->message)text(p,s->message,86,186,230,12,9,AMBER,0);
        rect(p,0,198,320,42,BLACK);
        if(s->page==MIXER_INSPECT)text(p,"Read only",4,210,312,16,12,WHITE,1);
        else{text(p,"Pads: to this track / back to MSTR   Bank +/-",4,201,312,15,10,AMBER,1);
            text(p,"Fader Level   K1 Pan   K3 Colour",4,220,312,14,10,WHITE,1);}
    }else{
        text(p,"Track FX",90,70,220,22,16,WHITE,0);
        text(p,"Coming later",90,100,220,20,13,GREY,0);
        rect(p,0,198,320,42,BLACK);
    }
}
void mixer_view_meters(const struct mixer_view_state*s,unsigned mask,const struct resample_painter*p)
{
    if(!s||!p||!p->rect||!p->text)return;
    if(s->page==MIXER_OVERVIEW){for(unsigned i=0;i<8;++i)if(mask&(1u<<i)){int x=(int)(i%4)*80,y=i<4?117:36;vmeter(s,i,x+34,y+18,12,81-24,p);}}
    else {unsigned track=s->master?8:s->primary;if(mask&(1u<<track))vmeter(s,track==8?9:track,46,106,14,86,p);}
}
void mixer_view_paint(const struct mixer_view_state*s,const struct resample_painter*p)
{
    if(!s||!s->project||!p||!p->rect||!p->text||s->primary>=8||s->bank>=8||s->page>MIXER_INSPECT)return;
    header(s,p);
    if(s->page==MIXER_ASSIGN||s->page==MIXER_INSPECT||s->page==MIXER_KNOB_FX){
        mixer_view_channel_strip(s,s->master?8:s->primary,0,80,p);mixer_view_context(s,p);}
    else {mixer_view_overview(s,p);footer(s,p);}
}
