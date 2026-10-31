/* Actual sample-processing tests, independent of native GUI/engine routing. */
#include "src/fx_chain.h"
#include <assert.h>
#include <math.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define PI 3.14159265358979323846264338327950288
static unsigned created,destroyed,fail_at;
static _Thread_local unsigned audio_thread;
static void *make(void *user,uint32_t id,double rate,const float *params)
{
    (void)user;(void)rate;(void)params;assert(!audio_thread);
    if(fail_at && created+1==fail_at)return NULL;
    float *n=malloc(sizeof(*n));assert(n);*n=id==1?.5f:2.f;++created;return n;
}
static void end(void *user,void *p){(void)user;assert(!audio_thread);++destroyed;free(p);}
static void multiply(void *user,void *p,float *data,size_t n)
{(void)user;assert(audio_thread);for(size_t i=0;i<n*2;++i)data[i]*=*(float*)p;}
static struct fx_backend backend={NULL,make,end,multiply};
static void eq_check(void)
{
    struct fx_eq_config c;fx_eq_default(&c);struct fx_eq e;
    assert(fx_eq_init(&e,48000,&c));float data[8192];
    for(unsigned i=0;i<8192;++i)data[i]=(float)sin((double)i);
    float original[8192];memcpy(original,data,sizeof(data));
    fx_eq_process(&e,data,4096);assert(!memcmp(data,original,sizeof(data)));
    c.count=1;c.band[0]=(struct fx_eq_band){1000,6,1,FX_EQ_BELL,1};
    assert(fabs(fx_eq_response_db(&c,48000,1000)-6)<1e-8);
    assert(fx_eq_init(&e,48000,&c));double in=0,out=0;
    for(unsigned block=0;block<12;++block){
        for(unsigned i=0;i<4096;++i){data[i*2]=.1f*(float)sin(2*PI*1000*(block*4096+i)/48000);data[i*2+1]=0;}
        memcpy(original,data,sizeof(data));fx_eq_process(&e,data,4096);
        for(unsigned i=0;i<4096;++i){assert(data[i*2+1]==0);if(block>4){in+=original[i*2]*original[i*2];out+=data[i*2]*data[i*2];}}
    }
    assert(fabs(10*log10(out/in)-fx_eq_response_db(&c,48000,1000))<.001);
    c.bypass=1;c.output_db=9;assert(fx_eq_update(&e,&c));
    fx_eq_process(&e,data,4096);memcpy(data,original,sizeof(data));fx_eq_process(&e,data,4096);
    assert(!memcmp(data,original,sizeof(data)));assert(fx_eq_response_db(&c,48000,1000)==0);
    struct fx_eq_config saved=c;c.count=FX_EQ_MAX_BANDS+1;assert(!fx_eq_update(&e,&c));c=saved;c.band[0].hz=NAN;assert(!fx_eq_init(&e,48000,&c));
    for(unsigned type=0;type<=FX_EQ_LOW_PASS;++type)for(unsigned r=0;r<3;++r){
        const double rates[]={44100,48000,96000};fx_eq_default(&c);c.count=1;
        c.band[0]=(struct fx_eq_band){1000,12,.70710678,type,1};assert(fx_eq_init(&e,rates[r],&c));
        for(unsigned i=0;i<8192;++i)data[i]=i<2?.5f:0;
        fx_eq_process(&e,data,4096);for(unsigned i=0;i<8192;++i)assert(isfinite(data[i]));
        if(type==FX_EQ_LOW_PASS)assert(fx_eq_response_db(&c,rates[r],10000)<-30);
        if(type==FX_EQ_HIGH_PASS)assert(fx_eq_response_db(&c,rates[r],100)<-35);
        if(type==FX_EQ_LOW_SHELF)assert(fx_eq_response_db(&c,rates[r],20)>11.9);
        if(type==FX_EQ_HIGH_SHELF)assert(fx_eq_response_db(&c,rates[r],15000)>11.9);
    }
    /* Boundary coefficients remain finite and stable, including low-frequency
     * high-Q changes, disabled points and opposite filter types. */
    for(unsigned type=0;type<=FX_EQ_LOW_PASS;++type)for(unsigned side=0;side<2;++side){
        fx_eq_default(&c);c.count=1;c.band[0]=(struct fx_eq_band){side?20000:20,side?18:-18,side?16:.2,type,1};
        assert(fx_eq_init(&e,48000,&c));
        for(unsigned b=0;b<20;++b){if(b==5){c.band[0].enabled=0;assert(fx_eq_update(&e,&c));}
            if(b==10){c.band[0].enabled=1;c.band[0].type=(type+2)%5;assert(fx_eq_update(&e,&c));}
            memset(data,0,sizeof(data));if(b==0)data[0]=data[1]=.1f;
            fx_eq_process(&e,data,4096);for(unsigned i=0;i<8192;++i)assert(isfinite(data[i])&&fabsf(data[i])<100);
        }
    }
    puts("PASS EQ: real stereo DSP, curve/audio agreement, shelves/pass filters, bypass, smoothing and boundaries");
}
static void policy_check(void)
{
    struct fx_stage s={.kind=FX_STAGE_NATIVE,.native_id=2};struct fx_chain_config c={0};
    for(unsigned i=0;i<FX_CHAIN_MAX;++i)assert(fx_chain_add(&c,&s,48000)==FX_OK);
    c.stage[0].bypass=1;struct fx_chain_config copy=c;
    assert(fx_chain_add(&c,&s,48000)==FX_FULL&&!memcmp(&c,&copy,sizeof(c)));
    c.count=FX_CHAIN_MAX+1;assert(!fx_program_prepare(&c,48000,&backend));c=copy;
    s.parameter[4]=NAN;assert(fx_chain_replace(&c,0,&s,48000)==FX_INVALID&&!memcmp(&c,&copy,sizeof(c)));s.parameter[4]=0;
    s.parameter[15]=NAN;assert(fx_chain_replace(&c,0,&s,48000)==FX_INVALID&&!memcmp(&c,&copy,sizeof(c)));s.parameter[15]=0;
    s.parameter[13]=.75f;s.parameter[14]=.25f;s.parameter[15]=1;
    assert(fx_chain_replace(&c,2,&s,48000)==FX_OK&&c.stage[2].parameter[13]==.75f&&c.stage[2].parameter[14]==.25f&&c.stage[2].parameter[15]==1);
    assert(fx_chain_remove(&c,2)==FX_OK&&c.count==FX_CHAIN_MAX-1);assert(fx_chain_add(&c,&s,48000)==FX_OK);
    assert(fx_chain_move(&c,0,FX_CHAIN_MAX-1)==FX_OK&&c.stage[FX_CHAIN_MAX-1].bypass==1);
    assert(fx_chain_replace(&c,2,&c.stage[FX_CHAIN_MAX-1],48000)==FX_OK);
    struct fx_program *p=fx_program_prepare(&c,48000,&backend);assert(p);
    float data[]={.1f,.2f};audio_thread=1;fx_program_process(p,data,1);audio_thread=0;
    assert(fabs(data[0]-.8f)<1e-6&&fabs(data[1]-1.6f)<1e-6);fx_program_destroy(p);
    const unsigned before=destroyed;fail_at=created+3;
    assert(!fx_program_prepare(&c,48000,&backend));assert(destroyed==before+2);fail_at=0;
    assert(created==destroyed);
    puts("PASS chain: five occupied slots, bypass counts, reject sixth/invalid restore, order, partial failure cleanup");
}
/* A gain-only test cannot detect reversed stage order. These deterministic
 * backend fixtures deliberately do not commute; they are not native FX. */
