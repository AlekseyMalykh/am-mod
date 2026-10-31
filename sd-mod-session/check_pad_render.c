#include "src/pad_render.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static struct pad_render_job *create(float*p,uint64_t frames,unsigned channels,
    struct pad_render_request*r,const struct fx_backend*b)
{
    struct pad_render_source s={p,frames,channels,48000};enum pad_render_status error;
    struct pad_render_job*j=pad_render_create(&s,r,b,&error);
    assert(j&&error==PAD_RENDER_WORKING);return j;
}
static void finish(struct pad_render_job*j,size_t budget)
{
    enum pad_render_status state;unsigned calls=0;
    do{state=pad_render_step(j,budget);assert(++calls<100000);}while(state==PAD_RENDER_WORKING);
    assert(state==PAD_RENDER_DONE);
}
static unsigned creates,destroys,processes,fail_create,nan_process;
static struct pad_render_job*cancel_job;
static void*native_create(void*u,uint32_t id,double rate,const float*p)
{
    (void)u;(void)p;assert(id==1&&rate==48000);if(++creates==fail_create)return NULL;
    return calloc(1,sizeof(float)*2);
}
static void native_destroy(void*u,void*p){(void)u;++destroys;free(p);}
/* Delayed half-gain fixture makes stage order/tail/nonfinite checks explicit.
 * Actual native DSP is exercised by check_pad_render_native.c separately. */
