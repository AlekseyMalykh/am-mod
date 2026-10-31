#include "resample_context.h"
#include "pad_assignment.h"
#include "pad_sample_pool.h"
#include "fx_native.h"
#include <atomic>
#include <mutex>
#include <new>
#include <cstring>
#include <cstdio>
#include <limits>
#define F(p,o,T) (*reinterpret_cast<T*>(reinterpret_cast<char*>(p)+(o)))
extern "C" void pad_source_layer_sample(void*,int,uintptr_t*,uintptr_t);
struct resample_context {
    std::atomic<unsigned> references{1};
    std::atomic<uint64_t> epoch{1};
    std::atomic<bool> live{true};
    uintptr_t base;void *context;
    std::atomic<unsigned> recovery_count{0};
    std::atomic<unsigned> capture_error{0};
    std::mutex hook_mutex;
    resample_context_hook *hooks=nullptr;
};
enum CaptureError { CAP_NONE,CAP_STOPPED,CAP_RESOURCE,CAP_SERVICES,CAP_MODEL,CAP_INSTRUMENT,CAP_PAD,
    CAP_EMPTY,CAP_SAMPLE_SERVICES,CAP_POOL_MEMBER,CAP_NAME,CAP_OTHER_PAD,CAP_CHANGED,CAP_EXCEPTION };
static const char*const capture_errors[]={"Sample context unavailable","Sample context stopped","Capture allocation failed",
    "Project services unavailable","Project library unavailable","Selected pad unavailable","Selected pad not in program",
    "Selected pad is empty","Sample services unavailable","Sample absent from project pool","Sample name unavailable",
    "Pad table does not match project","Project changed while opening","Capture failed unexpectedly"};
extern "C" const char*resample_context_error(resample_context*c){
    unsigned error=c?c->capture_error.load(std::memory_order_acquire):(unsigned)CAP_STOPPED;
    return error<sizeof(capture_errors)/sizeof(*capture_errors)?capture_errors[error]:capture_errors[CAP_EXCEPTION];
}
struct Capture {
    std::atomic<unsigned> references{1};
    resample_context *owner;
    uint64_t epoch;
    void *things,*program,*properties,*pool,*manager,*service;
    fx_native_context native_fx{};
};
static bool retain_context(resample_context*c){
    unsigned count=c->references.load(std::memory_order_relaxed);
    while(count&&count<std::numeric_limits<unsigned>::max())
        if(c->references.compare_exchange_weak(count,count+1,std::memory_order_acquire))return true;
    return false;
}
extern "C" int resample_context_retain(resample_context*c){return c&&retain_context(c);}
extern "C" void resample_context_release(resample_context*c){if(c&&c->references.fetch_sub(1,std::memory_order_acq_rel)==1)delete c;}
static int retain_capture(void*p){
    auto*c=static_cast<Capture*>(p);unsigned count=c->references.load(std::memory_order_relaxed);
    while(count&&count<std::numeric_limits<unsigned>::max())
        if(c->references.compare_exchange_weak(count,count+1,std::memory_order_acquire))return 1;
    return 0;
}
static void release_capture(void*p){
    auto*c=static_cast<Capture*>(p);if(c->references.fetch_sub(1,std::memory_order_acq_rel)==1){resample_context_release(c->owner);delete c;}
}
static bool context_ready(resample_context*c){
    if(!c->live.load(std::memory_order_acquire))return false;
    uintptr_t v=F(c->context,0,uintptr_t);
    return v==c->base+0x6d8cd88||v==c->base+0x6e0dfd8;
}
static int validate_capture(void*p,pad_commit_operation){
    auto*s=static_cast<Capture*>(p);auto*c=s->owner;
    /* Lifecycle flags are checked before touching cached native pointers.
     * Caller still serializes replacement with the owning model operation. */
    if(!c->live.load(std::memory_order_acquire)||c->epoch.load(std::memory_order_acquire)!=s->epoch)return 0;
    if(!context_ready(c)||F(c->context,0x78,void*)!=s->things||
       F(c->context,0xc8,void*)!=s->properties||F(c->context,0x68,void*)!=s->manager||
       F(c->context,0x70,void*)!=s->service)return 0;
    return F(s->things,0xb8,void*)==s->program&&F(s->properties,0x1278,void*)==s->pool;
}
/* Each History allocation already owns its own intrusive link. Never
 * allocate or impose a count limit in the noexcept native deleter. */
