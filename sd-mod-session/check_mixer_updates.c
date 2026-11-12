#include "src/mixer_buses.h"
#include <assert.h>
#include <math.h>
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
static _Thread_local int audio;
void*__real_malloc(size_t);void*__real_calloc(size_t,size_t);void*__real_realloc(void*,size_t);void __real_free(void*);
void*__wrap_malloc(size_t n){assert(!audio);return __real_malloc(n);}
void*__wrap_calloc(size_t n,size_t s){assert(!audio);return __real_calloc(n,s);}
void*__wrap_realloc(void*p,size_t n){assert(!audio);return __real_realloc(p,n);}
void __wrap_free(void*p){assert(!audio);__real_free(p);}
static struct mixer_project p;
static float input[64];
static void block(struct mixer_buses*b,unsigned n)
{
    audio=1;assert(mixer_buses_begin(b,n)==MIXER_BUS_OK);
    assert(mixer_buses_accumulate(b,0,input,input,2,0)==MIXER_BUS_OK);
    assert(mixer_buses_finish(b,NULL,0)==MIXER_BUS_OK);audio=0;
}
static float sample(struct mixer_buses*b,unsigned bus,unsigned at){struct mixer_pcm_view v;assert(mixer_buses_view(b,bus,&v));return v.channel[0][at];}
static void close_to(float a,float b){assert(fabsf(a-b)<1e-6f);}
static void publish(struct mixer_buses*b,uint64_t revision,const struct fx_backend*backend)
{
    enum mixer_bus_result error;struct mixer_bus_update*u=mixer_buses_update_prepare(&p,0,8000,revision,backend,&error);
    assert(u&&error==MIXER_BUS_OK&&mixer_buses_publish(b,u)==MIXER_BUS_OK);
}
static void ramps(void)
{
    mixer_defaults(&p);p.output[0]=1;enum mixer_bus_result error;
    struct mixer_buses*b=mixer_buses_prepare(&p,0,8000,64,NULL,&error);assert(b);
    block(b,20);close_to(sample(b,0,0),.25f);
    p.track[0].mix.level_db=-6.0206f;publish(b,1,NULL);block(b,20);
    close_to(sample(b,0,0),.25f-.125f/40);close_to(sample(b,0,19),.1875f);assert(!mixer_buses_applied_revision(b));
    block(b,20);close_to(sample(b,0,19),.125f);assert(mixer_buses_applied_revision(b)==1);mixer_buses_collect(b);
    p.track[0].mix.level_db=0;p.output[0]=2;publish(b,2,NULL);block(b,40);
    close_to(sample(b,1,0),.25f/40);close_to(sample(b,1,39),.25f);close_to(sample(b,0,39),0);
    assert(mixer_buses_applied_revision(b)==2);mixer_buses_collect(b);
    p.track[1].mix.mute=1;publish(b,3,NULL);block(b,40);
    close_to(sample(b,1,0),.25f*39/40);close_to(sample(b,1,39),0);mixer_buses_collect(b);
    assert(mixer_buses_begin(b,40)==MIXER_BUS_OK);
    p.track[1].mix.mute=0;publish(b,4,NULL);
    audio=1;assert(mixer_buses_accumulate(b,0,input,input,2,0)==MIXER_BUS_OK);
    assert(mixer_buses_finish(b,NULL,0)==MIXER_BUS_OK);audio=0;
    close_to(sample(b,1,39),0);assert(mixer_buses_applied_revision(b)==3);
    block(b,40);close_to(sample(b,1,39),.25f);assert(mixer_buses_applied_revision(b)==4);mixer_buses_collect(b);
    struct mixer_bus_update*u=mixer_buses_update_prepare(&p,0,8000,4,NULL,&error);assert(u);
    assert(mixer_buses_publish(b,u)==MIXER_BUS_INVALID);mixer_buses_update_destroy(u);
    u=mixer_buses_update_prepare(&p,0,16000,5,NULL,&error);assert(u);
    assert(mixer_buses_publish(b,u)==MIXER_BUS_INVALID);mixer_buses_update_destroy(u);
    p.master.mix.send=.5f;assert(!mixer_buses_update_prepare(&p,0,8000,5,NULL,&error)&&error==MIXER_BUS_UNSUPPORTED);
    mixer_buses_destroy(b);puts("PASS block-boundary Level/Pan/gates and Pad routing ramps, exact endpoints, delayed revision acknowledgement and off-RT collection");
}
struct effect {unsigned id,calls;};
static unsigned created,destroyed;
static void*create(void*u,uint32_t id,double rate,const float*params){(void)u;(void)params;assert(!audio&&rate==8000);struct effect*e=calloc(1,sizeof(*e));assert(e);e->id=id;++created;return e;}
static void destroy(void*u,void*p){(void)u;assert(!audio);++destroyed;free(p);}
static void process(void*u,void*p,float*pcm,size_t n){(void)u;assert(audio);struct effect*e=p;++e->calls;
    for(size_t i=0;i<n*2;++i)pcm[i]=e->id==1?pcm[i]+e->calls*.01f:pcm[i]*2;}
