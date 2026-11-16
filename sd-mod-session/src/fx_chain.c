#include "fx_chain.h"
#include "fx_own.h"
#include <math.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
struct sidechain {
    unsigned source;
    float threshold,slope,attack,release,mix,gain;
};
struct fx_instance { unsigned kind,bypass; struct fx_eq eq; struct sidechain duck; void *native; void *own; };
struct fx_program { unsigned delta,reuse; struct fx_chain_config config; unsigned count; double rate; struct fx_backend backend; struct fx_instance stage[FX_CHAIN_MAX]; };
struct fx_exchange {
    _Atomic(struct fx_program*) pending,retired;
    struct fx_program *active,*fading;
    unsigned fade_left,fade_total,stage_fade;
    float old_buffer[512];
};
float fx_sidechain_value(unsigned index,float value)
{
    switch(index){
    case 0:return fminf(8,floorf(value*9));
    case 1:return -60+60*value;
    case 2:return 1+19*value;
    case 3:return .2f*powf(500,value);
    case 4:return 10*powf(100,value);
    default:return 100*value;
    }
}
static void sidechain_init(struct sidechain*d,const float*p,double rate)
{
    d->source=(unsigned)fx_sidechain_value(0,p[0]);
    d->threshold=powf(10,fx_sidechain_value(1,p[1])*.05f);
    d->slope=1/fx_sidechain_value(2,p[2])-1;
    d->attack=expf(-1000.f/((float)rate*fx_sidechain_value(3,p[3])));
    d->release=expf(-1000.f/((float)rate*fx_sidechain_value(4,p[4])));
    d->mix=p[5];d->gain=1;
}
static float sidechain_process(struct sidechain*d,float*pcm,size_t frames,const float*key,int monitored)
{
    float minimum=1;
    for(size_t i=0;i<frames;++i){
        float desired=key&&key[i]>d->threshold?powf(key[i]/d->threshold,d->slope):1;
        float smoothing=desired<d->gain?d->attack:d->release;
        d->gain=desired+smoothing*(d->gain-desired);
        float gain=1+d->mix*(d->gain-1);
        pcm[i*2]*=gain;pcm[i*2+1]*=gain;
        if(monitored&&gain<minimum)minimum=gain;
    }
    return minimum;
}
static int stage_valid(const struct fx_stage *s,double rate)
{
    if(!s||s->bypass>1)return 0;
    if(s->kind==FX_STAGE_EQ)return fx_eq_valid(&s->eq,rate);
    if(s->kind!=FX_STAGE_NATIVE&&(s->kind!=FX_STAGE_SIDECHAIN||s->native_id!=FX_SIDECHAIN_ID))return 0;
    for(unsigned i=0;i<FX_CHAIN_PARAMETERS;++i)
        if(!isfinite(s->parameter[i])||s->parameter[i]<0||s->parameter[i]>1)return 0;
    return 1;
}
int fx_chain_valid(const struct fx_chain_config *c,double rate)
{
    if(!c||c->count>FX_CHAIN_MAX||!isfinite(rate)||rate<8000||rate>192000)return 0;
    for(unsigned i=0;i<c->count;++i)if(!stage_valid(&c->stage[i],rate))return 0;
    return 1;
}
enum fx_result fx_chain_add(struct fx_chain_config *c,const struct fx_stage *s,double rate)
{
    if(!fx_chain_valid(c,rate)||!stage_valid(s,rate))return FX_INVALID;
    if(c->count==FX_CHAIN_MAX)return FX_FULL;
    /* Copy first: s may alias a slot within c. */
    struct fx_stage copy=*s;c->stage[c->count++]=copy;return FX_OK;
}
enum fx_result fx_chain_replace(struct fx_chain_config *c,unsigned n,const struct fx_stage *s,double rate)
{
    if(!fx_chain_valid(c,rate)||n>=c->count||!stage_valid(s,rate))return FX_INVALID;
    c->stage[n]=*s;return FX_OK;
}
enum fx_result fx_chain_remove(struct fx_chain_config *c,unsigned n)
{
    if(!c||c->count>FX_CHAIN_MAX||n>=c->count)return FX_INVALID;
    memmove(c->stage+n,c->stage+n+1,(c->count-n-1)*sizeof(*c->stage));
    memset(&c->stage[--c->count],0,sizeof(*c->stage));return FX_OK;
}
enum fx_result fx_chain_move(struct fx_chain_config *c,unsigned from,unsigned to)
{
    if(!c||c->count>FX_CHAIN_MAX||from>=c->count||to>=c->count)return FX_INVALID;
    struct fx_stage copy=c->stage[from];
    if(from<to)memmove(c->stage+from,c->stage+from+1,(to-from)*sizeof(*c->stage));
    else if(from>to)memmove(c->stage+to+1,c->stage+to,(from-to)*sizeof(*c->stage));
    c->stage[to]=copy;return FX_OK;
}
struct fx_program *fx_program_prepare(const struct fx_chain_config *c,double rate,const struct fx_backend *backend)
{
    if(!fx_chain_valid(c,rate))return NULL;
    struct fx_program *p=calloc(1,sizeof(*p));if(!p)return NULL;
    p->config=*c;p->rate=rate;if(backend)p->backend=*backend;
    for(unsigned i=0;i<c->count;++i){const struct fx_stage *s=&c->stage[i];struct fx_instance *t=&p->stage[i];
        t->kind=s->kind;t->bypass=s->bypass;p->count=i+1;
        if(s->kind==FX_STAGE_EQ){struct fx_eq_config config=s->eq;config.bypass|=s->bypass;
            if(!fx_eq_init(&t->eq,rate,&config))goto fail;}
        else if(s->kind==FX_STAGE_SIDECHAIN)sidechain_init(&t->duck,s->parameter,rate);
        else if(fx_own_is(s->native_id)){t->own=fx_own_create(s->native_id,rate,s->parameter);if(!t->own)goto fail;}
        else {if(!backend||!backend->create||!backend->destroy||!backend->process)goto fail;
            t->native=backend->create(backend->user,s->native_id,rate,s->parameter);if(!t->native)goto fail;}
    }
    return p;
fail:fx_program_destroy(p);return NULL;
}
/* A parameter packet prepares only changed processors. Unchanged slots and
 * smoothly updated EQ/own DSP retain their live state on the audio owner. */
