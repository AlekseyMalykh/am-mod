#include "pad_render.h"
#include <math.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

struct pad_render_job {
    struct fx_program *fx;
    float *original,*output;
    double rate,peak;
    uint64_t clip_frames,body_frames,total,done,over_unity;
    enum pad_render_status status;
    _Atomic unsigned cancel;
    struct pad_render_stream stream;
    uint64_t capture_first,captured;
};
static void close_stream(struct pad_render_job *j)
{
    if(j->stream.close){void(*close)(void*)=j->stream.close;j->stream.close=NULL;j->stream.read=NULL;close(j->stream.user);}
}
static void discard(struct pad_render_job *j,enum pad_render_status status)
{
    close_stream(j);
    fx_program_destroy(j->fx);j->fx=NULL;
    free(j->output);j->output=NULL;
    free(j->original);j->original=NULL;
    j->status=status;
}
void pad_render_destroy(struct pad_render_job *j)
{
    if(j){discard(j,j->status);free(j);}
}
static struct pad_render_job *allocate_job(uint64_t frames,double rate,
    const struct pad_render_request *r,const struct fx_backend *backend,enum pad_render_status *error)
{
    enum pad_render_status result=PAD_RENDER_INVALID;
    struct pad_render_job *j=NULL;
    if(!r||!frames||r->first>=r->end||r->end>frames||!r->repeats||r->repeats>64||
       !fx_chain_valid(&r->chain,rate))goto fail;
    const uint64_t clip=r->end-r->first;
    result=PAD_RENDER_LIMIT;
    if(clip>UINT64_MAX/r->repeats)goto fail;
    const uint64_t body=clip*r->repeats;
    if(r->tail_frames>UINT64_MAX-body)goto fail;
    const uint64_t total=body+r->tail_frames;
    /* Both retained source and destination count against the PCM budget.
     * Compare frames first, so even malicious counts cannot wrap bytes. */
    const size_t budget=r->pcm_budget?r->pcm_budget:PAD_RENDER_MAX_PCM_BYTES;
    const size_t max_frames=budget/(2*sizeof(float));
    if(budget>PAD_RENDER_MAX_PCM_BYTES||clip>max_frames||total>max_frames-clip)goto fail;
    result=PAD_RENDER_RESOURCE;
    j=calloc(1,sizeof(*j));if(!j)goto fail;
    atomic_init(&j->cancel,0);
    j->original=malloc((size_t)clip*2*sizeof(float));
    j->output=malloc((size_t)total*2*sizeof(float));
    if(!j->original||!j->output)goto fail;
    j->rate=rate;j->clip_frames=clip;j->body_frames=body;j->total=total;j->capture_first=r->first;
    j->fx=fx_program_prepare(&r->chain,rate,backend);
    if(!j->fx){result=PAD_RENDER_FX_ERROR;goto fail;}
    j->status=PAD_RENDER_WORKING;if(error)*error=PAD_RENDER_WORKING;return j;
fail:
    pad_render_destroy(j);if(error)*error=result;return NULL;
}
struct pad_render_job *pad_render_create(const struct pad_render_source *s,
    const struct pad_render_request *r,const struct fx_backend *backend,enum pad_render_status *error)
{
    if(!s||!s->pcm||(s->channels!=1&&s->channels!=2)||s->frames>SIZE_MAX/(s->channels*sizeof(float))){if(error)*error=PAD_RENDER_INVALID;return NULL;}
    struct pad_render_job*j=allocate_job(s->frames,s->rate,r,backend,error);if(!j)return NULL;
    const uint64_t clip=j->clip_frames;
    for(uint64_t f=0;f<clip;++f){
        for(unsigned c=0;c<2;++c){
            const float v=s->pcm[(size_t)(r->first+f)*s->channels+(s->channels==1?0:c)];
            if(!isfinite(v)){pad_render_destroy(j);if(error)*error=PAD_RENDER_AUDIO_ERROR;return NULL;}
            j->original[(size_t)f*2+c]=v;
        }
    }
    j->captured=clip;return j;
}
struct pad_render_job *pad_render_create_stream(const struct pad_render_stream *s,
    const struct pad_render_request *r,const struct fx_backend *backend,enum pad_render_status *error)
{
    if(!s||!s->read||!s->close){if(error)*error=PAD_RENDER_INVALID;return NULL;}
    struct pad_render_job*j=allocate_job(s->frames,s->rate,r,backend,error);if(!j)return NULL;
    j->stream=*s;return j;
}
enum pad_render_status pad_render_step(struct pad_render_job *j,size_t budget)
{
    if(!j)return PAD_RENDER_INVALID;
    if(j->status!=PAD_RENDER_WORKING)return j->status;
    if(!budget)return PAD_RENDER_INVALID;
    if(budget>PAD_RENDER_MAX_STEP_FRAMES)budget=PAD_RENDER_MAX_STEP_FRAMES;
    while(budget&&j->captured<j->clip_frames){
        if(atomic_load_explicit(&j->cancel,memory_order_acquire)){discard(j,PAD_RENDER_CANCELLED);return j->status;}
        size_t n=budget<PAD_RENDER_BLOCK_FRAMES?budget:PAD_RENDER_BLOCK_FRAMES;
        if(n>j->clip_frames-j->captured)n=(size_t)(j->clip_frames-j->captured);
        float*dst=j->original+(size_t)j->captured*2;
        if(!j->stream.read(j->stream.user,j->capture_first+j->captured,n,dst)){discard(j,PAD_RENDER_AUDIO_ERROR);return j->status;}
        for(size_t i=0;i<n*2;++i)if(!isfinite(dst[i])){discard(j,PAD_RENDER_AUDIO_ERROR);return j->status;}
        j->captured+=n;budget-=n;
        if(j->captured==j->clip_frames)close_stream(j);
    }
    while(budget&&j->done<j->total){
        if(atomic_load_explicit(&j->cancel,memory_order_acquire)){
            discard(j,PAD_RENDER_CANCELLED);return j->status;
        }
        size_t n=budget<PAD_RENDER_BLOCK_FRAMES?budget:PAD_RENDER_BLOCK_FRAMES;
        if(n>j->total-j->done)n=(size_t)(j->total-j->done);
        float *block=j->output+(size_t)j->done*2;
        for(size_t f=0;f<n;++f){
            uint64_t at=j->done+f;
            if(at<j->body_frames){
                uint64_t source=at%j->clip_frames;
                block[f*2]=j->original[(size_t)source*2];
                block[f*2+1]=j->original[(size_t)source*2+1];
            }else block[f*2]=block[f*2+1]=0.f;
        }
        fx_program_process(j->fx,block,n);
        for(size_t f=0;f<n;++f){
            double left=block[f*2],right=block[f*2+1];
            if(!isfinite(left)||!isfinite(right)){discard(j,PAD_RENDER_AUDIO_ERROR);return j->status;}
            double peak=fmax(fabs(left),fabs(right));
            if(peak>j->peak)j->peak=peak;
            if(peak>1.)++j->over_unity;
        }
        j->done+=n;budget-=n;
    }
    if(atomic_load_explicit(&j->cancel,memory_order_acquire))discard(j,PAD_RENDER_CANCELLED);
    else if(j->done==j->total){
        /* No more DSP needed. Keep only completed audio for later A/B/Apply. */
        fx_program_destroy(j->fx);j->fx=NULL;
        free(j->original);j->original=NULL;j->status=PAD_RENDER_DONE;
    }
    return j->status;
}
void pad_render_cancel(struct pad_render_job *j)
{
    if(j)atomic_store_explicit(&j->cancel,1,memory_order_release);
}
void pad_render_progress(const struct pad_render_job *j,struct pad_render_progress *out)
{
    if(!out)return;
    if(!j){memset(out,0,sizeof(*out));out->status=PAD_RENDER_INVALID;return;}
    *out=(struct pad_render_progress){j->status,j->done,j->total,j->peak,j->over_unity,j->captured,j->clip_frames};
}
const float *pad_render_output(const struct pad_render_job *j,uint64_t *frames,double *rate)
{
    if(frames)*frames=0;
    if(rate)*rate=0;
    if(!j||j->status!=PAD_RENDER_DONE)return NULL;
    if(frames)*frames=j->total;
    if(rate)*rate=j->rate;
    return j->output;
}