static void effects(void)
{
    mixer_defaults(&p);p.output[0]=1;p.track[0].fx.count=1;p.track[0].fx.stage[0].kind=FX_STAGE_NATIVE;p.track[0].fx.stage[0].native_id=1;
    struct fx_backend backend={NULL,create,destroy,process};enum mixer_bus_result error;
    struct mixer_buses*b=mixer_buses_prepare(&p,0,8000,64,&backend,&error);assert(b);
    block(b,40);close_to(sample(b,0,0),.26f);
    p.track[0].mix.pan=1;publish(b,1,&backend);block(b,40);close_to(sample(b,0,0),.27f*39/40);
    struct mixer_pcm_view v;assert(mixer_buses_view(b,0,&v));close_to(v.channel[1][39],.27f);mixer_buses_collect(b);
    p.track[0].mix.pan=0;p.track[0].fx.stage[0].native_id=2;publish(b,2,&backend);block(b,40);
    assert(mixer_buses_view(b,0,&v));close_to(v.channel[1][0],.28f+(.5f-.28f)/40);close_to(v.channel[1][39],.5f);
    mixer_buses_collect(b);mixer_buses_destroy(b);assert(created==destroyed);
    puts("PASS unchanged FX retain history across mix edits; changed FX crossfade from the same input, with all processor destruction off audio");
}
static void flush_fx(void)
{
    mixer_defaults(&p);p.output[0]=1;p.track[0].fx.count=1;p.track[0].fx.stage[0].kind=FX_STAGE_NATIVE;p.track[0].fx.stage[0].native_id=1;
    struct fx_backend backend={NULL,create,destroy,process};enum mixer_bus_result error;
    unsigned before=created;
    struct mixer_buses*b=mixer_buses_prepare(&p,0,8000,64,&backend,&error);assert(b);
    block(b,40);block(b,40);close_to(sample(b,0,39),.27f); /* state has built up: second call */
    struct mixer_bus_update*u=mixer_buses_update_prepare_delta(&p,0,8000,1,&backend,NULL,&error);assert(u&&error==MIXER_BUS_OK);
    mixer_buses_update_flush(u);assert(mixer_buses_publish(b,u)==MIXER_BUS_OK);
    block(b,40);block(b,40);mixer_buses_collect(b);
    close_to(sample(b,0,39),.27f);                   /* same two-call history again: the program restarted from scratch */
    assert(created==before+2);                         /* exactly one new processor was built */
    mixer_buses_collect(b);mixer_buses_destroy(b);assert(created==destroyed);
    puts("PASS double-Stop flush rebuilds FX programs at the same revision, off the audio thread, restarting their state");
}
static struct mixer_buses*shared;
static _Atomic unsigned finished;
static void*producer(void*unused)
{
    (void)unused;for(unsigned rev=1;rev<=100;++rev){p.track[0].mix.level_db=-(float)(rev%6);
        enum mixer_bus_result error;struct mixer_bus_update*u=mixer_buses_update_prepare(&p,0,8000,rev,NULL,&error);assert(u);
        while(mixer_buses_publish(shared,u)==MIXER_BUS_STATE){mixer_buses_collect(shared);sched_yield();}
        while(mixer_buses_applied_revision(shared)<rev){mixer_buses_collect(shared);sched_yield();}
    }mixer_buses_collect(shared);atomic_store_explicit(&finished,1,memory_order_release);return NULL;
}
static void concurrent(void)
{
    mixer_defaults(&p);p.output[0]=1;enum mixer_bus_result error;shared=mixer_buses_prepare(&p,0,8000,64,NULL,&error);assert(shared);
    pthread_t thread;assert(!pthread_create(&thread,NULL,producer,NULL));
    while(!atomic_load_explicit(&finished,memory_order_acquire)){block(shared,64);float v=sample(shared,9,63);assert(v>=0&&v<=.251f);}
    assert(!pthread_join(thread,NULL)&&mixer_buses_applied_revision(shared)==100);mixer_buses_destroy(shared);
    puts("PASS concurrent control/audio publication, coherent complete snapshots and bounded processing without RT allocation/free");
}
int main(void){for(unsigned i=0;i<64;++i)input[i]=.25f;ramps();effects();flush_fx();concurrent();return 0;}
