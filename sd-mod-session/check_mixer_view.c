#include "src/mixer_view.h"
#include "src/mixer_buses.h"
#include "src/fx_catalog.h"
#include <assert.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
static FILE*trace;
static unsigned dynamic,draws,na,thumbs;
static uint64_t strip_hash;
static unsigned colour_labels,custom_labels,pickup_labels;
static void hash(const void*data,size_t bytes)
{const unsigned char*p=data;while(bytes--){strip_hash^=*p++;strip_hash*=UINT64_C(1099511628211);}}
static void r(void*u,int x,int y,int w,int h,uint32_t c){(void)u;
    assert(x>=0&&y>=0&&w>0&&h>0&&x+w<=320&&y+h<=240);++draws;
    if(dynamic)assert(y>=102&&y+h<=195);
    if(h==5&&w==12)++thumbs;
    if(x+w<=80&&y>=50&&y+h<=198){int shape[]={x,y,w,h};hash(shape,sizeof(shape));hash(&c,sizeof(c));}
    if(trace)fprintf(trace,"R %d %d %d %d %08x\n",x,y,w,h,c);}
static void t(void*u,const char*s,int x,int y,int w,int h,unsigned size,uint32_t c,int centre){(void)u;
    assert(s&&x>=0&&y>=0&&w>0&&h>0&&x+w<=320&&y+h<=240&&size>=8);++draws;
    if(!strcmp(s,"N/A"))++na;
    if(!strcmp(s,"Colour"))++colour_labels;
    if(!strcmp(s,"#123456"))++custom_labels;
    if(!strcmp(s,"PICKUP"))++pickup_labels;
    if(x+w<=80&&y>=50&&y+h<=198){int shape[]={x,y,w,h,(int)size,centre};hash(shape,sizeof(shape));hash(&c,sizeof(c));hash(s,strlen(s));}
    if(dynamic)assert(y>=102&&y+h<=195);
    if(trace)fprintf(trace,"T %d %d %d %d %u %08x %d %s\n",x,y,w,h,size,c,centre,s);}
int main(int argc,char**argv)
{
    assert(argc==2);struct mixer_project project;mixer_defaults(&project);
    const char*names[]={"Kick","Snare","Hats","Bass","Keys","Vox","Perc","FX"};
    const float pans[]={-1,-.5f,0,.25f,.5f,.75f,1,-.25f};
    for(unsigned i=0;i<8;++i){strcpy(project.track[i].name,names[i]);project.output[i]=(unsigned char)(i+1);project.track[i].mix.level_db=-(float)i*3;project.track[i].mix.pan=pans[i];}
    project.track[7].mix.level_db=-INFINITY;project.track[1].mix.mute=1;
    project.track[2].fx.count=1;assert(fx_catalog_stage(0,48000,project.track[2].fx.stage));
    struct mixer_meters*m=mixer_meters_create(48000);enum mixer_bus_result error;struct mixer_buses*b=mixer_buses_prepare(&project,0,48000,16,NULL,&error);assert(b&&m);
    float pcm[16];assert(mixer_buses_begin(b,16)==MIXER_BUS_OK);
    for(unsigned i=0;i<8;++i){for(unsigned n=0;n<16;++n)pcm[n]=(i+1)*.025f;
        assert(mixer_buses_accumulate(b,i,pcm,pcm,2,0)==MIXER_BUS_OK);}
    assert(mixer_buses_finish(b,m,7)==MIXER_BUS_OK);struct mixer_meter_snapshot snapshot;assert(mixer_meters_take(m,7,&snapshot));
    struct mixer_view_state s={.selected=5,.primary=2,.project=&project,.meter=&snapshot,.epoch=7,.meter_ms=1000,.now_ms=1000};
    struct resample_painter painter={NULL,r,t};char path[1024];
    const char*files[]={"overview","mix","fx","route","knob-fx","advanced","assign","inspect"};
    for(unsigned page=0;page<=MIXER_INSPECT;++page){s.page=(enum mixer_page)page;s.selected=page>=MIXER_KNOB_FX?4:5;
        snprintf(path,sizeof(path),"%s/%s.trace",argv[1],files[page]);trace=fopen(path,"w");assert(trace);thumbs=0;
        mixer_view_paint(&s,&painter);fclose(trace);trace=NULL;assert(thumbs==(page?1u:8u));}
    s.page=MIXER_KNOB_FX;s.fx_catalog=1;snprintf(path,sizeof(path),"%s/catalog.trace",argv[1]);trace=fopen(path,"w");assert(trace);mixer_view_paint(&s,&painter);fclose(trace);trace=NULL;s.fx_catalog=0;
    s.shifted=1;snprintf(path,sizeof(path),"%s/eq-shift.trace",argv[1]);trace=fopen(path,"w");assert(trace);mixer_view_paint(&s,&painter);fclose(trace);trace=NULL;s.shifted=0;
    s.page=MIXER_MIX;s.master=1;snprintf(path,sizeof(path),"%s/master.trace",argv[1]);trace=fopen(path,"w");assert(trace);mixer_view_paint(&s,&painter);fclose(trace);trace=NULL;
    s.master=0;s.selected=4;strip_hash=0;mixer_view_paint(&s,&painter);uint64_t unchanged_strip=strip_hash;
    s.cursor=5;s.scroll=1;s.colour_edit=1;strip_hash=0;colour_labels=0;
    snprintf(path,sizeof(path),"%s/colour.trace",argv[1]);trace=fopen(path,"w");assert(trace);mixer_view_paint(&s,&painter);fclose(trace);trace=NULL;
    assert(strip_hash==unchanged_strip&&colour_labels==2);
    pickup_labels=0;s.message="PICKUP";mixer_view_paint(&s,&painter);assert(pickup_labels==1);s.message=NULL;
    project.track[2].colour=0x123456;custom_labels=0;mixer_view_paint(&s,&painter);assert(custom_labels==2);
    s.master=1;snprintf(path,sizeof(path),"%s/master-colour.trace",argv[1]);trace=fopen(path,"w");assert(trace);mixer_view_paint(&s,&painter);fclose(trace);trace=NULL;
    s.master=s.cursor=s.scroll=s.colour_edit=0;
    s.page=MIXER_OVERVIEW;s.master=0;dynamic=1;draws=0;mixer_view_meters(&s,1,&painter);assert(draws>0&&draws<15);dynamic=0;
    s.now_ms=1251;na=0;mixer_view_paint(&s,&painter);assert(na==8);
    s.now_ms=1000;s.epoch=8;na=0;mixer_view_paint(&s,&painter);assert(na==8);
    assert(mixer_view_gain_y(0)==114&&mixer_view_gain_y(6)==102&&mixer_view_gain_y(-60)==174&&mixer_view_gain_y(-INFINITY)==177);
    assert(mixer_view_gain_db(114)==0&&mixer_view_gain_db(102)==6&&isinf(mixer_view_gain_db(177)));
    mixer_buses_destroy(b);mixer_meters_destroy(m);puts("PASS eight saved-gain faders, distinct real PCM meters, split Track/Master pages, bounds, stale/epoch invalidation and meter-only repaint");return 0;
}