static std::mutex recovery_mutex;
static void *recovery_head;
static void hold_recovery(void*p,void*command){
    auto*s=static_cast<Capture*>(p);
    std::lock_guard<std::mutex> lock(recovery_mutex);
    *pad_history_recovery_next(command)=recovery_head;recovery_head=command;
    s->owner->recovery_count.fetch_add(1,std::memory_order_release);
}
extern "C" resample_context *resample_context_create(uintptr_t base,void*context){
    if(!base||!context)return nullptr;
    auto*c=new(std::nothrow)resample_context{};if(!c)return nullptr;c->base=base;c->context=context;
    if(!context_ready(c)){delete c;return nullptr;}return c;
}
extern "C" void resample_context_changed(resample_context*c){if(c)c->epoch.fetch_add(1,std::memory_order_acq_rel);}
extern "C" void resample_context_stop(resample_context*c){if(c){
    c->live.store(false,std::memory_order_release);resample_context_changed(c);
    std::lock_guard<std::mutex> lock(c->hook_mutex);
    for(auto*h=c->hooks;h;h=h->next)h->stop(h->user);
}}
extern "C" int resample_context_add_hook(resample_context*c,resample_context_hook*h){
    if(!c||!h||!h->stop)return 0;
    std::lock_guard<std::mutex> lock(c->hook_mutex);
    if(!c->live.load(std::memory_order_acquire))return 0;
    for(auto*p=c->hooks;p;p=p->next)if(p==h)return 0;
    h->next=c->hooks;c->hooks=h;return 1;
}
extern "C" void resample_context_remove_hook(resample_context*c,resample_context_hook*h){
    if(!c||!h)return;
    std::lock_guard<std::mutex> lock(c->hook_mutex);auto**at=&c->hooks;
    while(*at&&*at!=h)at=&(*at)->next;
    if(*at){*at=h->next;h->next=nullptr;}
}
extern "C" int resample_context_capture(resample_context*c,resample_native_config*out,resample_selection*selection){
    if(!out||!selection)return 0;
    *out={};*selection={};
    if(!c)return 0;
    c->capture_error.store(CAP_STOPPED,std::memory_order_release);
    if(!context_ready(c))return 0;
    c->capture_error.store(CAP_RESOURCE,std::memory_order_release);
    auto*s=new(std::nothrow)Capture{};if(!s)return 0;
    if(!retain_context(c)){delete s;return 0;}
    s->owner=c;s->epoch=c->epoch.load(std::memory_order_acquire);
    uintptr_t sample[2]{};
    unsigned stage=CAP_SERVICES;int selected_pad=-1;
    try{
        s->things=F(c->context,0x78,void*);s->properties=F(c->context,0xc8,void*);
        s->manager=F(c->context,0x68,void*);s->service=F(c->context,0x70,void*);
        if(!s->things||!s->properties||!s->manager||!s->service||!F(c->context,0x48,void*)||
           F(s->manager,0,uintptr_t)!=c->base+0x6d97818)throw 0;
        stage=CAP_MODEL;
        s->program=F(s->things,0xb8,void*);s->pool=F(s->properties,0x1278,void*);
        if(!s->program||!s->pool||F(s->pool,0,uintptr_t)!=c->base+0x6dbd2f0)throw 0;
        /* Native CurrentThings::SetPad28f9d60 updates IntProperty5a8;
         * its real setter13b16a0 stores the selected index at5f8.
         * CurrentThings60 is NOT a selected Instrument. Resolve through
         * the same Program accessor used by native Sample loading. */
        stage=CAP_PAD;
        int pad=F(s->things,0x5f8,int);selected_pad=pad;
        if(pad<0||pad>=128)throw 0;
        stage=CAP_INSTRUMENT;
        void*instrument=reinterpret_cast<void*(*)(void*,int)>(c->base+0x286bbe0)(s->program,pad);
        if(!instrument||F(instrument,0x2310,void*)!=s->program)throw 0;
        stage=CAP_PAD;
        if(reinterpret_cast<int(*)(void*,void*)>(c->base+0x286bc20)(s->program,instrument)!=pad)throw 0;
        stage=CAP_EMPTY;
        pad_source_layer_sample(instrument,0,sample,c->base+0x2803b90);
        if(!sample[0]||!sample[1])throw 0;
        pad_memory_services memory{};
        stage=CAP_SAMPLE_SERVICES;
        if(!pad_memory_services_from_sample(c->base,sample,&memory))throw 0;
        stage=CAP_POOL_MEMBER;
        if(!pad_sample_pool_contains(c->base,s->pool,sample))throw 0;
        stage=CAP_NAME;
        const char*name=F(reinterpret_cast<void*>(sample[0]),0xa8,const char*);
        if(!name||!*name)throw 0;
        selection->pad=(unsigned)pad;size_t length=std::strlen(name);
        if(length>=sizeof(selection->sample)){length=sizeof(selection->sample)-1;while(length&&(static_cast<unsigned char>(name[length])&0xc0)==0x80)--length;}
        std::memcpy(selection->sample,name,length);selection->sample[length]=0;
        stage=CAP_OTHER_PAD;
        for(unsigned i=0;i<128;++i){
            void*p=reinterpret_cast<void*(*)(void*,int)>(c->base+0x286bbe0)(s->program,(int)i);
            if(!p)continue;
            if(F(p,0x2310,void*)!=s->program)throw 0;
            /* Any occupied layer makes overwriting this pad nonempty. */
            for(unsigned layer=0;layer<8;++layer){uintptr_t value[2]{};
                pad_source_layer_sample(p,(int)layer,value,c->base+0x2803b90);bool occupied=value[0]!=0;
                pad_assignment_release(c->base,value);
                if(occupied){selection->occupied[i/64]|=UINT64_C(1)<<(i%64);break;}
            }
        }
        stage=CAP_CHANGED;
        if(!validate_capture(s,PAD_COMMIT_APPLY)||F(s->things,0x5f8,int)!=pad)throw 0;
        out->commit={c->base,s->properties,s->program,s->service,s->pool,s,validate_capture,retain_capture,release_capture};
        out->memory=memory;out->manager=s->manager;out->hold_recovery=hold_recovery;
        if(fx_native_context_capture(c->base,&s->native_fx))fx_native_backend(&s->native_fx,&out->fx);
        std::fprintf(stderr,"RESAMPLE NATIVE_FX_CONTEXT ready=%u\n",out->fx.create!=nullptr);
        c->capture_error.store(CAP_NONE,std::memory_order_release);
        pad_assignment_release(c->base,sample);return 1;
    }catch(...){
        c->capture_error.store(stage,std::memory_order_release);
        /* All objects below have already been acquired by native getters.
         * Log only pinned-ELF vtable offsets, never names or heap addresses. */
        uintptr_t manager=s->manager?F(s->manager,0,uintptr_t):0;
        uintptr_t pool=s->pool?F(s->pool,0,uintptr_t):0;
        uintptr_t sample_type=sample[0]?F(reinterpret_cast<void*>(sample[0]),0,uintptr_t):0;
        auto relative=[c](uintptr_t value)->unsigned long long{return value>=c->base&&value-c->base<0x7500000?(unsigned long long)(value-c->base):0;};
        std::fprintf(stderr,"RESAMPLE CAPTURE_REJECTED stage=%u pad=%d manager_type=%llx pool_type=%llx sample_type=%llx\n",stage,selected_pad,relative(manager),relative(pool),relative(sample_type));
        pad_assignment_release(c->base,sample);release_capture(s);*selection={};return 0;
    }
}
extern "C" unsigned resample_context_recovery_count(resample_context*c){return c?c->recovery_count.load(std::memory_order_acquire):0;}
extern "C" int resample_context_finish_recovery(resample_context*c){
    if(!c)return 0;
    /* Owner only; detach before native callbacks. Host caller still owns c;
     * History's own leases keep its Capture alive until destruction finishes. */
    void*command=nullptr;
    {std::lock_guard<std::mutex> lock(recovery_mutex);void**at=&recovery_head;
     while(*at&&static_cast<Capture*>(pad_history_recovery_user(*at))->owner!=c)at=pad_history_recovery_next(*at);
     if(*at){command=*at;*at=*pad_history_recovery_next(command);}}
    if(!command)return 1;
    if(!pad_history_finish_recovery(command)){
        std::lock_guard<std::mutex> lock(recovery_mutex);*pad_history_recovery_next(command)=recovery_head;recovery_head=command;return 0;
    }
    c->recovery_count.fetch_sub(1,std::memory_order_release);return 1;
}
