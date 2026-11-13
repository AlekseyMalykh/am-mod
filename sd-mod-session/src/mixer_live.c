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
    struct fx_chain_config track_fx[9],master_fx[9]; /* control owner only */
    void*engine,*program;
    enum mixer_route_result block_result;
    unsigned matched_program;
    _Atomic(struct live_update*) pending,retired;
};
static _Thread_local struct mixer_live*live_scope;
/* Lab/device diagnostic counters (relaxed, audio-safe). */
static _Atomic uint64_t diag_program_calls,diag_program_scoped,diag_program_matched,
    diag_program_other_thread,diag_program_other_object,diag_engine_blocks,diag_engine_matched;
static _Atomic unsigned diag_engine_active;
static _Atomic uintptr_t diag_seen[4];   /* distinct Program objects seen in scope */
void mixer_live_diag_objects(const struct mixer_live*s,uintptr_t out[5])
{
    out[0]=s?(uintptr_t)s->program:0;
    for(unsigned i=0;i<4;++i)out[1+i]=atomic_load_explicit(&diag_seen[i],memory_order_relaxed);
}
void mixer_live_diag(uint64_t out[7])
{
    out[0]=atomic_load_explicit(&diag_engine_blocks,memory_order_relaxed);
    out[1]=atomic_load_explicit(&diag_engine_matched,memory_order_relaxed);
    out[2]=atomic_load_explicit(&diag_program_calls,memory_order_relaxed);
    out[3]=atomic_load_explicit(&diag_program_scoped,memory_order_relaxed);
    out[4]=atomic_load_explicit(&diag_program_matched,memory_order_relaxed);
    out[5]=atomic_load_explicit(&diag_program_other_thread,memory_order_relaxed);
    out[6]=atomic_load_explicit(&diag_program_other_object,memory_order_relaxed);
}
static void split(const struct mixer_project*p,struct mixer_project*t,struct mixer_project*m)
{
    *t=*p;mixer_defaults(m);m->master=p->master;t->master=m->track[0];
    memcpy(t->master.name,"Master",7); /* master is applied once, after engine mix */
}
static void destroy_update(struct live_update*u)
{
    if(u){mixer_buses_update_destroy(u->track);mixer_buses_update_destroy(u->master);free(u);}
}
static void fx_configs(const struct mixer_project*p,struct fx_chain_config out[9])
{
    for(unsigned i=0;i<9;++i){const struct mixer_channel*c=i<8?p->track+i:&p->master;
        memset(out+i,0,sizeof(*out));if(!c->fx_bypass)out[i]=c->fx;}
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
    fx_configs(&t,s->track_fx);fx_configs(&m,s->master_fx);
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
    u->track=mixer_buses_update_prepare_delta(&t,part,s->rate,revision,&s->backend,s->track_fx,&error);if(!u->track)goto fail;
    u->master=mixer_buses_update_prepare_delta(&m,0,s->rate,revision,&s->backend,s->master_fx,&error);if(!u->master)goto fail;
    struct live_update*expected=NULL;
    if(atomic_compare_exchange_strong_explicit(&s->pending,&expected,u,memory_order_release,memory_order_relaxed)){
        fx_configs(&t,s->track_fx);fx_configs(&m,s->master_fx);s->submitted_revision=revision;return MIXER_BUS_OK;
    }
    error=MIXER_BUS_STATE;
fail:destroy_update(u);return error;
}
/* Double Stop (all sounds off): rebuild every FX program so feedback loops and tails restart from silence.
 * Control owner only; returns MIXER_BUS_STATE while another update is still pending (caller retries). */