static void native_process(void*u,void*p,float*data,size_t frames)
{
    (void)u;float*history=p;++processes;
    for(size_t f=0;f<frames;++f)for(unsigned c=0;c<2;++c){
        float next=data[f*2+c];data[f*2+c]=history[c]*.5f;history[c]=next;
    }
    if(processes==nan_process)data[0]=NAN;
    if(cancel_job)pad_render_cancel(cancel_job);
}
static const struct fx_backend backend={NULL,native_create,native_destroy,native_process};
int main(void)
{
    float original[20];for(unsigned i=0;i<20;++i)original[i]=(float)i/20;
    struct pad_render_request r={.first=2,.end=8,.repeats=3,.tail_frames=4};
    struct pad_render_job*j=create(original,10,2,&r,NULL);
    uint64_t frames=99;double rate=99;assert(!pad_render_output(j,&frames,&rate)&&!frames&&!rate);
    memset(original,0,sizeof(original));r.first=0;r.repeats=64; /* immutable snapshot */
    assert(pad_render_step(j,0)==PAD_RENDER_INVALID);
    struct pad_render_progress p;pad_render_progress(j,&p);assert(!p.done&&p.total==22&&p.status==PAD_RENDER_WORKING);
    finish(j,3);const float*out=pad_render_output(j,&frames,&rate);assert(out&&frames==22&&rate==48000);
    for(unsigned f=0;f<22;++f)for(unsigned c=0;c<2;++c){
        float expected=f<18?(float)((2+f%6)*2+c)/20:0;assert(out[f*2+c]==expected);
    }
    pad_render_destroy(j);
    float mono[]={.1f,-.2f,.3f};r=(struct pad_render_request){.first=0,.end=3,.repeats=1};
    j=create(mono,3,1,&r,NULL);finish(j,4096);out=pad_render_output(j,&frames,&rate);
    for(unsigned i=0;i<3;++i)assert(out[i*2]==mono[i]&&out[i*2+1]==mono[i]);
    pad_render_destroy(j);
    puts("PASS immutable stereo/mono source snapshot, exclusive trim, finite repeats, exact tail length, bounded partial steps and completed-output gate");

    enum pad_render_status error;struct pad_render_source s={mono,3,1,48000};
    r.repeats=0;assert(!pad_render_create(&s,&r,NULL,&error)&&error==PAD_RENDER_INVALID);r.repeats=1;
    r.end=4;assert(!pad_render_create(&s,&r,NULL,&error));r.end=3;
    r.pcm_budget=47;assert(!pad_render_create(&s,&r,NULL,&error)&&error==PAD_RENDER_LIMIT);
    r.pcm_budget=48;j=pad_render_create(&s,&r,NULL,&error);assert(j);pad_render_destroy(j);
    r.pcm_budget=PAD_RENDER_MAX_PCM_BYTES+1;assert(!pad_render_create(&s,&r,NULL,&error)&&error==PAD_RENDER_LIMIT);r.pcm_budget=0;
    r.tail_frames=UINT64_MAX;assert(!pad_render_create(&s,&r,NULL,&error)&&error==PAD_RENDER_LIMIT);r.tail_frames=0;
    r.chain.count=FX_CHAIN_MAX+1;assert(!pad_render_create(&s,&r,NULL,&error));r.chain.count=0;
    mono[0]=NAN;assert(!pad_render_create(&s,&r,NULL,&error)&&error==PAD_RENDER_AUDIO_ERROR);mono[0]=.1f;
    puts("PASS memory preflight counts source+output, exact budget boundary, overflow/oversized chain/nonfinite input reject without changing caller audio");

    float signal[12000];for(unsigned i=0;i<12000;++i)signal[i]=.1f*sinf((float)i*.07f);
    r=(struct pad_render_request){.first=0,.end=6000,.repeats=1,.tail_frames=500};
    struct fx_stage eq={.kind=FX_STAGE_EQ};fx_eq_default(&eq.eq);eq.eq.band[1].gain_db=9;
    assert(fx_chain_add(&r.chain,&eq,48000)==FX_OK);
    j=create(signal,6000,2,&r,NULL);
    struct pad_render_job*k=create(signal,6000,2,&r,NULL);
    assert(pad_render_step(j,SIZE_MAX)==PAD_RENDER_WORKING);pad_render_progress(j,&p);assert(p.done==4096);
    finish(j,4096);finish(k,7);
    uint64_t other_frames;double other_rate;out=pad_render_output(j,&frames,&rate);
    const float*other=pad_render_output(k,&other_frames,&other_rate);
    assert(frames==other_frames&&rate==other_rate&&!memcmp(out,other,(size_t)frames*2*sizeof(float)));
    double difference=0;for(unsigned i=0;i<12000;++i)difference+=fabs(out[i]-signal[i]);assert(difference>1);
    pad_render_destroy(j);pad_render_destroy(k);
    eq.bypass=1;r.chain.stage[0]=eq;
    j=create(signal,6000,2,&r,NULL);finish(j,256);out=pad_render_output(j,&frames,&rate);
    assert(!memcmp(out,signal,sizeof(signal)));for(unsigned i=12000;i<frames*2;++i)assert(out[i]==0);pad_render_destroy(j);
    puts("PASS EQ changes actual PCM; render independent of chunk size, rerender resets histories from original, exact Bypass and4096-frame step cap");

    r=(struct pad_render_request){.first=0,.end=3,.repeats=1,.tail_frames=2};
    struct fx_stage effect={.kind=FX_STAGE_NATIVE,.native_id=1};
    assert(fx_chain_add(&r.chain,&effect,48000)==FX_OK);
    j=create(mono,3,1,&r,&backend);finish(j,2);out=pad_render_output(j,&frames,&rate);
    assert(frames==5&&out[0]==0&&out[2]==.05f&&out[6]==.15f&&out[8]==0);pad_render_destroy(j);
    assert(creates==destroys);
    creates=destroys=0;r.chain.count=4;for(unsigned i=0;i<4;++i)r.chain.stage[i]=effect;
    fail_create=3;assert(!pad_render_create(&s,&r,&backend,&error)&&error==PAD_RENDER_FX_ERROR&&creates==3&&destroys==2);fail_create=0;
    creates=destroys=0;r=(struct pad_render_request){.first=0,.end=6000,.repeats=1,.chain={.count=1,.stage={{.kind=FX_STAGE_NATIVE,.native_id=1}}}};
    processes=0;nan_process=2;j=create(signal,6000,2,&r,&backend);
    assert(pad_render_step(j,4096)==PAD_RENDER_AUDIO_ERROR&&!pad_render_output(j,NULL,NULL));pad_render_destroy(j);assert(creates==destroys);nan_process=0;
    j=create(signal,6000,2,&r,&backend);cancel_job=j;
    assert(pad_render_step(j,4096)==PAD_RENDER_CANCELLED);cancel_job=NULL;
    pad_render_progress(j,&p);assert(p.done==256&&!pad_render_output(j,NULL,NULL));pad_render_destroy(j);assert(creates==destroys);
    struct pad_render_source large={signal,6000,2,48000};
    assert(!pad_render_create(&large,&r,NULL,&error)&&error==PAD_RENDER_FX_ERROR);
    r.chain.count=0;j=create(signal,6000,2,&r,NULL);pad_render_cancel(j);
    assert(pad_render_step(j,256)==PAD_RENDER_CANCELLED&&!pad_render_output(j,NULL,NULL));pad_render_destroy(j);
    float loud[]={2.f,-3.f};r=(struct pad_render_request){.first=0,.end=1,.repeats=1};
    j=create(loud,1,2,&r,NULL);finish(j,1);out=pad_render_output(j,NULL,NULL);pad_render_progress(j,&p);
    assert(out[0]==2.f&&out[1]==-3.f&&p.peak==3&&p.over_unity_frames==1);pad_render_destroy(j);
    puts("PASS finite processed FX tail, failed backend creation cleanup, nonfinite DSP fails closed, cancel before/during render and no output exposure; peak/headroom reported without hidden normalization");
    return 0;
}