struct fx_program *fx_program_prepare_delta(const struct fx_chain_config*c,double rate,
    const struct fx_backend*backend,const struct fx_chain_config*previous)
{
    if(!previous||!fx_chain_valid(c,rate)||!fx_chain_valid(previous,rate)||c->count!=previous->count)
        return fx_program_prepare(c,rate,backend);
    for(unsigned i=0;i<c->count;++i)
        if(c->stage[i].kind!=previous->stage[i].kind||c->stage[i].native_id!=previous->stage[i].native_id)
            return fx_program_prepare(c,rate,backend);
    struct fx_program*p=calloc(1,sizeof(*p));if(!p)return NULL;
    p->delta=1;p->config=*c;p->rate=rate;p->count=c->count;if(backend)p->backend=*backend;
    for(unsigned i=0;i<c->count;++i){
        const struct fx_stage*a=previous->stage+i,*b=c->stage+i;
        if(b->kind==FX_STAGE_EQ||(a->bypass==b->bypass&&
           (!memcmp(a->parameter,b->parameter,sizeof(a->parameter))||
            (b->kind==FX_STAGE_NATIVE&&fx_own_can_update(b->native_id,a->parameter,b->parameter))))){
            p->reuse|=1u<<i;
        }else{
            struct fx_chain_config one={.count=1,.stage={*b}};
            struct fx_program*prepared=fx_program_prepare(&one,rate,backend);
            if(!prepared){fx_program_destroy(p);return NULL;}
            p->stage[i]=prepared->stage[0];prepared->count=0;fx_program_destroy(prepared);
        }
    }
    return p;
}
static int adopt_delta(struct fx_program*p,struct fx_program*old)
{
    int fading=0;
    for(unsigned i=0;i<p->count;++i){
        if(!(p->reuse&(1u<<i))){fading=1;continue;}
        p->stage[i]=old->stage[i];memset(old->stage+i,0,sizeof(old->stage[i]));
        const struct fx_stage*s=p->config.stage+i;
        if(s->kind==FX_STAGE_EQ){struct fx_eq_config c=s->eq;c.bypass|=s->bypass;fx_eq_update(&p->stage[i].eq,&c);}
        else if(p->stage[i].own&&memcmp(s->parameter,old->config.stage[i].parameter,sizeof(s->parameter)))
            fx_own_update(p->stage[i].own,s->parameter);
    }
    p->delta=0;return fading;
}
void fx_program_destroy(struct fx_program *p)
{
    if(!p)return;
    for(unsigned i=0;i<p->count;++i){if(p->stage[i].own)fx_own_destroy(p->stage[i].own);else if(p->stage[i].native)p->backend.destroy(p->backend.user,p->stage[i].native);}
    free(p);
}
static float instance_process(struct fx_program*p,struct fx_instance*s,float*samples,size_t frames,
                              const float*const*keys,size_t offset,int monitored)
{
    if(!s->kind)return 1;
    if(s->kind==FX_STAGE_EQ){fx_eq_process(&s->eq,samples,frames);return 1;}
    if(s->bypass)return 1;
    if(s->kind==FX_STAGE_SIDECHAIN){unsigned source=s->duck.source;
        const float*key=keys&&source?keys[source-1]:NULL;
        return sidechain_process(&s->duck,samples,frames,key?key+offset:NULL,monitored);
    }
    if(s->own)return fx_own_process(s->own,samples,frames);
    p->backend.process(p->backend.user,s->native,samples,frames);return 1;
}
static void program_process(struct fx_program*p,float*samples,size_t frames,unsigned slot,fx_stage_tap tap,void*user,
                            const float*const*keys,size_t offset)
{
    if(!p||!samples)return;
    for(unsigned i=0;i<p->count;++i){int monitored=tap&&i==slot;
        if(monitored)tap(user,samples,frames,0,1);
        float gain=instance_process(p,p->stage+i,samples,frames,keys,offset,monitored);
        if(monitored)tap(user,samples,frames,1,gain);
    }
}
/* Crossfade the changed slot from the same input, then run downstream DSP
 * once. Delay tails/oscillator phases in every unaffected slot stay alive. */
