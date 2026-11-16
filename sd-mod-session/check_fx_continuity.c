#include "src/fx_chain.h"
#include "src/fx_own.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned made,freed,rt;
struct delay {float data[128],gain;unsigned at;};
static void*create(void*u,uint32_t id,double rate,const float*p){
    (void)u;(void)id;(void)rate;assert(!rt);++made;
    struct delay*d=calloc(1,sizeof(*d));assert(d);d->gain=.5f+p[0];return d;
}
static void destroy(void*u,void*p){(void)u;assert(!rt);++freed;free(p);}
static void process(void*u,void*p,float*x,size_t n){
    (void)u;assert(rt);struct delay*d=p;
    for(size_t i=0;i<n;++i){float y=d->data[d->at];d->data[d->at]=(x[i*2]+y)*.75f;
        d->at=(d->at+1)%128;x[i*2]=x[i*2+1]=y*d->gain;}
}
static void block(struct fx_exchange*e,float*x,size_t n){rt=1;fx_exchange_process(e,x,n);rt=0;}
int main(void){
    struct fx_backend backend={NULL,create,destroy,process};
    struct fx_chain_config c={.count=2,.stage={{.kind=FX_STAGE_NATIVE,.native_id=123},{.kind=FX_STAGE_EQ}}};
    fx_eq_default(&c.stage[1].eq);
    struct fx_exchange*e=fx_exchange_create();assert(e);
    assert(fx_exchange_seed(e,fx_program_prepare(&c,48000,&backend)));
    float x[2048]={1,1};block(e,x,64);assert(made==1);
    struct fx_chain_config next=c;next.stage[1].eq.band[0].gain_db=3;
    struct fx_program*p=fx_program_prepare_delta(&next,48000,&backend,&c);assert(p&&made==1);
    assert(fx_exchange_publish(e,p)==FX_OK);memset(x,0,sizeof x);block(e,x,256);
    assert(x[128]>.1f);fx_exchange_collect(e);assert(freed==0);c=next;
    next.stage[0].parameter[0]=.3f;
    p=fx_program_prepare_delta(&next,48000,&backend,&c);assert(p&&made==2);
    assert(fx_exchange_publish(e,p)==FX_OK);memset(x,0,sizeof x);block(e,x,1024);
    fx_exchange_collect(e);assert(freed==1&&fx_exchange_ready_audio(e));
    fx_exchange_destroy(e);assert(freed==made);
    /* EQ state survives coefficient edits: output matches the smoothing DSP exactly. */
    memset(&c,0,sizeof c);c.count=1;c.stage[0].kind=FX_STAGE_EQ;fx_eq_default(&c.stage[0].eq);
    c.stage[0].eq.band[0].gain_db=12;struct fx_eq reference;assert(fx_eq_init(&reference,48000,&c.stage[0].eq));
    e=fx_exchange_create();assert(fx_exchange_seed(e,fx_program_prepare(&c,48000,NULL)));
    float ref[512];for(unsigned i=0;i<512;++i)x[i]=ref[i]=(float)sin(i*.06);
    block(e,x,256);fx_eq_process(&reference,ref,256);
    next=c;next.stage[0].eq.band[0].gain_db=-6;assert(fx_eq_update(&reference,&next.stage[0].eq));
    assert(fx_exchange_publish(e,fx_program_prepare_delta(&next,48000,NULL,&c))==FX_OK);
    for(unsigned i=0;i<512;++i)x[i]=ref[i]=(float)cos(i*.07);
    block(e,x,256);fx_eq_process(&reference,ref,256);
    for(unsigned i=0;i<512;++i)assert(fabsf(x[i]-ref[i])<1e-6f);
    fx_exchange_collect(e);fx_exchange_destroy(e);
    puts("PASS FX continuity: EQ edits preserve delay tails and exact filter history; only changed native slot is created, crossfaded and retired off audio");
    return 0;
}