static unsigned visited[FX_CHAIN_MAX],visits;
static void *order_make(void*u,uint32_t id,double rate,const float*p){
    (void)u;(void)rate;(void)p;unsigned*n=malloc(sizeof(*n));assert(n);*n=id;return n;
}
static void order_end(void*u,void*p){(void)u;free(p);}
static void order_process(void*u,void*p,float*pcm,size_t frames){
    (void)u;unsigned id=*(unsigned*)p;assert(visits<FX_CHAIN_MAX);visited[visits++]=id;
    for(size_t i=0;i<frames*2;++i){
        if(id==1)pcm[i]*=2;
        else if(id==2)pcm[i]=fmaxf(-.5f,fminf(.5f,pcm[i]));
    }
}
static void order_check(void){
    struct fx_chain_config c={0};
    for(unsigned i=0;i<FX_CHAIN_MAX;++i){struct fx_stage s={.kind=FX_STAGE_NATIVE,.native_id=i+1};s.parameter[15]=(float)i/(FX_CHAIN_MAX-1);assert(fx_chain_add(&c,&s,48000)==FX_OK);}
    const struct fx_backend b={NULL,order_make,order_end,order_process};
    struct fx_program*p=fx_program_prepare(&c,48000,&b);assert(p);
    float pcm[]={.4f,-.4f};visits=0;fx_program_process(p,pcm,1);fx_program_destroy(p);
    assert(visits==5&&pcm[0]==.5f&&pcm[1]==-.5f);
    for(unsigned i=0;i<FX_CHAIN_MAX;++i)assert(visited[i]==i+1);
    struct fx_stage saved=c.stage[0];assert(fx_chain_move(&c,0,4)==FX_OK&&!memcmp(&c.stage[4],&saved,sizeof(saved)));
    p=fx_program_prepare(&c,48000,&b);assert(p);pcm[0]=.4f;pcm[1]=-.4f;visits=0;fx_program_process(p,pcm,1);fx_program_destroy(p);
    assert(visits==5&&pcm[0]==.8f&&pcm[1]==-.8f);
    for(unsigned i=0;i<4;++i)assert(visited[i]==i+2);
    assert(visited[4]==1);
    puts("PASS audio order: all five stages execute top to bottom; moving gain after clipping changes rendered PCM, preserving stereo and slot settings");
}
struct race {struct fx_exchange *exchange;_Atomic unsigned done;};
static void *audio(void *user)
{
    struct race *r=user;audio_thread=1;float data[1024];
    while(!atomic_load_explicit(&r->done,memory_order_acquire)){
        for(unsigned i=0;i<1024;++i)data[i]=.1f;
        fx_exchange_process(r->exchange,data,512);
        for(unsigned i=0;i<1024;++i)assert(isfinite(data[i])&&data[i]>=.049f&&data[i]<=.201f);
    }
    audio_thread=0;return NULL;
}
static void exchange_check(void)
{
    struct fx_chain_config c={.count=1,.stage={{.kind=FX_STAGE_NATIVE,.native_id=2}}};
    struct fx_exchange *e=fx_exchange_create();assert(e);
    struct fx_program *p=fx_program_prepare(&c,48000,&backend),*q=fx_program_prepare(&c,48000,&backend);assert(p&&q);
    assert(fx_exchange_publish(e,p)==FX_OK);assert(fx_exchange_publish(e,q)==FX_BUSY);fx_program_destroy(q);
    float data[1024];for(unsigned i=0;i<1024;++i)data[i]=.1f;
    audio_thread=1;fx_exchange_process(e,data,512);audio_thread=0;
    for(unsigned i=1;i<240;++i)assert(fabsf(data[i*2]-data[(i-1)*2])<.001f);
    assert(fabsf(data[1023]-.2f)<1e-6f);
    struct race r={.exchange=e};atomic_init(&r.done,0);pthread_t t;assert(!pthread_create(&t,NULL,audio,&r));
    for(unsigned i=0;i<2000;++i){
        c.stage[0].native_id=i%2+1;p=fx_program_prepare(&c,48000,&backend);assert(p);
        while(fx_exchange_publish(e,p)==FX_BUSY)fx_exchange_collect(e);
        fx_exchange_collect(e);
    }
    atomic_store_explicit(&r.done,1,memory_order_release);assert(!pthread_join(t,NULL));
    fx_exchange_destroy(e);assert(created==destroyed);
    puts("PASS exchange: 2000 concurrent swaps, 5 ms crossfade, busy handoff and off-audio reclamation");
}
int main(void){eq_check();policy_check();order_check();exchange_check();puts("PASS FX core");return 0;}
