#include "resample_stock_backend.h"
#include "resample_stock.h"
#include "resample_queue.h"
#ifdef MPC_PAD_EQ
#include "pad_eq_native.h"
#include "music_settings.h"
#endif
#include <atomic>
#include <mutex>
#include <new>
#include <cstring>
#include <cstdio>
#define F(p,o,T) (*reinterpret_cast<T*>(reinterpret_cast<char*>(p)+(o)))
extern "C" void pad_source_layer_sample(void*,int,uintptr_t*,uintptr_t);
struct resample_stock_backend {
    uintptr_t base;void*copy;resample_queue*queue=nullptr;
    std::atomic<bool>live{true},busy{false};std::atomic<uint64_t>epoch{1};
    std::mutex mutex;unsigned command=0;resample_destination_request request{};resample_stock_reply reply{};
    void *mode=nullptr,*things=nullptr,*project=nullptr,*program=nullptr,*sequence=nullptr;
    struct Job {std::atomic<unsigned>done{0};uint64_t token,epoch;unsigned target;uintptr_t before;bool submitted=false;}*job=nullptr;
};
static void publish(resample_stock_backend*b,unsigned state,uint64_t token,const char*error=nullptr,const resample_stock_snapshot*s=nullptr){
    std::lock_guard<std::mutex>lock(b->mutex);b->reply={};b->reply.status=state;b->reply.token=token;
    if(error)std::snprintf(b->reply.error,sizeof(b->reply.error),"%s",error);
    if(s)b->reply.snapshot=*s;
}
static void*mode_find(resample_stock_backend*b){
    void*registry=F(b->copy,0x98,void*);if(!registry)return nullptr;
    void*stack[32];unsigned count=0,seen=0;void*node=F(registry,0x168,void*);
    while(node||count){
        while(node){if(count==32||++seen>256)return nullptr;stack[count++]=node;node=F(node,0x10,void*);}
        node=stack[--count];void*value=F(node,0x28,void*);
        if(value&&F(value,0,uintptr_t)==b->base+0x6e10598&&F(value,0x48,uintptr_t)==b->base+0x6e105f0)return value;
        node=F(node,0x18,void*);
    }return nullptr;
}
static uintptr_t sample_at(resample_stock_backend*b,unsigned pad,char*name=nullptr){
    void*instrument=reinterpret_cast<void*(*)(void*,int)>(b->base+0x286bbe0)(b->program,(int)pad);
    if(!instrument||F(instrument,0x2310,void*)!=b->program)return 0;
    uintptr_t first=0;
    for(unsigned layer=0;layer<8;++layer){uintptr_t pair[2]{};pad_source_layer_sample(instrument,(int)layer,pair,b->base+0x2803b90);
        if(pair[0]&&!first){first=pair[0];if(name){const char*s=F(reinterpret_cast<void*>(pair[0]),0xa8,const char*);if(s)std::snprintf(name,80,"%s",s);}}
        if(pair[1])reinterpret_cast<void(*)(void*)>(b->base+0x123bca0)(reinterpret_cast<void*>(pair[1]));
    }return first;
}
static bool model(resample_stock_backend*b){
    if(!b->live.load(std::memory_order_acquire))return false;
    auto type=F(b->copy,0,uintptr_t);if(type!=b->base+0x6d8cd88&&type!=b->base+0x6e0dfd8)return false;
    return F(b->copy,0x78,void*)==b->things&&F(b->copy,0xc8,void*)==b->project&&
        b->things&&b->project&&F(b->things,0xb8,void*)==b->program&&F(b->things,0x1c8,void*)==b->sequence;
}
static void done(void*user,int invoked){static_cast<resample_stock_backend::Job*>(user)->done.store(invoked?2:1,std::memory_order_release);}
static int run_tick(void*user){
    auto*b=static_cast<resample_stock_backend*>(user);if(!b->live.load(std::memory_order_acquire))return 0;
    unsigned command;resample_destination_request r{};{std::lock_guard<std::mutex>lock(b->mutex);command=b->command;b->command=0;r=b->request;}
    if(b->job&&b->job->done.load(std::memory_order_acquire)){
        auto*j=b->job;bool valid=j->epoch==b->epoch.load()&&model(b);
        bool applied=valid&&j->done.load()==2&&sample_at(b,j->target)&&sample_at(b,j->target)!=j->before;
        const char*failure=valid?resample_stock_error():nullptr;
        publish(b,applied?RSB_APPLIED:RSB_ERROR,j->token,applied?nullptr:valid?(failure?failure:"Native render did not assign a sample"):"Project changed during resampling");
        /* Tiny callback tokens live until cloned-process exit. Native task
         * copies may be released on another thread after terminal delivery. */
        b->job=nullptr;b->busy.store(false,std::memory_order_release);return 0;
    }
    if(command==1||command==4){
        b->things=F(b->copy,0x78,void*);b->project=F(b->copy,0xc8,void*);
        b->program=b->things?F(b->things,0xb8,void*):nullptr;b->sequence=b->things?F(b->things,0x1c8,void*):nullptr;
        if(!b->program||!model(b)){if(command==1)publish(b,RSB_ERROR,0,"Sample not ready: try again");return 0;}
#ifdef MPC_PAD_EQ
        pad_eq_native_program(b->program);
#endif
        if(command==4)return 0;
        b->mode=mode_find(b);
        if(!b->mode){std::fprintf(stderr,"RESAMPLE CONTEXT_ERROR stage=mode\n");publish(b,RSB_ERROR,0,"Native resample mode unavailable");return 0;}
        if(!resample_stock_install(b->base)){publish(b,RSB_ERROR,0,"Resample not ready: stop and retry");return 0;}
        unsigned source=F(b->things,0x5f8,unsigned);if(source>=128){publish(b,RSB_ERROR,0,"Select a source pad");return 0;}
        resample_stock_snapshot s{};s.epoch=b->epoch.load();s.source=source;s.has_sequence=b->sequence!=nullptr;
        s.has_sample=sample_at(b,source,s.sample)!=0;
        for(unsigned pad=0;pad<128;++pad)if(sample_at(b,pad))s.occupied[pad/64]|=UINT64_C(1)<<(pad%64);
        publish(b,RSB_CAPTURED,0,nullptr,&s);return 0;
    }
    if(command==3){if(!b->job){b->busy.store(false);publish(b,RSB_CANCELLED,r.token);}return 0;}
    if(command!=2||b->job)return 0;
    if(r.source_epoch!=b->epoch.load()||!model(b)||r.target_pad>=128||r.source_pad>=128||
       (sample_at(b,r.target_pad)&&!r.overwrite_confirmed)||F(b->things,0x5f8,unsigned)!=r.source_pad){
        b->busy.store(false);publish(b,RSB_ERROR,r.token,"Project or destination changed; reopen Resample");return 0;}
    auto*j=new(std::nothrow)resample_stock_backend::Job{};if(!j){b->busy.store(false);publish(b,RSB_ERROR,r.token,"Cannot allocate render request");return 0;}
    j->token=r.token;j->epoch=r.source_epoch;j->target=r.target_pad;j->before=sample_at(b,r.target_pad);b->job=j;
    char source_name[80]={};if(r.kind==RESAMPLE_SOUND)sample_at(b,r.source_pad,source_name);
    bool started=false;try{started=resample_stock_start_named_observed(b->base,b->mode,r.kind==RESAMPLE_SOUND,r.source_pad,r.target_pad,source_name,done,j);}catch(...){publish(b,RSB_ERROR,r.token,"Native resample setup failed");}
    if(started){j->submitted=true;publish(b,RSB_SUBMITTED,r.token);std::fprintf(stderr,"RESAMPLE STOCK_SUBMITTED kind=%u source=%u target=%u\n",r.kind,r.source_pad,r.target_pad);}
    else if(!j->done.load()){
        /* No callback was installed: no task acquired this request. */
        b->job=nullptr;b->busy.store(false);publish(b,RSB_ERROR,r.token,"Native resample start refused");
    }return 0;
}
static int tick(void*user){
    auto*b=static_cast<resample_stock_backend*>(user);
    try{return run_tick(user);}catch(...){
        publish(b,RSB_ERROR,b->request.token,"Native resample operation failed");
        if(!b->job)b->busy.store(false,std::memory_order_release);
        return 0;
    }
}
extern "C" resample_stock_backend*resample_stock_backend_create(uintptr_t base,void*copy){
    if(!base||!copy)return nullptr;
    auto*b=new(std::nothrow)resample_stock_backend{};if(!b)return nullptr;b->base=base;b->copy=copy;
    b->queue=resample_queue_create(base,copy,tick,b);if(!b->queue){delete b;return nullptr;}return b;
}
extern "C" void resample_stock_backend_stop(resample_stock_backend*b){if(b){b->live.store(false,std::memory_order_release);b->epoch.fetch_add(1);resample_queue_stop(b->queue);}}
extern "C" void resample_stock_backend_changed(resample_stock_backend*b){if(b)b->epoch.fetch_add(1,std::memory_order_acq_rel);
#ifdef MPC_PAD_EQ
    pad_eq_native_clear();
#endif
}
extern "C" int resample_stock_backend_capture(resample_stock_backend*b){
    if(!b||!b->live.load()||b->busy.load())return 0;
    {std::lock_guard<std::mutex>lock(b->mutex);b->command=1;b->reply={};}return resample_queue_wake(b->queue);
}
extern "C" int resample_stock_backend_bind_eq(resample_stock_backend*b){
    if(!b||!b->live.load()||b->busy.load())return 0;
    {std::lock_guard<std::mutex>lock(b->mutex);if(b->command)return 0;b->command=4;}
    return resample_queue_wake(b->queue);
}
extern "C" int resample_stock_backend_start(void*user,const resample_destination_request*r){
    auto*b=static_cast<resample_stock_backend*>(user);if(!b||!r||!b->live.load()||b->busy.exchange(true))return 0;
    {std::lock_guard<std::mutex>lock(b->mutex);b->request=*r;b->command=2;}
    if(resample_queue_wake(b->queue))return 1;
    {std::lock_guard<std::mutex>lock(b->mutex);b->command=0;}b->busy.store(false);return 0;
}
extern "C" int resample_stock_backend_cancel(void*user,uint64_t token){
    auto*b=static_cast<resample_stock_backend*>(user);if(!b||!b->live.load())return 0;
    {std::lock_guard<std::mutex>lock(b->mutex);if(b->command==2&&b->request.token==token)b->command=3;else return 0;}
    return resample_queue_wake(b->queue);
}
extern "C" int resample_stock_backend_poll(resample_stock_backend*b,resample_stock_reply*out){
    if(!b||!out||!b->live.load())return 0;
    if(b->busy.load())resample_queue_wake(b->queue);
    std::lock_guard<std::mutex>lock(b->mutex);if(!b->reply.status)return 0;*out=b->reply;b->reply={};return 1;
}
extern "C" int resample_stock_backend_busy(resample_stock_backend*b){return b&&b->busy.load(std::memory_order_acquire);}
