#include "mixer_meters.h"
#include <math.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#if ATOMIC_INT_LOCK_FREE != 2
#error Mixer telemetry requires always-lock-free unsigned indices
#endif
struct mixer_meters {
    /* SPSC ring: release/acquire hands each ordinary-data slot to exactly
     * one owner. Audio never rewrites a slot until UI has released it. */
    _Atomic unsigned read,write;
    struct mixer_meter_snapshot queue[MIXER_METER_QUEUE];
    /* Everything below belongs exclusively to the audio producer. */
    double rate;
    uint64_t epoch,frames,clip_until[MIXER_METER_BUSES];
    float envelope[MIXER_METER_BUSES][2],pending[MIXER_METER_BUSES][2];
};
size_t mixer_meters_bytes(void){return sizeof(struct mixer_meters);}
struct mixer_meters *mixer_meters_create(double rate)
{
    if(!isfinite(rate)||rate<8000||rate>192000)return NULL;
    struct mixer_meters*m=calloc(1,sizeof(*m));if(!m)return NULL;
    atomic_init(&m->read,0);atomic_init(&m->write,0);m->rate=rate;
    if(!atomic_is_lock_free(&m->read)||!atomic_is_lock_free(&m->write)){free(m);return NULL;}
    return m;
}
void mixer_meters_destroy(struct mixer_meters*m){free(m);}
int mixer_meters_push(struct mixer_meters*m,uint64_t epoch,unsigned frames,const struct mixer_meter_input in[MIXER_METER_BUSES])
{
    if(!m||!in||!epoch||epoch<m->epoch||!frames||frames>8192)return -1;
    uint64_t start=epoch==m->epoch?m->frames:0;
    uint64_t hold=(uint64_t)ceil(m->rate);
    if(start>UINT64_MAX-frames-hold)return -1;
    if(epoch!=m->epoch){
        m->epoch=epoch;m->frames=0;memset(m->envelope,0,sizeof(m->envelope));
        memset(m->pending,0,sizeof(m->pending));memset(m->clip_until,0,sizeof(m->clip_until));
    }
    struct mixer_meter_snapshot snapshot={0};snapshot.epoch=epoch;snapshot.frame_end=start+frames;
    float decay=expf(-(float)frames/(float)(m->rate*.300));
    for(unsigned bus=0;bus<MIXER_METER_BUSES;++bus){
        const struct mixer_meter_input*s=in+bus;uint32_t bit=1u<<bus;float peak[2]={0};
        int valid=s->channels==1||s->channels==2,fault=0;
        if(valid&&!s->clear&&(!s->channel[0]||(s->channels==2&&!s->channel[1]))){valid=0;fault=1;}
        if(s->channels>2){valid=0;fault=1;}
        if(valid&&!s->clear){
            for(unsigned i=0;i<frames;++i){
                float l=s->channel[0][i],r=s->channels==2?s->channel[1][i]:l;
                if(!isfinite(l)||!isfinite(r)){fault=1;continue;}
                l=fabsf(l);r=fabsf(r);peak[0]=fmaxf(peak[0],l);peak[1]=fmaxf(peak[1],r);
                if(l>=1||r>=1)m->clip_until[bus]=start+i+hold+1;
            }
        }
        if(fault){snapshot.fault_mask|=bit;valid=0;}
        if(!valid){
            m->envelope[bus][0]=m->envelope[bus][1]=0;
            m->pending[bus][0]=m->pending[bus][1]=0;m->clip_until[bus]=0;continue;
        }
        snapshot.valid_mask|=bit;
        if(m->clip_until[bus]>snapshot.frame_end)snapshot.clip_mask|=bit;
        for(unsigned ch=0;ch<2;++ch){
            m->envelope[bus][ch]=fmaxf(peak[ch],m->envelope[bus][ch]*decay);
            m->pending[bus][ch]=fmaxf(peak[ch],m->pending[bus][ch]);
            snapshot.peak[bus][ch]=fmaxf(m->envelope[bus][ch],m->pending[bus][ch]);
        }
    }
    m->frames=snapshot.frame_end;
    unsigned write=atomic_load_explicit(&m->write,memory_order_relaxed);
    unsigned read=atomic_load_explicit(&m->read,memory_order_acquire);
    if((unsigned)(write-read)>=MIXER_METER_QUEUE)return 0;
    m->queue[write%MIXER_METER_QUEUE]=snapshot;
    atomic_store_explicit(&m->write,write+1,memory_order_release);
    memset(m->pending,0,sizeof(m->pending));return 1;
}
int mixer_meters_take(struct mixer_meters*m,uint64_t epoch,struct mixer_meter_snapshot*out)
{
    if(!m||!epoch||!out)return 0;
    unsigned read=atomic_load_explicit(&m->read,memory_order_relaxed);
    unsigned end=atomic_load_explicit(&m->write,memory_order_acquire);
    struct mixer_meter_snapshot latest;int found=0;
    /* Capture end once: a busy producer cannot make this an unbounded loop. */
    unsigned available=(unsigned)(end-read);if(available>MIXER_METER_QUEUE)return 0;
    for(unsigned i=0;i<available;++i){
        struct mixer_meter_snapshot next=m->queue[read%MIXER_METER_QUEUE];
        ++read;atomic_store_explicit(&m->read,read,memory_order_release);
        if(next.epoch==epoch){latest=next;found=1;}
    }
    if(found)*out=latest;
    return found;
}
float mixer_meter_dbfs(float peak)
{
    if(!isfinite(peak)||peak<0)return NAN;
    return peak>0?20*log10f(peak):-INFINITY;
}
