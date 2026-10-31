#include "resample_native.h"
#include "pad_assignment.h"
#include <new>
#include <cstring>
#include <mutex>
#define F(p,o,T) (*reinterpret_cast<T*>(reinterpret_cast<char*>(p)+(o)))
extern "C" void pad_source_layer_sample(void*,int,uintptr_t*,uintptr_t);
struct resample_native {
    resample_native_config config;
    pad_memory_job *memory=nullptr;
    uintptr_t command[2]{};
    unsigned phase=0,borrows=0,source=128;
    bool cancel=false;
    uint64_t done=0,total=0;
    std::mutex progress_mutex;
    resample_spec spec{};
    uintptr_t source_sample[2]{};
    void *source_instrument=nullptr;
    uint64_t first=0,end=0;
    pad_memory_services services{};
    unsigned target=128;
    void *target_instrument=nullptr;
    uintptr_t target_samples[8][2]{};
    int target_notes[8]{};
    int64_t target_trim[8][2]{};
};
static bool valid_context(resample_native*n){auto&c=n->config.commit;
    return c.validate(c.user,PAD_COMMIT_APPLY)&&F(c.properties,0x1278,void*)==c.pool&&F(n->config.manager,0,uintptr_t)==c.base+0x6d97818;
}
static bool valid(resample_native*n){
    if(!valid_context(n))return false;
    auto&c=n->config.commit;
    void*instrument=reinterpret_cast<void*(*)(void*,int)>(c.base+0x286bbe0)(c.program,(int)n->source);
    if(!instrument||instrument!=n->source_instrument||F(instrument,0x2310,void*)!=c.program)return false;
    void*layer=F(instrument,0x22e8,void*);if(!layer||F(layer,0x7b8,uint64_t)!=n->first||F(layer,0x810,uint64_t)!=n->end)return false;
    uintptr_t sample[2]{};pad_source_layer_sample(instrument,0,sample,c.base+0x2803b90);
    bool ok=sample[0]==n->source_sample[0]&&sample[1]==n->source_sample[1];pad_assignment_release(c.base,sample);return ok;
}
static int open_source(void*p,unsigned source,pad_sample_selection*out){
    auto*n=static_cast<resample_native*>(p);if(n->phase||!valid_context(n))return 0;
    auto&c=n->config.commit;void*instrument=reinterpret_cast<void*(*)(void*,int)>(c.base+0x286bbe0)(c.program,(int)source);
    if(!instrument||F(instrument,0x2310,void*)!=c.program)return 0;
    uintptr_t sample[2]{};pad_source_layer_sample(instrument,0,sample,c.base+0x2803b90);
    pad_memory_services services{};
    bool ok=pad_memory_services_from_sample(c.base,sample,&services)!=0;
    auto&expected=n->config.memory;
    if(expected.queue_context||expected.storage||expected.resources)
        ok=ok&&expected.queue_context==services.queue_context&&expected.storage==services.storage&&expected.resources==services.resources;
    if(!ok){pad_assignment_release(c.base,sample);return 0;}
    pad_assignment_release(c.base,n->source_sample);n->source_sample[0]=sample[0];n->source_sample[1]=sample[1];n->services=services;
    if(pad_sample_stream_open_pad(n->config.commit.base,n->config.commit.program,source,out)!=PAD_RENDER_WORKING)return 0;
    n->source=source;n->source_instrument=instrument;n->first=out->first;n->end=out->end;return 1;
}
static int retain_output(void*p){auto*n=static_cast<resample_native*>(p);++n->borrows;return 1;}
static void release_output(void*p){auto*n=static_cast<resample_native*>(p);--n->borrows;}
static void release_target(resample_native*n){
    for(auto&p:n->target_samples)pad_assignment_release(n->config.commit.base,p);
    n->target=128;n->target_instrument=nullptr;
}
static void*target_layers(resample_native*n,unsigned target){
    auto&c=n->config.commit;
    void*instrument=reinterpret_cast<void*(*)(void*,int)>(c.base+0x286bbe0)(c.program,(int)target);
    if(!instrument||F(instrument,0x2310,void*)!=c.program)return nullptr;
    void*layers=F(instrument,0x22e8,void*),*end=F(instrument,0x22f0,void*);
    if(!layers||reinterpret_cast<uintptr_t>(end)-reinterpret_cast<uintptr_t>(layers)!=8*0xb30)return nullptr;
    if(n->target==128)n->target_instrument=instrument;
    else if(n->target_instrument!=instrument)return nullptr;
    return layers;
}
static bool capture_target(resample_native*n,unsigned target){
    release_target(n);void*layers=target_layers(n,target);if(!layers)return false;
    n->target=target;
    for(unsigned i=0;i<8;++i){void*layer=static_cast<char*>(layers)+i*0xb30;
        pad_source_layer_sample(n->target_instrument,(int)i,n->target_samples[i],n->config.commit.base+0x2803b90);
        n->target_notes[i]=F(layer,0x418,int);n->target_trim[i][0]=F(layer,0x7b8,int64_t);n->target_trim[i][1]=F(layer,0x810,int64_t);
    }
    return true;
}
static bool target_matches(resample_native*n){
    if(n->target>=128)return false;
    void*layers=target_layers(n,n->target);if(!layers)return false;
    for(unsigned i=0;i<8;++i){void*layer=static_cast<char*>(layers)+i*0xb30;uintptr_t sample[2]{};
        pad_source_layer_sample(n->target_instrument,(int)i,sample,n->config.commit.base+0x2803b90);
        bool ok=sample[0]==n->target_samples[i][0]&&sample[1]==n->target_samples[i][1]&&
            n->target_notes[i]==F(layer,0x418,int)&&n->target_trim[i][0]==F(layer,0x7b8,int64_t)&&n->target_trim[i][1]==F(layer,0x810,int64_t);
        pad_assignment_release(n->config.commit.base,sample);if(!ok)return false;
    }
    return true;
}
static void clean(resample_native*n){
    pad_memory_destroy(n->memory);n->memory=nullptr;
    pad_assignment_release(n->config.commit.base,n->command);release_target(n);n->phase=0;n->cancel=false;
}
static int begin_apply(void*p,const resample_spec*spec,const float*pcm,uint64_t frames,double rate){
    auto*n=static_cast<resample_native*>(p);
    if(n->phase||!spec||spec->source!=n->source||!valid(n))return 0;
    if(!capture_target(n,spec->destination)){release_target(n);return 0;}
    pad_output_reference audio{n,pcm,frames,rate,retain_output,release_output};pad_render_status error;
    n->memory=pad_memory_begin(n->config.commit.base,&n->services,&audio,"Resampled",0,&error);
    if(!n->memory){release_target(n);return 0;}
    n->phase=1;
    {std::lock_guard<std::mutex> lock(n->progress_mutex);n->done=0;n->total=frames;}
    /* Spec contains no borrowed pointers; snapshot for the later commit. */
    n->spec=*spec;
    return 2;
}
static int poll(void*p,unsigned operation){
    auto*n=static_cast<resample_native*>(p);if(operation!=3||!n->phase)return 0;
    try{
        if(n->cancel||!valid(n)||!target_matches(n)){clean(n);return 0;}
        if(n->phase==1){
            auto result=pad_memory_step(n->memory,PAD_RENDER_MAX_STEP_FRAMES);
            uint64_t done,total;pad_memory_progress(n->memory,&done,&total);
            {std::lock_guard<std::mutex> lock(n->progress_mutex);n->done=done;n->total=total;}
            if(result==PAD_RENDER_WORKING)return 2;
            if(result!=PAD_RENDER_DONE){clean(n);return 0;}
            uintptr_t sample[2]{};
            if(!pad_memory_take(n->memory,sample)){clean(n);return 0;}
            int prepared=pad_history_prepare(&n->config.commit,sample,n->spec.destination,n->spec.overwrite_confirmed,n->config.hold_recovery,n->command);
            pad_assignment_release(n->config.commit.base,sample);
            pad_memory_destroy(n->memory);n->memory=nullptr;
            if(!prepared){clean(n);return 0;}
            n->phase=2;return 2; /* Cancellation remains possible before commit. */
        }
        int result=reinterpret_cast<int(*)(void*,const uintptr_t*)>(n->config.commit.base+0x19d0a00)(n->config.manager,n->command)==0;
        clean(n);return result;
    }catch(...){clean(n);return 0;}
}
static int cancel_apply(void*p){auto*n=static_cast<resample_native*>(p);if(!n->phase)return 0;n->cancel=true;return 1;}
extern "C" resample_native *resample_native_create(const resample_native_config*c){
    if(!c||!c->manager||!c->hold_recovery||!c->commit.base||!c->commit.properties||!c->commit.program||!c->commit.service||
       !c->commit.pool||!c->commit.validate||!c->commit.retain||!c->commit.release)return nullptr;
    auto*n=new(std::nothrow)resample_native{};if(!n)return nullptr;
    n->config=*c;
    if(!c->commit.retain(c->commit.user)){delete n;return nullptr;}
    return n;
}
extern "C" void resample_native_backend(resample_native*n,resample_worker_backend*out){
    if(!out)return;
    std::memset(out,0,sizeof(*out));if(!n)return;
    out->user=n;out->open=open_source;out->apply=begin_apply;out->poll=poll;out->cancel_apply=cancel_apply;out->fx=n->config.fx;
}
extern "C" int resample_native_destroy(resample_native*n){
    if(!n)return 1;
    if(n->phase||n->borrows)return 0;
    auto c=n->config.commit;release_target(n);pad_assignment_release(c.base,n->source_sample);delete n;c.release(c.user);return 1;
}
extern "C" void resample_native_progress(resample_native*n,uint64_t*d,uint64_t*t){
    if(!n){if(d)*d=0;if(t)*t=0;return;}
    std::lock_guard<std::mutex> lock(n->progress_mutex);if(d)*d=n->done;if(t)*t=n->total;
}
