#include "mixer_buses.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

struct mixer_buses {
    unsigned max_frames,frames,state,solo;
    enum mixer_bus_result fault;
    unsigned char output[MIXER_PADS];
    float gain[9][2];
    struct fx_program*fx[9];
    float *pcm,*scratch;
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
        const struct mixer_mix*m=i<MIXER_TRACKS?mix+i:&p->master.mix;
        if(m->send!=0){result(error,MIXER_BUS_UNSUPPORTED);return NULL;}
        if(!fx_chain_valid(&c->fx,rate))return NULL;
    }
    struct mixer_buses*b=calloc(1,sizeof(*b));if(!b){result(error,MIXER_BUS_RESOURCE);return NULL;}
    b->max_frames=max_frames;
    /* Ten stereo planar buses plus one shared stereo interleaved FX block. */
    b->pcm=calloc((size_t)max_frames*22,sizeof(float));
    if(!b->pcm){free(b);result(error,MIXER_BUS_RESOURCE);return NULL;}
    b->scratch=b->pcm+(size_t)max_frames*20;
    memcpy(b->output,p->output,sizeof(b->output));
    for(unsigned i=0;i<MIXER_TRACKS;++i)if(mix[i].solo)b->solo=1;
    for(unsigned i=0;i<=MIXER_TRACKS;++i){
        const struct mixer_channel*c=i<MIXER_TRACKS?p->track+i:&p->master;
        const struct mixer_mix*m=i<MIXER_TRACKS?mix+i:&p->master.mix;
        int audible=i==MIXER_TRACKS||(!m->mute&&(!b->solo||m->solo));
        gains(b->gain[i],m,audible);
        if(!c->fx_bypass&&c->fx.count){
            b->fx[i]=fx_program_prepare(&c->fx,rate,backend);
            if(!b->fx[i]){mixer_buses_destroy(b);result(error,MIXER_BUS_FX_ERROR);return NULL;}
        }
    }
    result(error,MIXER_BUS_OK);return b;
}
void mixer_buses_destroy(struct mixer_buses*b)
{
    if(!b)return;
    for(unsigned i=0;i<9;++i)fx_program_destroy(b->fx[i]);
    free(b->pcm);free(b);
}
enum mixer_bus_result mixer_buses_begin(struct mixer_buses*b,unsigned frames)
{
    if(!b||!frames||frames>b->max_frames)return MIXER_BUS_INVALID;
    if(b->state==1)return MIXER_BUS_STATE;
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
    float*l=channel(b,bus,0),*r=channel(b,bus,1);
    if(channels==1)right=left;
    for(unsigned i=0;i<b->frames;++i){
        float a=left[i],c=right[i];
        if(!isfinite(a)||!isfinite(c)||!isfinite(l[i]+a)||!isfinite(r[i]+c))return b->fault=MIXER_BUS_AUDIO_ERROR;
        l[i]+=a;r[i]+=c;
    }
    return MIXER_BUS_OK;
}
static int process(struct mixer_buses*b,unsigned bus,unsigned fx)
{
    float*l=channel(b,bus,0),*r=channel(b,bus,1);
    if(b->fx[fx]){
        for(unsigned i=0;i<b->frames;++i){b->scratch[i*2]=l[i];b->scratch[i*2+1]=r[i];}
        fx_program_process(b->fx[fx],b->scratch,b->frames);
        for(unsigned i=0;i<b->frames;++i){l[i]=b->scratch[i*2];r[i]=b->scratch[i*2+1];}
    }
    for(unsigned i=0;i<b->frames;++i){
        if(!isfinite(l[i])||!isfinite(r[i]))return 0;
        l[i]*=b->gain[fx][0];r[i]*=b->gain[fx][1];
        if(!isfinite(l[i])||!isfinite(r[i]))return 0;
    }
    return 1;
}
enum mixer_bus_result mixer_buses_finish(struct mixer_buses*b,struct mixer_meters*meters,uint64_t epoch)
{
    if(!b||b->state!=1)return MIXER_BUS_STATE;
    if(meters&&!epoch)b->fault=MIXER_BUS_INVALID;
    if(b->fault==MIXER_BUS_OK){
        for(unsigned i=0;i<MIXER_TRACKS;++i)if(!process(b,i,i)){b->fault=MIXER_BUS_AUDIO_ERROR;break;}
        if(b->solo)for(unsigned ch=0;ch<2;++ch)memset(channel(b,MIXER_METER_DIRECT,ch),0,b->frames*sizeof(float));
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
    }else {memset(b->pcm,0,(size_t)b->max_frames*20*sizeof(float));b->state=3;}
    /* Failure publishes unavailable taps, not stale peaks from a former block.
     * A full queue is telemetry backpressure only, never an audio error. */
    if(meters&&epoch)mixer_meters_push(meters,epoch,b->frames,taps);
    return b->fault;
}
int mixer_buses_view(const struct mixer_buses*b,unsigned bus,struct mixer_pcm_view*out)
{
    if(!b||!out||b->state!=2||bus>=MIXER_METER_BUSES)return 0;
    out->channel[0]=channel(b,bus,0);out->channel[1]=channel(b,bus,1);out->frames=b->frames;return 1;
}
size_t mixer_buses_pcm_bytes(const struct mixer_buses*b){return b?(size_t)b->max_frames*22*sizeof(float):0;}
