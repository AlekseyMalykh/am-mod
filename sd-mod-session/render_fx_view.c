/* Host-only: paints one effect screen in Illustrated or Classic view and writes the
 * rect/text trace for render_fx_view.py. usage: render_fx_view <catalog index> <0|1> <out> [signal] */
#include "src/mixer_view.h"
#include "src/mixer_buses.h"
#include "src/fx_catalog.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
static FILE*out;
static void r(void*u,int x,int y,int w,int h,uint32_t c){(void)u;
    assert(x>=0&&y>=0&&w>0&&h>0&&x+w<=320&&y+h<=240);fprintf(out,"R %d %d %d %d %08x\n",x,y,w,h,c);}
static void t(void*u,const char*s,int x,int y,int w,int h,unsigned size,uint32_t c,int centre){(void)u;
    assert(s&&x>=0&&y>=0&&w>0&&h>0&&x+w<=320&&y+h<=240);fprintf(out,"T %d %d %d %d %u %08x %d %s\n",x,y,w,h,size,c,centre,s);}
int main(int argc,char**argv)
{
    assert(argc>=4);unsigned index=(unsigned)atoi(argv[1]);
    struct mixer_project project;mixer_defaults(&project);
    struct mixer_meter_snapshot snapshot={0};
    struct mixer_view_state s={.page=MIXER_KNOB_FX,.primary=0,.project=&project,.meter=&snapshot,.epoch=1,.now_ms=1000,.meter_ms=1000,
        .fx_editing=1,.fx_parameters=1,.rate=48000,.fx_illustrated=(unsigned)atoi(argv[2])};
    assert(fx_mixer_catalog_stage(index,s.rate,&project.track[0].fx.stage[0]));project.track[0].fx.count=1;
    for(int i=4;i<argc;++i){ /* optional "param=value" overrides */
        unsigned k;float v;if(sscanf(argv[i],"%u=%f",&k,&v)==2&&k<FX_CHAIN_PARAMETERS)project.track[0].fx.stage[0].parameter[k]=v;}
    static struct mixer_fx_monitor m;m.input_peak=.7f;m.output_peak=.5f;m.key_peak=.4f;m.gain_reduction_db=6;
    srand(3);
    for(unsigned i=0;i<MIXER_FX_MONITOR_FRAMES;++i){
        float n=(float)rand()/RAND_MAX-.5f;
        m.input_pcm[i]=.45f*sinf(i*.11f)+.2f*sinf(i*.9f)+.25f*n;
        m.pcm[i]=.45f*sinf(i*.11f)+.05f*sinf(i*.9f)+.04f*n;
    }
    s.fx_signal=&m;
    out=fopen(argv[3],"w");assert(out);
    struct resample_painter p={NULL,r,t};mixer_view_paint(&s,&p);fclose(out);
    return 0;
}
