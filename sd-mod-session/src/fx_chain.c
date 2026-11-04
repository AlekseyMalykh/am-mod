#include "fx_chain.h"
#include <math.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
struct fx_instance { unsigned kind,bypass; struct fx_eq eq; void *native; };
struct fx_program { unsigned count; double rate; struct fx_backend backend; struct fx_instance stage[FX_CHAIN_MAX]; };
struct fx_exchange {
    _Atomic(struct fx_program*) pending,retired;
    struct fx_program *active,*fading;
    unsigned fade_left,fade_total;
    float old_buffer[512];
};
static int stage_valid(const struct fx_stage *s,double rate)
{
    if(!s||s->bypass>1)return 0;
    if(s->kind==FX_STAGE_EQ)return fx_eq_valid(&s->eq,rate);
    if(s->kind!=FX_STAGE_NATIVE)return 0;
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
    p->rate=rate;if(backend)p->backend=*backend;
    for(unsigned i=0;i<c->count;++i){const struct fx_stage *s=&c->stage[i];struct fx_instance *t=&p->stage[i];
        t->kind=s->kind;t->bypass=s->bypass;p->count=i+1;
        if(s->kind==FX_STAGE_EQ){struct fx_eq_config config=s->eq;config.bypass|=s->bypass;
            if(!fx_eq_init(&t->eq,rate,&config))goto fail;}
        else {if(!backend||!backend->create||!backend->destroy||!backend->process)goto fail;
            t->native=backend->create(backend->user,s->native_id,rate,s->parameter);if(!t->native)goto fail;}
    }
    return p;
fail:fx_program_destroy(p);return NULL;
}
void fx_program_destroy(struct fx_program *p)
{
    if(!p)return;
    for(unsigned i=0;i<p->count;++i)if(p->stage[i].native)p->backend.destroy(p->backend.user,p->stage[i].native);
    free(p);
}
void fx_program_process(struct fx_program *p,float *samples,size_t frames)
{
    if(!p||!samples)return;
    for(unsigned i=0;i<p->count;++i){struct fx_instance *s=&p->stage[i];
        if(s->kind==FX_STAGE_EQ)fx_eq_process(&s->eq,samples,frames);
        else if(!s->bypass)p->backend.process(p->backend.user,s->native,samples,frames);
    }
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
void fx_exchange_process(struct fx_exchange *e,float *data,size_t frames)
{
    if(!e||!data)return;
    /* A full retirement slot delays the next swap; it never blocks audio. */
    if(!e->fade_left&&!atomic_load_explicit(&e->retired,memory_order_acquire)){
        struct fx_program *p=atomic_exchange_explicit(&e->pending,NULL,memory_order_acquire);
        if(p){e->fading=e->active;e->active=p;
            e->fade_total=e->fade_left=(unsigned)(p->rate*.005);}
    }
    size_t position=0;
    while(position<frames){
        size_t n=frames-position;if(n>256)n=256;
        if(e->fade_left && n>e->fade_left)n=e->fade_left;
        float *out=data+position*2;
        if(e->fade_left){
            memcpy(e->old_buffer,out,n*2*sizeof(float));
            fx_program_process(e->fading,e->old_buffer,n);fx_program_process(e->active,out,n);
            for(size_t i=0;i<n;++i){
                double wet=1.-(double)(e->fade_left-1)/e->fade_total;
                for(unsigned c=0;c<2;++c){size_t k=i*2+c;out[k]=(float)(e->old_buffer[k]+wet*(out[k]-e->old_buffer[k]));}
                --e->fade_left;
            }
            if(!e->fade_left){
                atomic_store_explicit(&e->retired,e->fading,memory_order_release);e->fading=NULL;
            }
        }else fx_program_process(e->active,out,n);
        position+=n;
    }
}
void fx_exchange_destroy(struct fx_exchange *e)
{
    if(!e)return;
    fx_program_destroy(atomic_load_explicit(&e->pending,memory_order_relaxed));
    fx_program_destroy(atomic_load_explicit(&e->retired,memory_order_relaxed));
    fx_program_destroy(e->fading);fx_program_destroy(e->active);free(e);
}
