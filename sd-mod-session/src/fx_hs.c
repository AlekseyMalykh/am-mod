#include "fx_hs.h"
#include "hs_dsp/ladspa.h"
#include "hs_dsp/hs_saturator.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
/* Henry Skillz DSP: https://github.com/Holla-Back-Entertainment/hs-mpc-plugins (author's permission). */
extern const LADSPA_Descriptor hs_heat_descriptor,hs_bass_focus_descriptor;
/* Key bus is an HS-to-HS feature; Am MOD has no bus, so MOD Source stays Off. */
float hs_env_read(int bus){(void)bus;return 0.f;}
void hs_env_send(int bus,float peak){(void)bus;(void)peak;}
#define BLOCK 128u
#define FIRST_CTL 4u
#define MAX_PORTS 32u
struct fx_hs {
    uint32_t id;double rate;
    const LADSPA_Descriptor *d;LADSPA_Handle h;
    unsigned nctl;float ctl[MAX_PORTS];
    float in[2][BLOCK],out[2][BLOCK];
    struct hs_saturator sat;
};
int fx_hs_is(uint32_t id){return id>=FX_HS_SATURATOR&&id<FX_HS_SATURATOR+FX_HS_COUNT;}
static unsigned user_params(uint32_t id){return id==FX_HS_HEAT?18u:3u;}
/* normalized 0..1 -> the effect's own range, with the port's own hints */
static float native(const LADSPA_PortRangeHint *h,float n)
{
    n=fminf(1.f,fmaxf(0.f,isfinite(n)?n:0.f));
    float lo=h->LowerBound,hi=h->UpperBound;int hd=h->HintDescriptor;
    if(LADSPA_IS_HINT_TOGGLED(hd))return n>=.5f?1.f:0.f;
    float v=(LADSPA_IS_HINT_LOGARITHMIC(hd)&&lo>0&&hi>0)?lo*powf(hi/lo,n):lo+n*(hi-lo);
    if(LADSPA_IS_HINT_INTEGER(hd))v=roundf(v);
    return fminf(hi,fmaxf(lo,v));
}
static void apply(struct fx_hs *s,const float *p)
{
    if(s->id==FX_HS_SATURATOR){
        float drive=24.f*fminf(1.f,fmaxf(0.f,p[0]));
        hs_saturator_configure(&s->sat,(float)s->rate,drive,p[1],p[2]);
        return;
    }
    for(unsigned i=0;i<user_params(s->id);++i)s->ctl[FIRST_CTL+i]=native(&s->d->PortRangeHints[FIRST_CTL+i],p[i]);
}
void *fx_hs_create(uint32_t id,double rate,const float *p)
{
    if(!fx_hs_is(id)||!p||!isfinite(rate)||rate<8000||rate>192000)return NULL;
    struct fx_hs *s=calloc(1,sizeof(*s));if(!s)return NULL;
    s->id=id;s->rate=rate;
    if(id!=FX_HS_SATURATOR){
        s->d=id==FX_HS_HEAT?&hs_heat_descriptor:&hs_bass_focus_descriptor;
        s->h=s->d->instantiate(s->d,(unsigned long)rate);
        if(!s->h){free(s);return NULL;}
        /* Heat's MOD block (ports 22-26) is not exposed: Source Off, Depth 0, the author's follower times. */
        if(id==FX_HS_HEAT){s->ctl[22]=0.f;s->ctl[23]=0.f;s->ctl[24]=0.f;s->ctl[25]=5.623f;s->ctl[26]=223.6f;}
        s->d->connect_port(s->h,0,s->in[0]);s->d->connect_port(s->h,1,s->in[1]);
        s->d->connect_port(s->h,2,s->out[0]);s->d->connect_port(s->h,3,s->out[1]);
        for(unsigned i=FIRST_CTL;i<s->d->PortCount;++i)s->d->connect_port(s->h,i,&s->ctl[i]);
        if(s->d->activate)s->d->activate(s->h);
    }
    apply(s,p);
    return s;
}
void fx_hs_update(void *o,const float *p){struct fx_hs *s=o;if(s&&p)apply(s,p);}
void fx_hs_destroy(void *o)
{
    struct fx_hs *s=o;if(!s)return;
    if(s->h&&s->d->cleanup)s->d->cleanup(s->h);
    free(s);
}
float fx_hs_process(void *o,float *x,size_t frames)
{
    struct fx_hs *s=o;if(!s||!x)return 1.f;
    for(size_t at=0;at<frames;){
        size_t n=frames-at;if(n>BLOCK)n=BLOCK;
        for(size_t i=0;i<n;++i){s->in[0][i]=x[(at+i)*2];s->in[1][i]=x[(at+i)*2+1];}
        if(s->id==FX_HS_SATURATOR){
            hs_saturator_process(&s->sat,s->in[0],s->in[1],n);
            for(size_t i=0;i<n;++i){x[(at+i)*2]=s->in[0][i];x[(at+i)*2+1]=s->in[1][i];}
        }else{
            s->d->run(s->h,(unsigned long)n);
            for(size_t i=0;i<n;++i){x[(at+i)*2]=s->out[0][i];x[(at+i)*2+1]=s->out[1][i];}
        }
        at+=n;
    }
    return 1.f;
}
