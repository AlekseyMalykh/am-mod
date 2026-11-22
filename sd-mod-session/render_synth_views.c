/* Renders the real mixer_view_paint() synth screens (traces) for the release documentation.
 * Parameter values and the live meter are fixtures, so the PNGs are review artefacts, not hardware screenshots. */
#include "src/mixer_view.h"
#include "src/mixer_buses.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
unsigned rear_setting(unsigned ch,unsigned field){(void)ch;(void)field;return 0;}
int rear_native_source(void){return 1;}
static FILE*trace;
static void r(void*u,int x,int y,int w,int h,uint32_t c){(void)u;if(trace)fprintf(trace,"R %d %d %d %d %08x\n",x,y,w,h,c);}
static void t(void*u,const char*s,int x,int y,int w,int h,unsigned size,uint32_t c,int centre){(void)u;if(trace)fprintf(trace,"T %d %d %d %d %u %08x %d %s\n",x,y,w,h,size,c,centre,s);}
static unsigned params[64]={1,100,2,40,90,40,100,15,0,1, 0,0,0,0,0,0,0,0,0,0,0,0, 2,70,0,64,30,0,64,21,2,40,90,40,0,0};
unsigned am_synth_param_get(unsigned i){return i<64?params[i]:0;}
struct synth_live{float peak,env_level,bend,mod;unsigned env_stage,voices;unsigned notes[4];};
int am_synth_peek(struct synth_live*l){memset(l,0,sizeof *l);l->peak=.16f;l->env_level=.7f;l->env_stage=2;l->voices=2;l->notes[0]=60;l->notes[1]=64;return 1;}
uint64_t am_synth_pad_info(unsigned pad,unsigned*note){if(note)*note=60+pad;return (uint64_t)0xff000000u|(pad%4==0?0x00c8a030u:0x00404848u);}
void music_get_key(int*a,int*b){*a=0;*b=1;}
unsigned music_light_colour_count(void){return 12;}
unsigned music_light_colour_choice(unsigned i){return i;}
unsigned music_light_colour_index(unsigned i){return i;}
const char*music_light_colour_name(unsigned i){static const char*n[]={"Red","Orange","Yellow","Lime","Green","Teal","Cyan","Blue","Violet","Pink","White","Grey"};return n[i%12];}
int main(int argc,char**argv){
    assert(argc==2);struct mixer_project project;mixer_defaults(&project);
    struct mixer_view_state s={.project=&project,.now_ms=1000,.epoch=1};
    struct resample_painter painter={NULL,r,t};char path[1024];
    const char*names[9]={"synth-osc","synth-mix","synth-amp","synth-fenv","synth-keys","synth-filter","synth-lfo","synth-voices","synth-controller"};
    s.synth_page=1;s.synth_keys=0;s.synth_layout=2;s.synth_transpose=24;s.synth_root=1;s.synth_scale=1;
    for(unsigned p=0;p<9;++p){s.synth_cursor=p;
        for(unsigned sh=0;sh<2;++sh){s.synth_shift=sh;snprintf(path,sizeof path,"%s/%s%s.trace",argv[1],names[p],sh?"-shift":"");
            trace=fopen(path,"w");assert(trace);mixer_view_paint(&s,&painter);fclose(trace);trace=NULL;}}
    s.shifted=0;s.synth_pick=1;s.synth_cursor=0;snprintf(path,sizeof path,"%s/synth-loop.trace",argv[1]);trace=fopen(path,"w");mixer_view_paint(&s,&painter);fclose(trace);
    struct mixer_view_state q={.project=&project,.now_ms=1000,.epoch=1,.master=2,.page=MIXER_KNOB_FX,.input_tab=2};
    for(unsigned c=0;c<2;++c){q.cursor=c;snprintf(path,sizeof path,"%s/input-route%u.trace",argv[1],c);trace=fopen(path,"w");assert(trace);mixer_view_paint(&q,&painter);fclose(trace);trace=NULL;}
    q.input_tab=1;snprintf(path,sizeof path,"%s/input-fx.trace",argv[1]);trace=fopen(path,"w");mixer_view_paint(&q,&painter);fclose(trace);trace=NULL;
    return 0;}
