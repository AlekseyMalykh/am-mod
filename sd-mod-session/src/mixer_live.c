#include "mixer_live.h"
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#if ATOMIC_POINTER_LOCK_FREE != 2
#error Live Mixer requires lock-free packet publication
#endif
struct live_update {struct mixer_bus_update*track,*master;};
struct mixer_live {
    struct mixer_route_native*track;
    struct mixer_buses*master;
    struct mixer_meters*meters;
    struct mixer_live_ops ops;
    struct fx_backend backend;
    double rate;unsigned capacity;
    uint64_t epoch;
    uint64_t submitted_revision; /* single control producer only */
    void*engine,*program;
    enum mixer_route_result block_result;
    _Atomic(struct live_update*) pending,retired;
};
static _Thread_local struct mixer_live*live_scope;
static void split(const struct mixer_project*p,struct mixer_project*t,struct mixer_project*m)
{
    *t=*p;mixer_defaults(m);m->master=p->master;t->master=m->track[0];
    memcpy(t->master.name,"Master",7); /* master is applied once, after engine mix */
}
static void destroy_update(struct live_update*u)
{
    if(u){mixer_buses_update_destroy(u->track);mixer_buses_update_destroy(u->master);free(u);}
}
static void commit(void*user)
{
    struct mixer_live*s=user;struct mixer_buses*t=mixer_route_buses(s->track);
    if(atomic_load_explicit(&s->retired,memory_order_acquire)||!mixer_buses_ready_audio(t)||!mixer_buses_ready_audio(s->master))return;
    struct live_update*u=atomic_exchange_explicit(&s->pending,NULL,memory_order_acquire);if(!u)return;
    if(mixer_buses_publish(t,u->track)==MIXER_BUS_OK)u->track=NULL;
    if(mixer_buses_publish(s->master,u->master)==MIXER_BUS_OK)u->master=NULL;
    atomic_store_explicit(&s->retired,u,memory_order_release);
}
struct mixer_live*mixer_live_prepare(const struct mixer_project*p,uint64_t part,double rate,unsigned capacity,uint64_t epoch,
    void*engine,void*program,const struct mixer_live_ops*ops,const struct fx_backend*backend,enum mixer_bus_result*error)
{
    if(error)*error=MIXER_BUS_INVALID;
    if(!p||!epoch||!engine||!program||!ops||!ops->engine||!ops->final_output||!mixer_project_valid(p))return NULL;
    struct mixer_live*s=calloc(1,sizeof(*s));if(!s){if(error)*error=MIXER_BUS_RESOURCE;return NULL;}
    atomic_init(&s->pending,NULL);atomic_init(&s->retired,NULL);
    struct mixer_project t,m;split(p,&t,&m);s->ops=*ops;s->rate=rate;s->capacity=capacity;s->epoch=epoch;
    s->engine=engine;s->program=program;if(backend)s->backend=*backend;
    s->track=mixer_route_prepare(&t,part,rate,capacity,backend,&ops->route,error);
    if(!s->track)goto fail;
    s->master=mixer_buses_prepare(&m,0,rate,capacity,backend,error);if(!s->master)goto fail;
    s->meters=mixer_meters_create(rate);if(!s->meters){if(error)*error=MIXER_BUS_RESOURCE;goto fail;}
    mixer_route_before_block(s->track,commit,s);return s;
fail:mixer_live_destroy(s);return NULL;
}
void mixer_live_destroy(struct mixer_live*s)
{
    if(!s)return;
    destroy_update(atomic_load(&s->pending));destroy_update(atomic_load(&s->retired));
    mixer_route_destroy(s->track);mixer_buses_destroy(s->master);mixer_meters_destroy(s->meters);free(s);
}
enum mixer_bus_result mixer_live_publish(struct mixer_live*s,const struct mixer_project*p,uint64_t part,uint64_t revision)
{
    if(!s||!p||!revision||!mixer_project_valid(p)||revision<=s->submitted_revision)return MIXER_BUS_INVALID;
    if(atomic_load_explicit(&s->pending,memory_order_relaxed))return MIXER_BUS_STATE;
    struct mixer_project t,m;split(p,&t,&m);enum mixer_bus_result error;
    struct live_update*u=calloc(1,sizeof(*u));if(!u)return MIXER_BUS_RESOURCE;
    u->track=mixer_buses_update_prepare(&t,part,s->rate,revision,&s->backend,&error);if(!u->track)goto fail;
    u->master=mixer_buses_update_prepare(&m,0,s->rate,revision,&s->backend,&error);if(!u->master)goto fail;
    struct live_update*expected=NULL;
    if(atomic_compare_exchange_strong_explicit(&s->pending,&expected,u,memory_order_release,memory_order_relaxed)){
        s->submitted_revision=revision;return MIXER_BUS_OK;
    }
    error=MIXER_BUS_STATE;
fail:destroy_update(u);return error;
}
void mixer_live_collect(struct mixer_live*s)
{
    if(s){destroy_update(atomic_exchange_explicit(&s->retired,NULL,memory_order_acquire));
        mixer_buses_collect(mixer_route_buses(s->track));mixer_buses_collect(s->master);}
}
uint64_t mixer_live_applied_revision(const struct mixer_live*s)
{
    if(!s)return 0;
    uint64_t t=mixer_buses_applied_revision(mixer_route_buses(s->track)),m=mixer_buses_applied_revision(s->master);
    return t<m?t:m;
}
int mixer_live_take_meter(struct mixer_live*s,struct mixer_meter_snapshot*out){return s&&mixer_meters_take(s->meters,s->epoch,out);}
void mixer_live_program(const struct mixer_route_ops*ops,void*p,void*clock,unsigned frames)
{
    struct mixer_live*s=live_scope;
    if(!s||s->program!=p){ops->render(p,clock,frames);return;}
    s->block_result=mixer_route_render(s->track,p,clock,frames);
}
static void*pointer(void*p,unsigned offset){void*v;memcpy(&v,(char*)p+offset,sizeof(v));return v;}
static unsigned integer(void*p,unsigned offset){unsigned v;memcpy(&v,(char*)p+offset,4);return v;}
int mixer_live_render(struct mixer_live*s,void*engine,void*clock,unsigned frames)
{
    if(!s||!engine)return 0;
    if(!frames||frames>s->capacity||live_scope||s->engine!=engine){
        struct mixer_live*previous=live_scope;live_scope=NULL;
        s->ops.engine(engine,clock,frames);live_scope=previous;return 0;
    }
    s->block_result=MIXER_ROUTE_PASSTHROUGH;live_scope=s;s->ops.engine(engine,clock,frames);live_scope=NULL;
    struct mixer_meter_input taps[MIXER_METER_BUSES]={0};
    if(s->block_result==MIXER_ROUTE_APPLIED)for(unsigned bus=0;bus<9;++bus){struct mixer_pcm_view v;
        if(mixer_route_view(s->track,bus,&v)&&v.frames==frames)taps[bus]=(struct mixer_meter_input){{v.channel[0],v.channel[1]},2,0};}
    void*out=s->ops.final_output(engine);int success=0;
    if(out&&integer(out,0)==2&&integer(out,4)>=frames){float**pcm=pointer(out,16);unsigned clear=*((unsigned char*)out+0x128);
        if(pcm&&pcm[0]&&pcm[1]&&clear<=1&&mixer_buses_begin(s->master,frames)==MIXER_BUS_OK){
            mixer_buses_accumulate(s->master,0,pcm[0],pcm[1],2,clear);
            if(mixer_buses_finish(s->master,NULL,0)==MIXER_BUS_OK){
                struct mixer_pcm_view v;if(mixer_buses_view(s->master,9,&v)){unsigned silent=1;
                    for(unsigned c=0;c<2;++c){memcpy(pcm[c],v.channel[c],frames*sizeof(float));
                        for(unsigned i=0;i<frames;++i)if(pcm[c][i]!=0)silent=0;}
                    *((unsigned char*)out+0x128)=(unsigned char)silent;
                    taps[9]=(struct mixer_meter_input){{pcm[0],pcm[1]},2,silent};success=1;
                }
            }
        }
    }
    mixer_meters_push(s->meters,s->epoch,frames,taps);return success;
}