enum mixer_bus_result mixer_live_flush_fx(struct mixer_live*s,const struct mixer_project*p,uint64_t part)
{
    if(!s||!p||!mixer_project_valid(p))return MIXER_BUS_INVALID;
    if(atomic_load_explicit(&s->pending,memory_order_relaxed))return MIXER_BUS_STATE;
    struct mixer_project t,m;split(p,&t,&m);enum mixer_bus_result error;
    struct live_update*u=calloc(1,sizeof(*u));if(!u)return MIXER_BUS_RESOURCE;
    uint64_t revision=s->submitted_revision?s->submitted_revision:1;
    u->track=mixer_buses_update_prepare_delta(&t,part,s->rate,revision,&s->backend,NULL,&error);if(!u->track)goto fail;
    u->master=mixer_buses_update_prepare_delta(&m,0,s->rate,revision,&s->backend,NULL,&error);if(!u->master)goto fail;
    mixer_buses_update_flush(u->track);mixer_buses_update_flush(u->master);
    struct live_update*expected=NULL;
    if(atomic_compare_exchange_strong_explicit(&s->pending,&expected,u,memory_order_release,memory_order_relaxed))return MIXER_BUS_OK;
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
void mixer_live_rebind(struct mixer_live*s,void*engine,void*program){if(s){s->engine=engine;s->program=program;}}
int mixer_live_bus_view(const struct mixer_live*s,unsigned bus,struct mixer_pcm_view*out){return s&&mixer_route_view(s->track,bus,out);}
int mixer_live_fx_monitor(struct mixer_live*s,unsigned channel,unsigned slot,struct mixer_fx_monitor*out)
{
    struct mixer_buses*track=mixer_route_buses(s->track);
    mixer_buses_monitor(track,channel<8?channel:9,slot);
    mixer_buses_monitor(s->master,channel==8?8:9,slot);
    struct mixer_fx_monitor track_packet,master_packet;
    int t=mixer_buses_take_monitor(track,&track_packet);
    int m=mixer_buses_take_monitor(s->master,&master_packet);
    if(channel<8&&t&&track_packet.channel==channel&&track_packet.slot==slot){*out=track_packet;return 1;}
    if(channel==8&&m&&master_packet.slot==slot){*out=master_packet;return 1;}
    return 0;
}
int mixer_live_take_meter(struct mixer_live*s,struct mixer_meter_snapshot*out){return s&&mixer_meters_take(s->meters,s->epoch,out);}
static _Thread_local uintptr_t offline_kit_vptr;
void mixer_live_program(const struct mixer_route_ops*ops,void*p,void*clock,unsigned frames)
{
    struct mixer_live*s=live_scope;
    atomic_fetch_add_explicit(&diag_program_calls,1,memory_order_relaxed);
    if(!s&&atomic_load_explicit(&diag_engine_active,memory_order_relaxed))
        atomic_fetch_add_explicit(&diag_program_other_thread,1,memory_order_relaxed);
    if(s&&offline_kit_vptr&&*(uintptr_t*)p==offline_kit_vptr)s->program=p;
    if(s){atomic_fetch_add_explicit(&diag_program_scoped,1,memory_order_relaxed);
        if(s->program!=p){atomic_fetch_add_explicit(&diag_program_other_object,1,memory_order_relaxed);
            for(unsigned i=0;i<4;++i){uintptr_t expected=0,v=(uintptr_t)p;
                uintptr_t now=atomic_load_explicit(&diag_seen[i],memory_order_relaxed);
                if(now==v)break;
                if(!now&&atomic_compare_exchange_strong_explicit(&diag_seen[i],&expected,v,memory_order_relaxed,memory_order_relaxed))break;}}}
    if(!s||s->program!=p){ops->render(p,clock,frames);return;}
    atomic_fetch_add_explicit(&diag_program_matched,1,memory_order_relaxed);
    s->matched_program=1;
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
    s->matched_program=0;
    s->block_result=MIXER_ROUTE_PASSTHROUGH;live_scope=s;
    atomic_fetch_add_explicit(&diag_engine_blocks,1,memory_order_relaxed);
    atomic_fetch_add_explicit(&diag_engine_active,1,memory_order_relaxed);
    s->ops.engine(engine,clock,frames);
    atomic_fetch_sub_explicit(&diag_engine_active,1,memory_order_relaxed);
    live_scope=NULL;
    if(s->matched_program)atomic_fetch_add_explicit(&diag_engine_matched,1,memory_order_relaxed);
    struct mixer_meter_input taps[MIXER_METER_BUSES]={0};
    if (!s->matched_program) {
        mixer_meters_push(s->meters,s->epoch,frames,taps);
        return 0;
    }
    if(s->block_result==MIXER_ROUTE_APPLIED)for(unsigned bus=0;bus<9;++bus){struct mixer_pcm_view v;
        if(mixer_route_view(s->track,bus,&v)&&v.frames==frames)taps[bus]=(struct mixer_meter_input){{v.channel[0],v.channel[1]},2,0};}
    void*out=s->ops.final_output(engine);int success=0;
    if(out&&integer(out,0)==2&&integer(out,4)>=frames){float**pcm=pointer(out,16);unsigned clear=*((unsigned char*)out+0x128);
        if(pcm&&pcm[0]&&pcm[1]&&clear<=1&&mixer_buses_begin(s->master,frames)==MIXER_BUS_OK){
            mixer_buses_key_source(s->master,s->block_result==MIXER_ROUTE_APPLIED?mixer_route_buses(s->track):NULL);
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
    float returns[MIXER_TRACKS]={0};
    uint32_t return_mask=s->block_result==MIXER_ROUTE_APPLIED?
        mixer_buses_return_peaks(mixer_route_buses(s->track),returns):0;
    mixer_meters_push_returns(s->meters,s->epoch,frames,taps,return_mask,returns);return success;
}

int mixer_live_render_offline(struct mixer_live *live,void *engine,void *clock,
                              unsigned frames,uintptr_t program_vptr)
{
    uintptr_t previous=offline_kit_vptr;
    offline_kit_vptr=program_vptr;
    int applied=mixer_live_render(live,engine,clock,frames);
    offline_kit_vptr=previous;
    return applied;
}
