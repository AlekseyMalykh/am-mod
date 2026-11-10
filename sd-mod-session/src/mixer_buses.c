#include "mixer_buses.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#if ATOMIC_POINTER_LOCK_FREE != 2 || ATOMIC_LLONG_LOCK_FREE != 2
#error Mixer updates require lock-free pointer/revision publication
#endif

struct mixer_bus_update {
    uint64_t revision;
    double rate;
    unsigned char output[MIXER_PADS];
    unsigned solo;
    float gain[9][2],output_gate[MIXER_TRACKS];
    unsigned char send_to[MIXER_TRACKS];
    unsigned order[MIXER_TRACKS];
    float send[MIXER_TRACKS];
    struct fx_chain_config config[9];
    struct fx_program *fx[9];
};

struct mixer_buses {
    unsigned max_frames,frames,state,solo;
    enum mixer_bus_result fault;
    unsigned char output[MIXER_PADS];
    float gain[9][2],output_gate[MIXER_TRACKS];
    unsigned char send_to[MIXER_TRACKS];
    unsigned order[MIXER_TRACKS];
    float send[MIXER_TRACKS];
    struct fx_exchange*fx[9];
    struct fx_chain_config config[9];
    unsigned char previous_output[MIXER_PADS];
    float previous_gain[9][2],previous_send[MIXER_TRACKS],previous_output_gate[MIXER_TRACKS];
    unsigned previous_solo,ramp_position,ramp_total,transition;
    uint64_t target_revision;
    double rate;
    _Atomic uint64_t applied_revision;
    _Atomic(struct mixer_bus_update*) pending,retired;
    float *pcm,*scratch,*keys,*returns;
    uint32_t return_mask;
    float return_peak[MIXER_TRACKS];
    const struct mixer_buses*key_source;
    _Atomic unsigned monitor,monitor_ready;
    unsigned monitor_key,monitor_frames,monitor_offset;
    const float *monitor_detector;
    struct mixer_fx_monitor collecting,monitor_packet;
};
static void result(enum mixer_bus_result*out,enum mixer_bus_result value){if(out)*out=value;}
static float*channel(const struct mixer_buses*b,unsigned bus,unsigned ch){return b->pcm+(bus*2u+ch)*b->max_frames;}
static void gains(float out[2],const struct mixer_mix*mix,int audible)
{
    /* Stereo balance: centre is unity on both sides, extremes attenuate
     * only the opposite side. Existing per-Pad pan remains upstream. */
    float gain=audible&&isfinite(mix->level_db)?powf(10.f,mix->level_db/20.f):0;
    out[0]=gain*(mix->pan>0?1-mix->pan:1);out[1]=gain*(mix->pan<0?1+mix->pan:1);
}
struct mixer_buses*mixer_buses_prepare(const struct mixer_project*p,uint64_t part_id,double rate,unsigned max_frames,
    const struct fx_backend*backend,enum mixer_bus_result*error)
{
    result(error,MIXER_BUS_INVALID);
    if(!mixer_project_valid(p)||!isfinite(rate)||rate<8000||rate>192000||!max_frames||max_frames>8192)return NULL;
    struct mixer_mix mix[MIXER_TRACKS];mixer_scene_resolve(p,part_id,mix);
    for(unsigned i=0;i<=MIXER_TRACKS;++i){
        const struct mixer_channel*c=i<MIXER_TRACKS?p->track+i:&p->master;
        if(i==MIXER_TRACKS&&p->master.mix.send!=0){result(error,MIXER_BUS_UNSUPPORTED);return NULL;}
        if(!fx_chain_valid(&c->fx,rate))return NULL;
    }
    struct mixer_buses*b=calloc(1,sizeof(*b));if(!b){result(error,MIXER_BUS_RESOURCE);return NULL;}
    b->max_frames=max_frames;b->rate=rate;
    atomic_init(&b->monitor,0);atomic_init(&b->monitor_ready,0);
    atomic_init(&b->pending,NULL);atomic_init(&b->retired,NULL);atomic_init(&b->applied_revision,0);
    /* Ten stereo planar buses plus one shared stereo interleaved FX block. */
    b->pcm=calloc((size_t)max_frames*(22+FX_SIDECHAIN_KEYS+2*MIXER_TRACKS),sizeof(float));
    if(!b->pcm){free(b);result(error,MIXER_BUS_RESOURCE);return NULL;}
    b->scratch=b->pcm+(size_t)max_frames*20;
    b->keys=b->pcm+(size_t)max_frames*22;
    b->returns=b->keys+(size_t)max_frames*FX_SIDECHAIN_KEYS;
    memcpy(b->output,p->output,sizeof(b->output));
    memcpy(b->send_to,p->send_to,sizeof(b->send_to));
    mixer_send_order(b->send_to,b->order);
    for(unsigned i=0;i<MIXER_TRACKS;++i)b->send[i]=mix[i].send;
    for(unsigned i=0;i<MIXER_TRACKS;++i)if(mix[i].solo)b->solo=1;
    for(unsigned i=0;i<=MIXER_TRACKS;++i){
        const struct mixer_channel*c=i<MIXER_TRACKS?p->track+i:&p->master;
        const struct mixer_mix*m=i<MIXER_TRACKS?mix+i:&p->master.mix;
        gains(b->gain[i],m,i==MIXER_TRACKS||!m->mute);
        if(i<MIXER_TRACKS)b->output_gate[i]=!b->solo||m->solo;
        b->fx[i]=fx_exchange_create();if(!b->fx[i]){mixer_buses_destroy(b);result(error,MIXER_BUS_RESOURCE);return NULL;}
        if(!c->fx_bypass&&c->fx.count){
            b->config[i]=c->fx;
            struct fx_program *fx=fx_program_prepare(&c->fx,rate,backend);
            if(!fx){mixer_buses_destroy(b);result(error,MIXER_BUS_FX_ERROR);return NULL;}
            fx_exchange_seed(b->fx[i],fx);
        }
    }
    result(error,MIXER_BUS_OK);return b;
}
void mixer_buses_destroy(struct mixer_buses*b)
{
    if(!b)return;
    for(unsigned i=0;i<9;++i)fx_exchange_destroy(b->fx[i]);
    mixer_buses_update_destroy(atomic_load(&b->pending));mixer_buses_update_destroy(atomic_load(&b->retired));
    free(b->pcm);free(b);
}
static int same_fx(const struct fx_chain_config*a,const struct fx_chain_config*c)
{
    if(a->count!=c->count)return 0;
    for(unsigned i=0;i<a->count;++i){const struct fx_stage*x=a->stage+i,*y=c->stage+i;
        if(x->kind!=y->kind||x->bypass!=y->bypass)return 0;
        if(x->kind!=FX_STAGE_EQ){if(x->native_id!=y->native_id)return 0;
            for(unsigned p=0;p<FX_CHAIN_PARAMETERS;++p)if(x->parameter[p]!=y->parameter[p])return 0;
        }else{if(x->eq.count!=y->eq.count||x->eq.bypass!=y->eq.bypass||x->eq.output_db!=y->eq.output_db)return 0;
            for(unsigned j=0;j<x->eq.count;++j){const struct fx_eq_band*u=x->eq.band+j,*v=y->eq.band+j;
                if(u->hz!=v->hz||u->gain_db!=v->gain_db||u->q!=v->q||u->type!=v->type||u->enabled!=v->enabled)return 0;}}
    }
    return 1;
}
static void accept_update(struct mixer_buses*b)
{
    if(b->transition||atomic_load_explicit(&b->retired,memory_order_acquire))return;
    for(unsigned i=0;i<9;++i)if(!fx_exchange_ready_audio(b->fx[i]))return;
    struct mixer_bus_update*u=atomic_load_explicit(&b->pending,memory_order_acquire);
    if(u)for(unsigned i=0;i<9;++i)if(!same_fx(b->config+i,u->config+i)&&!u->fx[i])return;
    u=atomic_exchange_explicit(&b->pending,NULL,memory_order_acquire);
    if(!u)return;
    if(u->revision>b->target_revision){
        memcpy(b->previous_output,b->output,sizeof(b->output));memcpy(b->output,u->output,sizeof(b->output));
        memcpy(b->previous_gain,b->gain,sizeof(b->gain));memcpy(b->gain,u->gain,sizeof(b->gain));
        for(unsigned i=0;i<MIXER_TRACKS;++i)
            b->previous_send[i]=b->send_to[i]==u->send_to[i]?b->send[i]:0;
        memcpy(b->send,u->send,sizeof(b->send));
        memcpy(b->send_to,u->send_to,sizeof(b->send_to));
        memcpy(b->order,u->order,sizeof(b->order));
        memcpy(b->previous_output_gate,b->output_gate,sizeof(b->output_gate));
        memcpy(b->output_gate,u->output_gate,sizeof(b->output_gate));
        b->previous_solo=b->solo;b->solo=u->solo;b->target_revision=u->revision;
        b->ramp_position=0;b->ramp_total=(unsigned)(b->rate*.005);if(!b->ramp_total)b->ramp_total=1;
        b->transition=1;
        for(unsigned i=0;i<9;++i)if(!same_fx(b->config+i,u->config+i)){
            /* All slots were checked before consuming the packet. Only this
             * audio owner publishes here, so the nine-slot commit cannot fail. */
            if(fx_exchange_publish(b->fx[i],u->fx[i])==FX_OK)u->fx[i]=NULL;
            b->config[i]=u->config[i];
        }
    }
    atomic_store_explicit(&b->retired,u,memory_order_release);
}
static float blend(const struct mixer_buses*b,unsigned sample)
{
    if(!b->transition)return 1;
    unsigned position=b->ramp_position+sample+1;
    return position>=b->ramp_total?1.f:(float)position/b->ramp_total;
}
enum mixer_bus_result mixer_buses_begin(struct mixer_buses*b,unsigned frames)
{
    if(!b||!frames||frames>b->max_frames)return MIXER_BUS_INVALID;
    if(b->state==1)return MIXER_BUS_STATE;
    accept_update(b);
    b->frames=frames;b->state=1;b->fault=MIXER_BUS_OK;
    memset(b->pcm,0,(size_t)b->max_frames*20*sizeof(float));return MIXER_BUS_OK;
}
enum mixer_bus_result mixer_buses_accumulate(struct mixer_buses*b,unsigned pad,const float*left,const float*right,unsigned channels,unsigned clear)
{
    if(!b||b->state!=1)return MIXER_BUS_STATE;
    if(b->fault!=MIXER_BUS_OK)return b->fault;
    if(pad>=MIXER_PADS||(channels!=1&&channels!=2)||clear>1||(!clear&&(!left||(channels==2&&!right))))return b->fault=MIXER_BUS_INVALID;
    if(clear)return MIXER_BUS_OK;
    unsigned bus=b->output[pad]?(unsigned)b->output[pad]-1u:MIXER_METER_DIRECT;
    unsigned old=b->transition?(b->previous_output[pad]?(unsigned)b->previous_output[pad]-1u:MIXER_METER_DIRECT):bus;
    float*l=channel(b,bus,0),*r=channel(b,bus,1),*old_l=channel(b,old,0),*old_r=channel(b,old,1);
    if(channels==1)right=left;
    for(unsigned i=0;i<b->frames;++i){
        float a=left[i],c=right[i];
        float wet=old==bus?1:blend(b,i),dry=1-wet;
        if(!isfinite(a)||!isfinite(c)||!isfinite(l[i]+a*wet)||!isfinite(r[i]+c*wet)||
           !isfinite(old_l[i]+a*dry)||!isfinite(old_r[i]+c*dry))return b->fault=MIXER_BUS_AUDIO_ERROR;
        l[i]+=a*wet;r[i]+=c*wet;
        if(old!=bus){old_l[i]+=a*dry;old_r[i]+=c*dry;}
    }
    return MIXER_BUS_OK;
}
void mixer_buses_key_source(struct mixer_buses*b,const struct mixer_buses*source)
{b->key_source=source;}
void mixer_buses_monitor(struct mixer_buses*b,unsigned channel,unsigned slot)
{
    unsigned key=channel<9&&slot<FX_CHAIN_MAX?1+channel*FX_CHAIN_MAX+slot:0;
    atomic_store_explicit(&b->monitor,key,memory_order_relaxed);
}
int mixer_buses_take_monitor(struct mixer_buses*b,struct mixer_fx_monitor*out)
{
    if(!atomic_load_explicit(&b->monitor_ready,memory_order_acquire))return 0;
    *out=b->monitor_packet;
    atomic_store_explicit(&b->monitor_ready,0,memory_order_release);
    return 1;
}
static void monitor_stage(void*user,const float*pcm,size_t frames,int after,float minimum_gain)
{
    struct mixer_buses*b=user;
    unsigned n=MIXER_FX_MONITOR_FRAMES-b->monitor_frames;
    if(frames<n)n=(unsigned)frames;
    float peak=0;
    for(unsigned i=0;i<n;++i){
        float l=pcm[i*2],r=pcm[i*2+1];
        peak=fmaxf(peak,fmaxf(fabsf(l),fabsf(r)));
        float *wave=after?b->collecting.pcm:b->collecting.input_pcm;
        wave[b->monitor_frames+i]=.5f*l+.5f*r;
        if(!after&&b->monitor_detector)
            b->collecting.key_peak=fmaxf(b->collecting.key_peak,b->monitor_detector[b->monitor_offset+i]);
    }
    if(!after){b->collecting.input_peak=fmaxf(b->collecting.input_peak,peak);return;}
    b->collecting.output_peak=fmaxf(b->collecting.output_peak,peak);
    if(minimum_gain<1){
        float reduction=-20.f*log10f(fmaxf(minimum_gain,1e-6f));
        b->collecting.gain_reduction_db=fmaxf(b->collecting.gain_reduction_db,reduction);
    }
    b->monitor_offset+=(unsigned)frames;
    b->monitor_frames+=n;
    if(b->monitor_frames!=MIXER_FX_MONITOR_FRAMES)return;
    if(!atomic_load_explicit(&b->monitor_ready,memory_order_acquire)){
        b->monitor_packet=b->collecting;
        atomic_store_explicit(&b->monitor_ready,1,memory_order_release);
    }
    b->monitor_frames=0;
    b->collecting.input_peak=b->collecting.output_peak=b->collecting.key_peak=0;
    b->collecting.gain_reduction_db=0;
}
static int process(struct mixer_buses*b,unsigned bus,unsigned fx)
{
    float*l=channel(b,bus,0),*r=channel(b,bus,1);
    if(b->fx[fx]){
        for(unsigned i=0;i<b->frames;++i){b->scratch[i*2]=l[i];b->scratch[i*2+1]=r[i];}
        const struct mixer_buses*source=b->key_source?b->key_source:b;
        const float*keys[FX_SIDECHAIN_KEYS]={0};
        if(source->frames==b->frames&&(source==b||source->state==2))
            for(unsigned i=0;i<FX_SIDECHAIN_KEYS;++i)keys[i]=source->keys+(size_t)i*source->max_frames;
        unsigned key=atomic_load_explicit(&b->monitor,memory_order_relaxed);
        if(key&&((key-1)/FX_CHAIN_MAX)==fx){
            unsigned slot=(key-1)%FX_CHAIN_MAX;
            if(key!=b->monitor_key){
                b->monitor_key=key;b->monitor_frames=0;
                b->collecting.input_peak=b->collecting.output_peak=b->collecting.key_peak=0;
                b->collecting.gain_reduction_db=0;
                b->collecting.channel=fx;b->collecting.slot=slot;
            }
            b->monitor_detector=NULL;
            b->monitor_offset=0;
            if(slot<b->config[fx].count){
                const struct fx_stage*stage=b->config[fx].stage+slot;
                if(stage->kind==FX_STAGE_SIDECHAIN){
                    unsigned detector=(unsigned)fx_sidechain_value(0,stage->parameter[0]);
                    if(detector)b->monitor_detector=keys[detector-1];
                }
            }
            fx_exchange_process_keyed(b->fx[fx],b->scratch,b->frames,slot,monitor_stage,b,keys);
        }else{
            if(!key)b->monitor_key=0;
            fx_exchange_process_keyed(b->fx[fx],b->scratch,b->frames,0,NULL,NULL,keys);
        }
        for(unsigned i=0;i<b->frames;++i){l[i]=b->scratch[i*2];r[i]=b->scratch[i*2+1];}
    }
    for(unsigned i=0;i<b->frames;++i){
        if(!isfinite(l[i])||!isfinite(r[i]))return 0;
        float wet=blend(b,i);
        l[i]*=b->transition?b->previous_gain[fx][0]+wet*(b->gain[fx][0]-b->previous_gain[fx][0]):b->gain[fx][0];
        r[i]*=b->transition?b->previous_gain[fx][1]+wet*(b->gain[fx][1]-b->previous_gain[fx][1]):b->gain[fx][1];
        if(!isfinite(l[i])||!isfinite(r[i]))return 0;
    }
    return 1;
}
enum mixer_bus_result mixer_buses_finish(struct mixer_buses*b,struct mixer_meters*meters,uint64_t epoch)
{
    if(!b||b->state!=1)return MIXER_BUS_STATE;
    if(meters&&!epoch)b->fault=MIXER_BUS_INVALID;
    b->return_mask=0;
    if(b->fault==MIXER_BUS_OK){
        /* Snapshot the detector before any track FX, send, fader or gate.
         * All tracks can key each other without an audio dependency cycle. */
        for(unsigned i=0;i<FX_SIDECHAIN_KEYS;++i){
            float *key=b->keys+(size_t)i*b->max_frames;
            const float *l=channel(b,i,0),*r=channel(b,i,1);
            for(unsigned frame=0;frame<b->frames;++frame)key[frame]=fmaxf(fabsf(l[frame]),fabsf(r[frame]));
        }
        for(unsigned n=0;n<MIXER_TRACKS;++n){
            unsigned i=b->order[n];
            if(b->return_mask&(1u<<i)){
                float peak=0;
                for(unsigned ch=0;ch<2;++ch){
                    float*input=channel(b,i,ch);
                    const float*returns=b->returns+(size_t)(2*i+ch)*b->max_frames;
                    for(unsigned frame=0;frame<b->frames;++frame){
                        float value=returns[frame];
                        peak=fmaxf(peak,fabsf(value));
                        input[frame]+=value;
                    }
                }
                b->return_peak[i]=peak;
            }
            if(!process(b,i,i)){b->fault=MIXER_BUS_AUDIO_ERROR;break;}
            unsigned destination=b->send_to[i];
            if(destination&&(b->send[i]||(b->transition&&b->previous_send[i]))){
                unsigned bit=1u<<(destination-1);
                for(unsigned ch=0;ch<2;++ch){
                    const float*source=channel(b,i,ch);
                    float*target=b->returns+(size_t)(2*(destination-1)+ch)*b->max_frames;
                    if(!(b->return_mask&bit))memset(target,0,b->frames*sizeof(float));
                    for(unsigned frame=0;frame<b->frames;++frame){
                        float level=b->transition?b->previous_send[i]+blend(b,frame)*(b->send[i]-b->previous_send[i]):b->send[i];
                        target[frame]+=source[frame]*level;
                    }
                }
                b->return_mask|=bit;
            }
            /* A solo gates only the track output to Master. The post-fader
             * send stays available to a soloed destination, including through
             * intermediate tracks. Explicit Mute still cuts the send. */
            float gate=b->output_gate[i],before=b->transition?b->previous_output_gate[i]:gate;
            if(gate==1&&before==1)continue;
            for(unsigned ch=0;ch<2;++ch){
                float *output=channel(b,i,ch);
                if(gate==0&&before==0)memset(output,0,b->frames*sizeof(float));
                else for(unsigned frame=0;frame<b->frames;++frame)
                    output[frame]*=before+blend(b,frame)*(gate-before);
            }
        }
        for(unsigned ch=0;ch<2;++ch){float*direct=channel(b,MIXER_METER_DIRECT,ch);
            for(unsigned i=0;i<b->frames;++i){float gate=b->solo?0:1;
                if(b->transition){float before=b->previous_solo?0:1;gate=before+blend(b,i)*(gate-before);}
                direct[i]*=gate;}}
    }
    if(b->fault==MIXER_BUS_OK){
        for(unsigned ch=0;ch<2;++ch){float*out=channel(b,MIXER_METER_MASTER,ch);
            for(unsigned bus=0;bus<=MIXER_METER_DIRECT;++bus){const float*in=channel(b,bus,ch);
                for(unsigned i=0;i<b->frames;++i){out[i]+=in[i];if(!isfinite(out[i]))b->fault=MIXER_BUS_AUDIO_ERROR;}
            }
        }
        if(!process(b,MIXER_METER_MASTER,8))b->fault=MIXER_BUS_AUDIO_ERROR;
    }
    struct mixer_meter_input taps[MIXER_METER_BUSES]={0};
    if(b->fault==MIXER_BUS_OK){
        for(unsigned bus=0;bus<MIXER_METER_BUSES;++bus){taps[bus].channels=2;taps[bus].channel[0]=channel(b,bus,0);taps[bus].channel[1]=channel(b,bus,1);}
        b->state=2;
        if(b->transition){
            b->ramp_position+=b->frames;
            if(b->ramp_position>=b->ramp_total){int ready=1;
                for(unsigned i=0;i<9;++i)if(fx_exchange_transitioning_audio(b->fx[i]))ready=0;
                if(ready){b->transition=0;atomic_store_explicit(&b->applied_revision,b->target_revision,memory_order_release);}}
        }
    }else {memset(b->pcm,0,(size_t)b->max_frames*20*sizeof(float));b->state=3;}
    /* Failure publishes unavailable taps, not stale peaks from a former block.
     * A full queue is telemetry backpressure only, never an audio error. */
    if(meters&&epoch)mixer_meters_push_returns(meters,epoch,b->frames,taps,b->state==2?b->return_mask:0,b->return_peak);
    return b->fault;
}
int mixer_buses_view(const struct mixer_buses*b,unsigned bus,struct mixer_pcm_view*out)
{
    if(!b||!out||b->state!=2||bus>=MIXER_METER_BUSES)return 0;
    out->channel[0]=channel(b,bus,0);out->channel[1]=channel(b,bus,1);out->frames=b->frames;return 1;
}
uint32_t mixer_buses_return_peaks(const struct mixer_buses*b,float out[MIXER_TRACKS])
{
    if(!b||b->state!=2)return 0;
    memcpy(out,b->return_peak,sizeof(b->return_peak));
    return b->return_mask;
}
size_t mixer_buses_pcm_bytes(const struct mixer_buses*b){return b?(size_t)b->max_frames*(22+FX_SIDECHAIN_KEYS+2*MIXER_TRACKS)*sizeof(float):0;}
struct mixer_bus_update*mixer_buses_update_prepare_delta(const struct mixer_project*p,uint64_t part,double rate,
    uint64_t revision,const struct fx_backend*backend,const struct fx_chain_config previous[9],enum mixer_bus_result*error)
{
    result(error,MIXER_BUS_INVALID);
    if(!revision||!mixer_project_valid(p)||!isfinite(rate)||rate<8000||rate>192000)return NULL;
    struct mixer_mix mix[MIXER_TRACKS];mixer_scene_resolve(p,part,mix);
    struct mixer_bus_update*u=calloc(1,sizeof(*u));if(!u){result(error,MIXER_BUS_RESOURCE);return NULL;}
    u->revision=revision;u->rate=rate;memcpy(u->output,p->output,sizeof(u->output));
    memcpy(u->send_to,p->send_to,sizeof(u->send_to));
    mixer_send_order(u->send_to,u->order);
    for(unsigned i=0;i<MIXER_TRACKS;++i)u->send[i]=mix[i].send;
    for(unsigned i=0;i<8;++i)if(mix[i].solo)u->solo=1;
    for(unsigned i=0;i<9;++i){const struct mixer_channel*c=i<8?p->track+i:&p->master;
        const struct mixer_mix*m=i<8?mix+i:&p->master.mix;
        if(i==8&&m->send){result(error,MIXER_BUS_UNSUPPORTED);goto fail;}
        if(!fx_chain_valid(&c->fx,rate))goto fail;
        gains(u->gain[i],m,i==MIXER_TRACKS||!m->mute);
        if(i<MIXER_TRACKS)u->output_gate[i]=!u->solo||m->solo;
        if(!c->fx_bypass)u->config[i]=c->fx;
        if(!previous||!same_fx(previous+i,u->config+i)){
            u->fx[i]=fx_program_prepare(u->config+i,rate,backend);
            if(!u->fx[i]){result(error,MIXER_BUS_FX_ERROR);goto fail;}
        }
    }
    result(error,MIXER_BUS_OK);return u;
fail:mixer_buses_update_destroy(u);return NULL;
}
struct mixer_bus_update*mixer_buses_update_prepare(const struct mixer_project*p,uint64_t part,double rate,
    uint64_t revision,const struct fx_backend*backend,enum mixer_bus_result*error)
{return mixer_buses_update_prepare_delta(p,part,rate,revision,backend,NULL,error);}
void mixer_buses_update_destroy(struct mixer_bus_update*u)
{
    if(u){for(unsigned i=0;i<9;++i)fx_program_destroy(u->fx[i]);free(u);}
}
enum mixer_bus_result mixer_buses_publish(struct mixer_buses*b,struct mixer_bus_update*u)
{
    if(!b||!u||u->rate!=b->rate||u->revision<=atomic_load_explicit(&b->applied_revision,memory_order_acquire))return MIXER_BUS_INVALID;
    struct mixer_bus_update*expected=NULL;
    return atomic_compare_exchange_strong_explicit(&b->pending,&expected,u,memory_order_release,memory_order_relaxed)?MIXER_BUS_OK:MIXER_BUS_STATE;
}
void mixer_buses_collect(struct mixer_buses*b)
{
    if(!b)return;
    mixer_buses_update_destroy(atomic_exchange_explicit(&b->retired,NULL,memory_order_acquire));
    for(unsigned i=0;i<9;++i)fx_exchange_collect(b->fx[i]);
}
uint64_t mixer_buses_applied_revision(const struct mixer_buses*b)
{
    return b?atomic_load_explicit(&b->applied_revision,memory_order_acquire):0;
}
int mixer_buses_ready_audio(const struct mixer_buses*b)
{
    if(!b||b->transition||atomic_load_explicit(&b->pending,memory_order_acquire)||atomic_load_explicit(&b->retired,memory_order_acquire))return 0;
    for(unsigned i=0;i<9;++i)if(!fx_exchange_ready_audio(b->fx[i]))return 0;
    return 1;
}