static void transition_stages(struct fx_exchange*e,float*out,size_t n,unsigned slot,fx_stage_tap tap,void*user,
                              const float*const*keys,size_t offset)
{
    for(unsigned i=0;i<e->active->count;++i){int monitored=tap&&i==slot;
        if(monitored)tap(user,out,n,0,1);
        if(e->fading->stage[i].kind){
            memcpy(e->old_buffer,out,n*2*sizeof(float));
            instance_process(e->fading,e->fading->stage+i,e->old_buffer,n,keys,offset,0);
        }
        float gain=instance_process(e->active,e->active->stage+i,out,n,keys,offset,monitored);
        if(e->fading->stage[i].kind)for(size_t f=0;f<n;++f){
            float wet=1.f-(float)(e->fade_left-f-1)/e->fade_total;
            for(unsigned c=0;c<2;++c){size_t k=f*2+c;out[k]=e->old_buffer[k]+wet*(out[k]-e->old_buffer[k]);}
        }
        if(monitored)tap(user,out,n,1,gain);
    }
}
void fx_program_process(struct fx_program *p,float *samples,size_t frames)
{
    program_process(p,samples,frames,0,NULL,NULL,NULL,0);
}
struct fx_exchange *fx_exchange_create(void)
{
    struct fx_exchange *e=calloc(1,sizeof(*e));if(!e)return NULL;
    atomic_init(&e->pending,NULL);atomic_init(&e->retired,NULL);
    if(!atomic_is_lock_free(&e->pending)||!atomic_is_lock_free(&e->retired)){free(e);return NULL;}
    return e;
}
enum fx_result fx_exchange_publish(struct fx_exchange *e,struct fx_program *p)
{
    if(!e||!p)return FX_INVALID;
    struct fx_program *expected=NULL;
    return atomic_compare_exchange_strong_explicit(&e->pending,&expected,p,memory_order_release,memory_order_relaxed)?FX_OK:FX_BUSY;
}
int fx_exchange_seed(struct fx_exchange*e,struct fx_program*p)
{
    if(!e||e->active||e->fading||e->fade_left||atomic_load(&e->pending)||atomic_load(&e->retired))return 0;
    e->active=p;return 1;
}
int fx_exchange_ready_audio(const struct fx_exchange*e)
{
    return e&&!e->fade_left&&!atomic_load_explicit(&e->pending,memory_order_acquire)&&
        !atomic_load_explicit(&e->retired,memory_order_acquire);
}
int fx_exchange_transitioning_audio(const struct fx_exchange*e)
{
    return e&&(e->fade_left||atomic_load_explicit(&e->pending,memory_order_acquire));
}
void fx_exchange_collect(struct fx_exchange *e)
{
    if(e)fx_program_destroy(atomic_exchange_explicit(&e->retired,NULL,memory_order_acquire));
}
void fx_exchange_process_keyed(struct fx_exchange *e,float *data,size_t frames,unsigned slot,fx_stage_tap tap,void *user,
                               const float *const keys[FX_SIDECHAIN_KEYS])
{
    if(!e||!data)return;
    /* A full retirement slot delays the next swap; it never blocks audio. */
    if(!e->fade_left&&!atomic_load_explicit(&e->retired,memory_order_acquire)){
        struct fx_program *p=atomic_exchange_explicit(&e->pending,NULL,memory_order_acquire);
        if(p&&p->delta&&e->active){
            struct fx_program*old=e->active;
            int fading=adopt_delta(p,old);e->active=p;
            if(fading){e->fading=old;e->stage_fade=1;e->fade_total=e->fade_left=(unsigned)(p->rate*.02);}
            else atomic_store_explicit(&e->retired,old,memory_order_release);
        }else if(p){p->delta=0;e->fading=e->active;e->active=p;e->stage_fade=0;
            e->fade_total=e->fade_left=(unsigned)(p->rate*.005);}
    }
    size_t position=0;
    while(position<frames){
        size_t n=frames-position;if(n>256)n=256;
        if(e->fade_left && n>e->fade_left)n=e->fade_left;
        float *out=data+position*2;
        if(e->fade_left&&e->stage_fade){
            transition_stages(e,out,n,slot,tap,user,keys,position);
            e->fade_left-=(unsigned)n;
            if(!e->fade_left){atomic_store_explicit(&e->retired,e->fading,memory_order_release);e->fading=NULL;}
        }else if(e->fade_left){
            memcpy(e->old_buffer,out,n*2*sizeof(float));
            program_process(e->fading,e->old_buffer,n,0,NULL,NULL,keys,position);
            program_process(e->active,out,n,slot,tap,user,keys,position);
            for(size_t i=0;i<n;++i){
                double wet=1.-(double)(e->fade_left-1)/e->fade_total;
                for(unsigned c=0;c<2;++c){size_t k=i*2+c;out[k]=(float)(e->old_buffer[k]+wet*(out[k]-e->old_buffer[k]));}
                --e->fade_left;
            }
            if(!e->fade_left){
                atomic_store_explicit(&e->retired,e->fading,memory_order_release);e->fading=NULL;
            }
        }else program_process(e->active,out,n,slot,tap,user,keys,position);
        position+=n;
    }
}
void fx_exchange_process_tapped(struct fx_exchange *e,float *data,size_t frames,unsigned slot,fx_stage_tap tap,void *user)
{
    fx_exchange_process_keyed(e,data,frames,slot,tap,user,NULL);
}
void fx_exchange_process(struct fx_exchange *e,float *data,size_t frames)
{
    fx_exchange_process_tapped(e,data,frames,0,NULL,NULL);
}
void fx_exchange_destroy(struct fx_exchange *e)
{
    if(!e)return;
    fx_program_destroy(atomic_load_explicit(&e->pending,memory_order_relaxed));
    fx_program_destroy(atomic_load_explicit(&e->retired,memory_order_relaxed));
    fx_program_destroy(e->fading);fx_program_destroy(e->active);free(e);
}
