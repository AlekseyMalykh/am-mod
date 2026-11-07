#include "src/mixer_view.h"
#include "src/mixer_buses.h"
#include "src/fx_catalog.h"
#include <assert.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
static FILE*trace;
static unsigned dynamic,draws,na,thumbs,letters,mstr,tabs;
static void r(void*u,int x,int y,int w,int h,uint32_t c){(void)u;
    assert(x>=0&&y>=0&&w>0&&h>0&&x+w<=320&&y+h<=240);++draws;
    if(dynamic)assert(y+h<=198);
    if(h==5&&w==12)++thumbs;
    if(trace)fprintf(trace,"R %d %d %d %d %08x\n",x,y,w,h,c);}
static void t(void*u,const char*s,int x,int y,int w,int h,unsigned size,uint32_t c,int centre){(void)u;
    assert(s&&x>=0&&y>=0&&w>0&&h>0&&x+w<=320&&y+h<=240&&size>=8);++draws;
    if(!strcmp(s,"N/A"))++na;
    if(strlen(s)==1&&s[0]>='A'&&s[0]<='H'&&size>=18)++letters;
    if(!strcmp(s,"MSTR"))++mstr;
    if(!strncmp(s,"B1",2)&&strlen(s)>3&&s[2]==' '&&s[3]!=':')++tabs;
    if(!strncmp(s,"Track ",6)&&s[6]>='A'&&s[6]<='H'&&!s[7])assert(0); /* no "Track A" labels */
    if(dynamic)assert(y+h<=198);
    if(trace)fprintf(trace,"T %d %d %d %d %u %08x %d %s\n",x,y,w,h,size,c,centre,s);}
int main(int argc,char**argv)
{
    assert(argc==2);struct mixer_project project;mixer_defaults(&project);
    const float pans[]={-1,-.5f,0,.25f,.5f,.75f,1,-.25f};
    for(unsigned i=0;i<8;++i){project.output[i]=(unsigned char)(i+1);project.track[i].mix.level_db=-(float)i*3;project.track[i].mix.pan=pans[i];}
    project.track[7].mix.level_db=-INFINITY;project.track[1].mix.mute=1;
    struct mixer_meters*m=mixer_meters_create(48000);enum mixer_bus_result error;struct mixer_buses*b=mixer_buses_prepare(&project,0,48000,16,NULL,&error);assert(b&&m);
    float pcm[16];assert(mixer_buses_begin(b,16)==MIXER_BUS_OK);
    for(unsigned i=0;i<8;++i){for(unsigned n=0;n<16;++n)pcm[n]=(i+1)*.025f;
        assert(mixer_buses_accumulate(b,i,pcm,pcm,2,0)==MIXER_BUS_OK);}
    assert(mixer_buses_finish(b,m,7)==MIXER_BUS_OK);struct mixer_meter_snapshot snapshot;assert(mixer_meters_take(m,7,&snapshot));
    struct mixer_view_state s={.selected=5,.primary=2,.project=&project,.meter=&snapshot,.epoch=7,.meter_ms=1000,.now_ms=1000};
    struct resample_painter painter={NULL,r,t};char path[1024];
    /* Mixer page: 2x4 Track cells, A-H letters, no tab labels, MSTR header. */
    s.page=MIXER_OVERVIEW;snprintf(path,sizeof(path),"%s/overview.trace",argv[1]);trace=fopen(path,"w");assert(trace);
    thumbs=letters=mstr=tabs=na=0;mixer_view_paint(&s,&painter);fclose(trace);trace=NULL;
    assert(thumbs==8&&letters==8&&mstr==1&&!tabs&&na==0);
    /* PAD FX / KNOB FX: left area with the big name; Master as MSTR. */
    const enum mixer_page pages[]={MIXER_ASSIGN,MIXER_INSPECT,MIXER_KNOB_FX};const char*files[]={"assign","inspect","knob-fx"};
    for(unsigned i=0;i<3;++i){s.page=pages[i];snprintf(path,sizeof(path),"%s/%s.trace",argv[1],files[i]);trace=fopen(path,"w");assert(trace);
        thumbs=letters=0;mixer_view_paint(&s,&painter);fclose(trace);trace=NULL;assert(thumbs==1&&letters>=1);}
    s.page=MIXER_ASSIGN;s.master=1;mstr=0;mixer_view_paint(&s,&painter);assert(mstr>=2);s.master=0;
    s.page=MIXER_OVERVIEW;dynamic=1;draws=0;mixer_view_meters(&s,1,&painter);assert(draws>0&&draws<15);dynamic=0;
    s.now_ms=1251;na=0;mixer_view_paint(&s,&painter);assert(na==9);
    s.now_ms=1000;s.epoch=8;na=0;mixer_view_paint(&s,&painter);assert(na==9);
    assert(mixer_view_gain_y(0)==114&&mixer_view_gain_y(6)==102&&mixer_view_gain_y(-60)==174&&mixer_view_gain_y(-INFINITY)==177);
    assert(mixer_view_gain_db(114)==0&&mixer_view_gain_db(102)==6&&isinf(mixer_view_gain_db(177)));
    mixer_buses_destroy(b);mixer_meters_destroy(m);puts("PASS Mixer page 2x4 Track cells with A-H, MSTR header meter, no tab labels, PAD FX/KNOB FX left area, bounds, stale/epoch invalidation and meter-only repaint");return 0;
}
