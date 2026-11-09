#include "mixer_view.h"
#include "fx_catalog.h"
#include "shift_badge.h"
#include "spectrum_plot.h"
#include "mixer_sample_feedback.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
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
    rect(p,x,y,w,h,BLACK);
    if(!available(s,bus)){text(p,"N/A",x-6,y+h/2-6,w+12,12,8,GREY,1);return;}
    int bar=(w-1)/2;
    for(unsigned c=0;c<2;++c){float db=mixer_meter_dbfs(s->meter->peak[bus][c]);
        int fill=isfinite(db)?(int)lroundf((fmaxf(-60,fminf(0,db))+60)*h/60):0;
        rect(p,x+(int)c*(bar+1),y,bar,h,DARK);
        if(fill>0)rect(p,x+(int)c*(bar+1),y+h-fill,bar,fill,(s->meter->clip_mask&(1u<<bus))?0xffff4747u:AMBER);}
}
static void send_meter(const struct mixer_view_state*s,unsigned track,
                       int x,int y,int h,const struct resample_painter*p)
{
    if(track>=MIXER_TRACKS)return;
    int receiving=available(s,track)&&(s->meter->return_mask&(1u<<track));
    rect(p,x,y,2,h,receiving?0xff351619u:BLACK);
    if(!receiving)return;
    float db=mixer_meter_dbfs(s->meter->return_peak[track]);
    if(!isfinite(db))return;
    int height=(int)lroundf(fmaxf(0,fminf(60,db+60))*h/60);
    if(height)rect(p,x,y+h-height,2,height,0xffff4040u);
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
static void fader_rail(float db,int selected,int x,int y,int h,int cap,const struct resample_painter*p)
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
    rect(p,left+1,t-3,cap-2,7,selected?0xffaa783bu:0xff787d84u);
    rect(p,left+1,t-3,cap-2,1,selected?0xffffcf81u:0xffcdd0d4u);
    rect(p,left+2,t-1,cap-4,1,0xff414246u);
    rect(p,left+2,t+1,cap-4,1,0xff414246u);
    rect(p,left+2,t,cap-4,1,selected?0xffffe7bf:WHITE);
    rect(p,left+1,t+3,cap-2,1,0xff38393bu);
}
static void mixer_key(const struct resample_painter*p,const char*label,int x,int y,int w,int h,
                      unsigned size,uint32_t ink,int lit)
{
    if(!*label)return;
    rect(p,x,y,w,h,0xff101113u);
    rect(p,x+1,y+1,w-2,h-2,lit?0xff332c24u:0xff22252au);
    rect(p,x+1,y+1,w-2,1,lit?ink:0xff454950u);
    rect(p,x+1,y+h-2,w-2,1,0xff131517u);
    text(p,label,x+2,y+1,w-4,h-2,size,ink,1);
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
    rect(p,x,y,w,h,BLACK);rect(p,x+w-1,y,1,h,DARK);rect(p,x,y+h-1,w,1,DARK);
    rect(p,x+2,y+1,w-4,3,0xff000000u|(c->colour&0xffffff));
    if(selected)border(p,x+1,y+5,w-3,h-7,s->primary==track?AMBER:WHITE);
    pan_bar(m->pan,x+5,y+8,34,p);
    fader_rail(m->level_db,selected,x+14,y+20,h-26,14,p);
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
    const struct mixer_channel*c=track<8?s->project->track+track:&s->project->master;
    struct mixer_mix resolved[8];const struct mixer_mix*m=track<8?(mixer_scene_resolve(s->project,s->part_id,resolved),resolved+track):&s->project->master.mix;
    rect(p,x,36,80,162,BLACK);rect(p,x+79,36,1,162,GREY);
    rect(p,x+2,37,76,4,0xff000000u|(c->colour&0xffffff));
    char name[8],stock[12];
    if(track<8)snprintf(name,sizeof(name),"%c",'A'+track);else snprintf(name,sizeof(name),"MSTR");
    snprintf(stock,sizeof(stock),"Track %c",'A'+track);
    int custom=track<8&&strcmp(c->name,stock)!=0;
    track_title(p,custom?c->name:name,x+2,43,76,32,custom?21:track<8?34:21,AMBER,1,s->name_elapsed);
    char db[24];level_text(db,sizeof(db),m->level_db);text(p,db,x+2,76,76,14,11,WHITE,1);
    pan_bar(m->pan,x+8,93,64,p);
    fader_rail(m->level_db,1,x+22,106,86,18,p);
    vmeter(s,track<8?track:9,x+46,106,14,86,p);
    if(track<8)send_meter(s,track,x+32,106,86,p);
    if(track<8){
        mixer_key(p,"S",x+62,114,16,15,10,m->solo?(0xff000000u|MIXER_SOLO_RGB):GREY,m->solo);
        mixer_key(p,"M",x+62,134,16,15,10,m->mute?(0xff000000u|MIXER_MUTE_RGB):GREY,m->mute);
    }
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
static void header(const struct mixer_view_state*s,const struct resample_painter*p)
{
    rect(p,0,0,320,s->page==MIXER_OVERVIEW?44:36,BLACK);
    /* B1 B2 B3 functions on top (no "B1" text in the names). */
    int sub=s->page==MIXER_ASSIGN||s->page==MIXER_INSPECT||s->page==MIXER_KNOB_FX;
    int muted=0,soloed=0;struct mixer_mix all[8];mixer_scene_resolve(s->project,s->part_id,all);
    if(!s->master)for(unsigned i=0;i<8;++i)if(s->selected&(1u<<i)){muted|=all[i].mute;soloed|=all[i].solo;}
    const char*names[3]={sub?"Back":"Sample","Mute","Solo"};
    int lit[3]={0,muted,soloed},off=s->master||s->page==MIXER_INSPECT||s->page==MIXER_KNOB_FX;
    if(s->page==MIXER_KNOB_FX&&s->fx_editing){
        names[1]=s->shifted?"Chain Bypass":"Copy";
        names[2]=s->shifted?"":"Paste";
        lit[1]=lit[2]=off=0;
    }
    if(s->page==MIXER_KNOB_FX&&s->fx_parameters){
        names[0]="FX List";names[1]=s->shifted?"Chain Bypass":"Bypass";names[2]="";
        const struct mixer_channel*ch=s->master?&s->project->master:s->project->track+s->primary;
        if(s->shifted&&s->fx_slot<ch->fx.count&&ch->fx.stage[s->fx_slot].kind==FX_STAGE_EQ)names[2]="+ EQ Point";
    }
    if(s->name_edit){names[0]="Cancel";names[1]="Erase";names[2]="Save";lit[1]=lit[2]=off=0;}
    else if(s->shifted&&s->page==MIXER_OVERVIEW){names[1]="Rename";names[2]="";lit[1]=lit[2]=0;}
    if(s->page==MIXER_KNOB_FX&&s->fx_delete){names[0]="Cancel";names[1]="";names[2]="Delete";}
    for(unsigned i=0;i<3;++i){int x=i==2?213:(int)i*106,w=i==0?106:107;
        uint32_t active=i==1?(0xff000000u|MIXER_MUTE_RGB):(0xff000000u|MIXER_SOLO_RGB);
        int gates=!s->name_edit&&!s->shifted&&(s->page==MIXER_OVERVIEW||s->page==MIXER_ASSIGN);
        uint32_t ink=i&&off?GREY:lit[i]?active:i&&gates?GREY:WHITE;
        mixer_key(p,names[i],x+6,1,w-12,18,12,ink,lit[i]);}
    unsigned group=0;for(unsigned i=0;i<8;++i)if(s->selected&(1u<<i))++group;
    int overview=s->page==MIXER_OVERVIEW&&!s->name_edit;
    if(overview){
        char title[MIXER_NAME_BYTES+8],db[24];
        const struct mixer_mix*m=s->master?&s->project->master.mix:all+s->primary;
        const char*name=s->master?s->project->master.name:s->project->track[s->primary].name;
        if(group>1)snprintf(title,sizeof(title),"%s +%c",name,'0'+(int)group-1);
        else snprintf(title,sizeof(title),"%s",name);
        level_text(db,sizeof(db),m->level_db);
        const unsigned heading_size=19;
        track_title(p,title,5,20,125,24,heading_size,WHITE,0,s->name_elapsed);
        text(p,db,133,20,73,24,heading_size,WHITE,1);
        rect(p,208,22,1,17,DARK);
    }
    int x=overview?245:204,w=overview?71:112;
    text(p,"MSTR",overview?211:160,20,overview?33:40,15,11,s->master?0xff00c700u:WHITE,1);
    rect(p,x,22,w,11,BLACK);
    if(available(s,9))for(unsigned c=0;c<2;++c){
        float v=mixer_meter_dbfs(s->meter->peak[9][c]);
        int fill=isfinite(v)?(int)lroundf((fmaxf(-60,fminf(0,v))+60)*w/60):0;
        rect(p,x,22+(int)c*6,w,5,DARK);
        if(fill>0)rect(p,x,22+(int)c*6,fill,5,0xff00c700u);
    }else text(p,"N/A",x,21,w,13,8,GREY,1);
}
static void knob_cell(const struct resample_painter*p,unsigned column,const char*label,
                      const char*value,int focused,unsigned alternate)
{
    int x=(int)column*106,w=column==2?108:106;
    uint32_t ink=focused?AMBER:*label?WHITE:GREY;
    if(focused){
        rect(p,x,198,w,2,ink);
        rect(p,x,198,2,34,ink);
        rect(p,x+w-2,198,2,34,ink);
    }
    text(p,label,x+4,200,alternate?w-22:w-8,12,12,ink,1);
    if(alternate)SHIFT_BADGE_COLOURED(p->rect,p->user,(int)column*106+94,200,
                                       alternate==2?0xffff4747u:WHITE);
    text(p,value,x+4,214,w-8,19,16,ink,1);
}
static void footer(const struct mixer_view_state*s,const struct resample_painter*p)
{
    rect(p,0,198,320,42,BLACK);
    struct mixer_mix resolved[8];
    mixer_scene_resolve(s->project,s->part_id,resolved);
    const struct mixer_mix*m=s->master?&s->project->master.mix:resolved+s->primary;
    char value[32];
    pan(value,sizeof(value),m->pan);
    knob_cell(p,0,"Pan",value,s->cursor==1,0);
    if(s->master)knob_cell(p,1,"","",0,0);
    else if(s->shifted){
        unsigned destination=s->project->send_to[s->primary];
        if(destination)snprintf(value,sizeof(value),"Track %c",'A'+destination-1);
        else snprintf(value,sizeof(value),"Off");
        knob_cell(p,1,"Send To",value,s->cursor==3,2);
    }else{
        snprintf(value,sizeof(value),"%.0f%%",m->send*100.f);
        knob_cell(p,1,"Send",value,s->cursor==3,1);
    }
    const char*name=s->master?"Green":mixer_colour_name(s->project->track[s->primary].colour);
    knob_cell(p,2,"Colour",name?name:"Custom",s->cursor==2,0);
    if(s->message&&*s->message&&s->page==MIXER_OVERVIEW){
        rect(p,0,186,320,12,BLACK);text(p,s->message,4,186,312,12,9,AMBER,1);
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
    const int left=42,top=72,width=260,height=102;
    const double span=log(1000.);
    if(s->fx_signal)fx_spectrum(p,s->fx_signal->pcm,rate,left,top,width,height,0xe05a4419u,1);
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
        rect(p,left+x,start,1,x?abs(y-previous)+1:1,eq->bypass?GREY:AMBER);
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
    text(p,"1k",174,176,30,10,10,GREY,0);
    text(p,"20k",270,177,32,10,10,GREY,0);
}
static int signal_width(float db)
{
    return isfinite(db)?(int)lroundf(228*fmaxf(0,fminf(1,(db+60)/60))):0;
}
static void signal_meter(const struct resample_painter*p,const char*label,float peak,int y,int height,
                         uint32_t ink,float threshold)
{
    rect(p,65,y,228,height,DARK);
    int width=signal_width(mixer_meter_dbfs(peak));
    if(width)rect(p,65,y,width,height,ink);
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
    signal_meter(p,"Send",s->meter->return_peak[track],189,5,0xffff4040u,NAN);
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
    signal_meter(p,"Input",m?m->input_peak:0,103,9,0xff60b8e0u,NAN);
    signal_meter(p,"Output",m?m->output_peak:0,124,9,AMBER,NAN);
    char value[40];
    if(stage->kind==FX_STAGE_SIDECHAIN){
        float threshold=fx_sidechain_value(1,stage->parameter[1]);
        signal_meter(p,"Key",m?m->key_peak:0,145,9,0xff60b8e0u,threshold);
        snprintf(value,sizeof(value),"Threshold %.1f dBFS",threshold);
        text(p,value,64,157,229,13,10,WHITE,0);
        return;
    }
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
static void effect_editor(const struct mixer_view_state*s,const struct resample_painter*p,const struct fx_stage*stage)
{
    const struct fx_catalog_entry*e=fx_catalog_find(stage->kind==FX_STAGE_EQ?0:stage->native_id);
    const char*label[3]={"","",""};
    char value[3][20]={{0}},title[64],status[64];
    unsigned alternate[3]={0};
    const struct mixer_channel*ch=s->master?&s->project->master:s->project->track+s->primary;
    rect(p,0,36,320,204,BLACK);
    snprintf(title,sizeof(title),"%s%s",fx_catalog_stage_name(stage),stage->bypass||ch->fx_bypass?"  (off)":"");
    int is_eq=stage->kind==FX_STAGE_EQ&&stage->eq.count;
    text(p,title,is_eq?18:12,is_eq?36:38,is_eq?175:296,25,19,0xff000000u|mixer_fx_slot_rgb(s->fx_slot),0);
    if(stage->kind==FX_STAGE_EQ&&stage->eq.count){
        const struct fx_eq_config*eq=&stage->eq;
        unsigned index=s->parameter_page<eq->count?s->parameter_page:0;
        const struct fx_eq_band*b=eq->band+index;
        eq_plot(s,p,eq);
        snprintf(status,sizeof(status),"Point %u/%u",index+1,eq->count);
        text(p,status,198,38,108,24,13,AMBER,1);
        if(s->message&&*s->message)text(p,s->message,16,57,288,13,10,AMBER,1);
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
    }else{
        unsigned pages=(fx_catalog_parameter_count(e)+5)/6;
        if(!pages)pages=1;
        for(unsigned page=0;page<pages;++page){
            int x=12+(int)page*52;
            char number[16];snprintf(number,sizeof(number),"%u",page+1);
            rect(p,x,64,49,17,page==s->parameter_page?0xff473416u:DARK);
            text(p,number,x,64,49,17,11,page==s->parameter_page?AMBER:GREY,1);
        }
        const struct mixer_fx_monitor*m=s->fx_signal;
        if(e&&!strcmp(e->category,"Dynamics")){
            fx_dynamics(p,e,stage,m);
        }else{
            if(e&&!strcmp(e->category,"Filter")){
                double rate=s->rate?s->rate:48000;
                if(m){
                    fx_spectrum(p,m->input_pcm,rate,16,87,288,43,0xff386477u,1);
                    fx_spectrum(p,m->pcm,rate,16,87,288,43,AMBER,0);
                }
            }else{
                int envelope=e&&(!strcmp(e->category,"Delay")||!strcmp(e->category,"Reverb"));
                rect(p,16,111,288,1,DARK);
                if(m){
                    fx_trace(p,m->input_pcm,0xff60b8e0u,envelope);
                    fx_trace(p,m->pcm,AMBER,envelope);
                }
            }
            signal_meter(p,"Input",m?m->input_peak:0,138,8,0xff60b8e0u,NAN);
            signal_meter(p,"Output",m?m->output_peak:0,155,8,AMBER,NAN);
        }
        for(unsigned knob=0;knob<3;++knob){
            unsigned position=s->parameter_page*6+knob;
            if(fx_catalog_parameter_index(e,position+3)>=0)alternate[knob]=s->shifted?2:1;
            int index=fx_catalog_parameter_index(e,position+(s->shifted?3:0));
            if(index<0)continue;
            label[knob]=e->parameter[index];
            if(stage->kind==FX_STAGE_SIDECHAIN){
                float v=fx_sidechain_value((unsigned)index,stage->parameter[index]);
                const char*formats[]={"","%.1f dB","%.1f:1","%.1f ms","%.0f ms","%.0f%%"};
                if(index==0){
                    if(v)snprintf(value[knob],sizeof(value[knob]),"Track %c",'A'+(int)v-1);
                    else snprintf(value[knob],sizeof(value[knob]),"Off");
                }else snprintf(value[knob],sizeof(value[knob]),formats[index],v);
            }else snprintf(value[knob],sizeof(value[knob]),"%.0f%%",stage->parameter[index]*100.f);
        }
        snprintf(status,sizeof(status),"%s    %s",pages>1?"DATA: page":"Parameters",s->shifted?"Shift controls":"Shift: more controls");
    }
    if(!is_eq)text(p,s->message&&*s->message?s->message:status,10,173,300,13,10,WHITE,1);
    for(unsigned i=0;i<3;++i)knob_cell(p,i,label[i],value[i],0,alternate[i]);
    if(is_eq)eq_knob_bars(s,p,&stage->eq);
    mixer_view_meters(s,1u<<(s->master?8:s->primary),p);
}
static void track_fx(const struct mixer_view_state*s,const struct resample_painter*p)
{
    const struct mixer_channel*ch=s->master?&s->project->master:s->project->track+s->primary;
    const struct fx_stage*stage=s->fx_slot<ch->fx.count?ch->fx.stage+s->fx_slot:NULL;
    rect(p,0,198,320,42,BLACK);
    if(s->fx_delete){
        text(p,"Delete effect?",85,75,230,27,20,WHITE,1);
        if(stage)text(p,fx_catalog_stage_name(stage),85,111,230,25,17,AMBER,1);
        text(p,"B1 Cancel        B3 Delete",85,154,230,22,13,WHITE,1);
        return;
    }
    text(p,s->fx_catalog?"Add Effect":ch->fx_bypass?"Track FX (bypassed)":"Track FX",86,36,230,20,15,AMBER,0);
    if(s->fx_catalog){
        unsigned total=fx_mixer_catalog_count();
        unsigned first=s->cursor>2?s->cursor-2:0;
        if(first+6>total)first=total>6?total-6:0;
        for(unsigned row=0;row<6&&first+row<total;++row){
            unsigned index=first+row;
            int y=57+(int)row*19;
            if(index==s->cursor)rect(p,84,y,232,19,DARK);
            text(p,fx_mixer_catalog_at(index)->name,90,y,222,19,15,index==s->cursor?AMBER:WHITE,0);
        }
        rect(p,80,198,240,1,DARK);
        text(p,"DATA: browse / press to add",86,202,230,16,11,AMBER,0);
        return;
    }
    for(unsigned i=0;i<MIXER_FX_SLOTS;++i){
        const struct fx_stage*slot=i<ch->fx.count?ch->fx.stage+i:NULL;
        int y=56+(int)i*19;
        if(i==s->fx_slot)rect(p,84,y,232,19,DARK);
        char line[64];
        snprintf(line,sizeof(line),"%u   %s%s",i+1,slot?fx_catalog_stage_name(slot):"Empty",slot&&slot->bypass?" (off)":"");
        uint32_t ink=0xff000000u|mixer_fx_slot_rgb(i);
        rect(p,86,y+3,3,13,ink);
        text(p,line,94,y,218,19,15,slot||i==s->fx_slot?ink:GREY,0);
    }
    const char*hint=s->shifted?"Turn: move FX   Press: bypass":"Turn: slot   Press: open   Erase: delete";
    text(p,s->message&&*s->message?s->message:hint,86,185,230,12,9,AMBER,0);
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
        else{
            if(!s->message||!*s->message)text(p,"Pads assign / unassign   Bank +/-",86,186,230,12,9,GREY,0);
            footer(s,p);
        }
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

    if(s->page==MIXER_OVERVIEW){
        for(unsigned i=0;i<8;++i)if(mask&(1u<<i)){
            int x=(int)(i%4)*80,y=i<4?121:44;
            vmeter(s,i,x+34,y+18,12,77-24,p);
            send_meter(s,i,x+22,y+20,77-26,p);
        }
    }else{
        unsigned track=s->master?8:s->primary;
        if(mask&(1u<<track)){
            if(s->page==MIXER_KNOB_FX&&s->fx_parameters){
                effect_send_meter(s,p);
            }else{
                vmeter(s,track==8?9:track,46,106,14,86,p);
                if(track<MIXER_TRACKS)send_meter(s,track,32,106,86,p);
            }
        }
    }
}
void mixer_view_paint(const struct mixer_view_state*s,const struct resample_painter*p)
{
    if(!s||!s->project||!p||!p->rect||!p->text||s->primary>=8||s->bank>=8||s->page>MIXER_INSPECT)return;
    header(s,p);
    if(s->name_edit){
        rect(p,0,36,320,204,BLACK);
        text(p,"Track name",12,56,296,25,19,WHITE,0);
        rect(p,12,94,296,43,DARK);
        text(p,s->name_edit,19,96,282,39,19,AMBER,0);
        text(p,s->name_pads?"Pads type   DATA moves":"DATA edits   Press to advance",12,151,296,19,13,WHITE,0);
        if(s->message&&*s->message)text(p,s->message,12,176,296,17,11,AMBER,0);
        knob_cell(p,0,"Input",s->name_pads?"Pads":"Encoder",0,0);
        return;
    }
    if(s->page==MIXER_KNOB_FX&&s->fx_parameters){
        const struct mixer_channel*ch=s->master?&s->project->master:s->project->track+s->primary;
        if(s->fx_slot<ch->fx.count){effect_editor(s,p,ch->fx.stage+s->fx_slot);return;}
    }
    if(s->page==MIXER_ASSIGN||s->page==MIXER_INSPECT||s->page==MIXER_KNOB_FX){
        mixer_view_channel_strip(s,s->master?8:s->primary,0,80,p);mixer_view_context(s,p);}
    else {mixer_view_overview(s,p);footer(s,p);}
}
