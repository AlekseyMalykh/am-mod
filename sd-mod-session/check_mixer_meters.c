#include "src/mixer_meters.h"
#include <assert.h>
#include <math.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

static void peaks(void)
{
    assert(!mixer_meters_create(NAN)&&!mixer_meters_create(0));
    struct mixer_meters*m=mixer_meters_create(48000);assert(m);
    float l[128]={0},r[128]={0};l[11]=.25f;r[97]=-.5f;
    struct mixer_meter_input in[MIXER_METER_BUSES]={0};
    in[0]=(struct mixer_meter_input){{l,r},2,0};
    in[MIXER_METER_MASTER]=(struct mixer_meter_input){{r,NULL},1,0};
    struct mixer_meter_snapshot out={0};
    assert(mixer_meters_push(m,1,128,in)==1&&mixer_meters_take(m,1,&out));
    assert(out.epoch==1&&out.frame_end==128&&out.valid_mask==((1u<<0)|(1u<<MIXER_METER_MASTER))&&!out.clip_mask&&!out.fault_mask);
    assert(out.peak[0][0]==.25f&&out.peak[0][1]==.5f);
    assert(out.peak[MIXER_METER_MASTER][0]==.5f&&out.peak[MIXER_METER_MASTER][1]==.5f);
    assert(fabsf(mixer_meter_dbfs(.25f)+12.0412f)<.0001f);
    assert(isinf(mixer_meter_dbfs(0))&&mixer_meter_dbfs(0)<0&&isnan(mixer_meter_dbfs(NAN)));
    assert(l[11]==.25f&&r[97]==-.5f);
    in[0].clear=1;in[0].channel[0]=in[0].channel[1]=NULL;
    assert(mixer_meters_push(m,1,128,in)==1&&mixer_meters_take(m,1,&out));
    assert(out.valid_mask&1);assert(out.peak[0][0]<.25f&&out.peak[0][0]>.24f);
    in[0].clear=0;assert(mixer_meters_push(m,1,128,in)==1&&mixer_meters_take(m,1,&out));
    assert(!(out.valid_mask&1)&&(out.fault_mask&1)&&out.peak[0][0]==0);
    in[0]=(struct mixer_meter_input){{l,r},2,0};l[0]=NAN;
    assert(mixer_meters_push(m,1,128,in)==1&&mixer_meters_take(m,1,&out));
    assert(!(out.valid_mask&1)&&(out.fault_mask&1));l[0]=0;
    assert(mixer_meters_push(m,2,128,in)==1&&mixer_meters_take(m,2,&out));
    assert(out.epoch==2&&out.frame_end==128&&out.peak[0][0]==.25f);
    assert(mixer_meters_push(m,1,128,in)==-1);
    assert(mixer_meters_push(m,2,8193,in)==-1&&mixer_meters_push(m,2,0,in)==-1);
    struct mixer_meter_snapshot saved=out;assert(!mixer_meters_take(m,2,&out)&&!memcmp(&out,&saved,sizeof(out)));
    mixer_meters_destroy(m);
    puts("PASS real L/R peaks, mono, dBFS, native clear semantics, missing/invalid taps and epoch reset");
}
static void overload(void)
{
    struct mixer_meters*m=mixer_meters_create(8000);assert(m);
    struct mixer_meter_input in[MIXER_METER_BUSES]={0};float impulse[1]={0};
    in[2]=(struct mixer_meter_input){{impulse,NULL},1,0};struct mixer_meter_snapshot out;
    for(unsigned i=0;i<MIXER_METER_QUEUE;++i)assert(mixer_meters_push(m,4,1,in)==1);
    impulse[0]=1.25f;assert(mixer_meters_push(m,4,1,in)==0);impulse[0]=0;
    for(unsigned i=0;i<100;++i)assert(mixer_meters_push(m,4,1,in)==0);
    assert(mixer_meters_take(m,4,&out)&&out.frame_end==MIXER_METER_QUEUE);
    assert(mixer_meters_push(m,4,1,in)==1&&mixer_meters_take(m,4,&out));
    assert(out.peak[2][0]==1.25f&&(out.clip_mask&(1u<<2)));
    assert(mixer_meters_push(m,4,4000,(struct mixer_meter_input[MIXER_METER_BUSES]){{{NULL,NULL},0,0}})==1);
    assert(mixer_meters_take(m,4,&out)&&!out.valid_mask&&!out.clip_mask);
    /* Sample-clock hold test at block-sized zero taps; no wall-clock sleep. */
    in[2].clear=1;assert(mixer_meters_push(m,5,1,in)==1);assert(mixer_meters_take(m,5,&out));
    in[2].clear=0;impulse[0]=1;assert(mixer_meters_push(m,5,1,in)==1);assert(mixer_meters_take(m,5,&out));
    in[2].clear=1;
    assert(mixer_meters_push(m,5,7999,in)==1&&mixer_meters_take(m,5,&out)&&(out.clip_mask&(1u<<2)));
    assert(mixer_meters_push(m,5,1,in)==1&&mixer_meters_take(m,5,&out)&&!(out.clip_mask&(1u<<2)));
    assert(out.peak[2][0]<.04f&&out.peak[2][0]>.03f);
    /* Old-epoch slots cannot be mistaken for a new project's meters. */
    assert(mixer_meters_push(m,5,1,in)==1);
    struct mixer_meter_snapshot saved=out;assert(!mixer_meters_take(m,6,&out)&&!memcmp(&out,&saved,sizeof(out)));
    mixer_meters_destroy(m);
    puts("PASS full queue never stalls audio or loses pending transient; exact sample-clock clipping hold and stale-epoch filtering");
}
struct stress {struct mixer_meters*meters;atomic_uint done;unsigned reads;};
static void*producer(void*arg)
{
    struct stress*s=arg;float l[16],r[16];struct mixer_meter_input in[MIXER_METER_BUSES]={0};
    in[0]=(struct mixer_meter_input){{l,r},2,0};
    for(unsigned block=0;block<20000;++block){
        for(unsigned i=0;i<16;++i){l[i]=(float)(block%100)/100;r[i]=-l[i]*.5f;}
        assert(mixer_meters_push(s->meters,10,16,in)>=0);
    }
    atomic_store_explicit(&s->done,1,memory_order_release);return NULL;
}
static void*consumer(void*arg)
{
    struct stress*s=arg;uint64_t previous=0;struct mixer_meter_snapshot snapshot;
    do{
        if(mixer_meters_take(s->meters,10,&snapshot)){
            assert(snapshot.frame_end>previous&&snapshot.frame_end%16==0&&snapshot.valid_mask==1&&!snapshot.fault_mask);
            assert(isfinite(snapshot.peak[0][0])&&snapshot.peak[0][0]>=0&&snapshot.peak[0][0]<=.99f);
            assert(snapshot.peak[0][1]==snapshot.peak[0][0]*.5f);previous=snapshot.frame_end;++s->reads;
        }
    }while(!atomic_load_explicit(&s->done,memory_order_acquire));
    return NULL;
}
int main(void)
{
    peaks();overload();struct stress s={0};s.meters=mixer_meters_create(48000);assert(s.meters);atomic_init(&s.done,0);
    pthread_t audio,ui;assert(!pthread_create(&ui,NULL,consumer,&s));assert(!pthread_create(&audio,NULL,producer,&s));
    assert(!pthread_join(audio,NULL)&&!pthread_join(ui,NULL));assert(s.reads>0);mixer_meters_destroy(s.meters);
    printf("PASS concurrent audio/UI telemetry with bounded SPSC ownership; state RAM %zu bytes\n",mixer_meters_bytes());return 0;
}
