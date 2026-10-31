#include "resample_worker.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#define CAPACITY 16
struct resample_worker {
    pthread_mutex_t mutex;
    struct resample_command queue[CAPACITY];unsigned read,count;
    uint64_t serial,token;
    struct resample_worker_event event;
    struct resample_worker_backend backend;
    struct pad_sample_selection source;
    struct pad_render_job *job;
    struct resample_spec spec;
    unsigned owned,rendering,mixing,failed;
    unsigned pending,wanted_mix,cancel_requested;
    unsigned apply_cancelled;
    unsigned terminal;
    struct resample_spec apply_spec;
};
enum { P_COMPARE=1,P_RESTORE_CANCEL,P_RESTORE_APPLY,P_APPLY };
static void publish(struct resample_worker*w,enum resample_reply reply,const char*error){
    if(reply==RS_REPLY_APPLIED||reply==RS_REPLY_CANCELLED)w->terminal=1;
    struct resample_worker_event e={0};e.update.reply=reply;e.update.token=w->token;e.update.revision=w->spec.revision;
    e.update.rate=w->source.stream.rate;e.update.capture_total=w->source.end-w->source.first;e.update.rendered=w->mixing;
    if(w->job){struct pad_render_progress p;pad_render_progress(w->job,&p);e.update.captured=p.source_done;e.update.capture_total=p.source_total;e.update.done=p.done;e.update.total=p.total;e.update.peak=p.peak;}
    snprintf(e.error,sizeof(e.error),"%s",error?error:"");
    pthread_mutex_lock(&w->mutex);w->event=e;++w->serial;pthread_mutex_unlock(&w->mutex);
}
static void clear(struct resample_worker*w){
    pad_render_destroy(w->job);w->job=NULL;
    if(w->source.stream.close)w->source.stream.close(w->source.stream.user);
    memset(&w->source,0,sizeof(w->source));w->owned=w->rendering=w->failed=w->mixing=w->cancel_requested=w->apply_cancelled=0;
}
struct resample_worker*resample_worker_create(const struct resample_worker_backend*b){
    if(!b||!b->open)return NULL;
    struct resample_worker*w=calloc(1,sizeof(*w));if(!w)return NULL;
    if(pthread_mutex_init(&w->mutex,NULL)){free(w);return NULL;}w->backend=*b;return w;
}
int resample_worker_send(void*p,const struct resample_command*c){
    struct resample_worker*w=p;
    if(!w||!c||!c->token||c->spec.source>=128||c->spec.destination>=128||c->action<RS_ACT_PREPARE||c->action>RS_ACT_RELEASE_STALE)return 0;
    pthread_mutex_lock(&w->mutex);int ok=w->count<CAPACITY;
    if(ok){w->queue[(w->read+w->count)%CAPACITY]=*c;++w->count;}
    pthread_mutex_unlock(&w->mutex);return ok;
}
int resample_worker_poll(struct resample_worker*w,uint64_t*after,struct resample_worker_event*out){
    if(!w||!after||!out)return 0;
    pthread_mutex_lock(&w->mutex);int fresh=w->serial!=*after;
    if(fresh){*out=w->event;*after=w->serial;out->update.error=out->error[0]?out->error:NULL;}
    pthread_mutex_unlock(&w->mutex);return fresh;
}
static void fail(struct resample_worker*w,const char*error){w->rendering=0;w->failed=1;publish(w,RS_REPLY_FAILED,error);}
static void cancel(struct resample_worker*);
static void begin_apply(struct resample_worker*);
static void completed(struct resample_worker*w,unsigned operation,int result){
    if(result==2){w->pending=operation;return;}
    w->pending=0;
    if(operation==P_COMPARE){
        if(result==1)w->mixing=w->wanted_mix;
        if(w->cancel_requested){w->cancel_requested=0;cancel(w);}
        else if(result==1)publish(w,RS_REPLY_COMPARED,NULL);
        else fail(w,"Cannot switch mix source");
    }else if(operation==P_RESTORE_CANCEL||operation==P_RESTORE_APPLY){
        if(result!=1){w->cancel_requested=0;fail(w,"Restore original failed: retry Cancel");return;}
        w->mixing=0;
        if(operation==P_RESTORE_CANCEL||w->cancel_requested){clear(w);publish(w,RS_REPLY_CANCELLED,NULL);}
        else begin_apply(w);
    }else if(operation==P_APPLY){
        /* Once a native commit has started, its final acknowledgement wins
         * over a late cancellation. A pending native reader keeps its PCM. */
        if(result==1){clear(w);publish(w,RS_REPLY_APPLIED,NULL);}
        else if(w->apply_cancelled){clear(w);publish(w,RS_REPLY_CANCELLED,NULL);}
        else fail(w,"Apply failed; check recovery state");
    }
}
static void cancel(struct resample_worker*w){
    if(w->pending){
        w->cancel_requested=1;
        if(w->pending==P_APPLY&&!w->apply_cancelled&&w->backend.cancel_apply)
            w->apply_cancelled=w->backend.cancel_apply(w->backend.user)==1;
        return;
    }
    if(!w->mixing){clear(w);publish(w,RS_REPLY_CANCELLED,NULL);return;}
    if(!w->backend.compare){fail(w,"Cannot restore original");return;}
    completed(w,P_RESTORE_CANCEL,w->backend.compare(w->backend.user,w->spec.source,NULL,0,w->source.stream.rate,0));
}
static void begin_apply(struct resample_worker*w){
    uint64_t frames=0;double rate=0;const float*pcm=pad_render_output(w->job,&frames,&rate);
    completed(w,P_APPLY,w->backend.apply(w->backend.user,&w->apply_spec,pcm,frames,rate));
}
static void command(struct resample_worker*w,const struct resample_command*c){
    if(c->action==RS_ACT_PREPARE){
        if(w->owned)return; /* Never replace a live/borrowed source. */
        if(w->terminal&&c->token==w->token)return;
        w->terminal=0;
        w->token=c->token;w->spec=c->spec;
        if(!w->backend.open(w->backend.user,c->spec.source,&w->source)){
            if(w->source.stream.close)w->source.stream.close(w->source.stream.user);
            memset(&w->source,0,sizeof(w->source));fail(w,"Cannot read selected sample");return;
        }
        w->owned=1;
        if(!w->source.stream.close||!w->source.stream.read||w->source.first>=w->source.end||w->source.end>w->source.stream.frames||
            !fx_chain_valid(&c->spec.fx,w->source.stream.rate)){fail(w,"Invalid source snapshot");return;}
        publish(w,RS_REPLY_PREPARED,NULL);return;
    }
    if(c->token!=w->token)return; /* A stale cleanup cannot touch a new job. */
    if(w->terminal)return; /* Late Cancel must not overwrite an Apply ack. */
    if(c->action==RS_ACT_CANCEL||c->action==RS_ACT_RELEASE_STALE){
        cancel(w);return;
    }
    if(w->pending)return;
    if(!w->owned||c->spec.source!=w->spec.source)return;
    if(c->action==RS_ACT_RENDER){
        if(w->job||!w->source.stream.read||w->failed){fail(w,"Source already used: reopen sample");return;}
        w->spec=c->spec;
        /* A velocity multiply is not the instrument's velocity curve. Do
         * not silently claim this raw-PCM prototype bakes that stage. */
        if(c->spec.velocity!=127){fail(w,"Pad velocity processing not connected");return;}
        if(!c->spec.repeats||c->spec.repeats>64||c->spec.tail_ms>10000){fail(w,"Invalid render length");return;}
        struct pad_render_request r={.first=w->source.first,.end=w->source.end,.repeats=c->spec.repeats,
            .tail_frames=(uint64_t)(w->source.stream.rate*c->spec.tail_ms/1000.),.chain=c->spec.fx};
        enum pad_render_status error;
        w->job=pad_render_create_stream(&w->source.stream,&r,&w->backend.fx,&error);
        if(!w->job){
            const char*message=error==PAD_RENDER_LIMIT?"Sample exceeds PCM budget":
                error==PAD_RENDER_FX_ERROR?"Cannot initialize FX":"Cannot allocate render buffers";
            if(error==PAD_RENDER_FX_ERROR&&(!w->backend.fx.create||!w->backend.fx.destroy||!w->backend.fx.process))
                for(unsigned i=0;i<c->spec.fx.count;++i)if(c->spec.fx.stage[i].kind==FX_STAGE_NATIVE){message="Native FX unavailable";break;}
            fail(w,message);return;
        }
        /* The job now owns the reader; keep numeric metadata for replies. */
        w->source.stream.close=NULL;w->source.stream.read=NULL;w->source.stream.user=NULL;
        w->rendering=1;return;
    }
    if(c->spec.revision!=w->spec.revision||w->failed)return;
    uint64_t frames=0;double rate=0;const float*pcm=pad_render_output(w->job,&frames,&rate);
    if(!pcm){fail(w,"Render not complete");return;}
    if(c->action==RS_ACT_COMPARE){
        if(!w->backend.compare){fail(w,"Mix comparison not connected");return;}
        w->wanted_mix=!!c->rendered;
        completed(w,P_COMPARE,w->backend.compare(w->backend.user,w->spec.source,pcm,frames,rate,!!c->rendered));
    }else if(c->action==RS_ACT_APPLY){
        if(!w->backend.apply){fail(w,"Native Apply/Undo not connected");return;}
        w->apply_spec=c->spec;
        if(w->mixing)completed(w,P_RESTORE_APPLY,w->backend.compare(w->backend.user,w->spec.source,NULL,0,rate,0));
        else begin_apply(w);
    }
}
void resample_worker_step(struct resample_worker*w){
    if(!w)return;
    struct resample_command c;unsigned found=0;
    pthread_mutex_lock(&w->mutex);
    if(w->count){c=w->queue[w->read];w->read=(w->read+1)%CAPACITY;--w->count;found=1;}
    pthread_mutex_unlock(&w->mutex);
    if(found)command(w,&c);
    if(w->pending&&w->backend.poll){
        unsigned op=w->pending;
        unsigned native=op==P_COMPARE?1:op==P_APPLY?3:2;
        completed(w,op,w->backend.poll(w->backend.user,native));
    }
    if(w->rendering){
        enum pad_render_status result=pad_render_step(w->job,PAD_RENDER_MAX_STEP_FRAMES);
        if(result==PAD_RENDER_DONE){w->rendering=0;publish(w,RS_REPLY_RENDERED,NULL);}
        else if(result!=PAD_RENDER_WORKING)fail(w,result==PAD_RENDER_CANCELLED?"Render cancelled":"Audio capture/processing failed");
        else publish(w,RS_REPLY_PROGRESS,NULL);
    }
}
int resample_worker_destroy(struct resample_worker*w){
    if(!w)return 1;
    if(w->pending)return 0;
    if(w->mixing){cancel(w);if(w->mixing||w->pending)return 0;}
    clear(w);
    pthread_mutex_destroy(&w->mutex);free(w);return 1;
}
int resample_worker_work_pending(struct resample_worker*w){
    if(!w)return 0;
    pthread_mutex_lock(&w->mutex);unsigned count=w->count;pthread_mutex_unlock(&w->mutex);
    return count||w->pending||w->rendering;
}
