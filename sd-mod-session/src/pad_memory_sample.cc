#include "pad_memory_sample.h"
#include <memory>
#include <vector>
#include <new>
#include <cmath>
#include <cstring>
#define F(p,o,T) (*reinterpret_cast<T*>(reinterpret_cast<char*>(p)+(o)))
extern "C" void pad_memory_factory(void*,void*,void*,uintptr_t*,uintptr_t);
extern "C" int pad_memory_services_from_sample(uintptr_t base,const uintptr_t sample[2],pad_memory_services*out){
    if(!out)return 0;
    *out={};if(!base||!sample||!sample[0]||!sample[1])return 0;
    void*p=reinterpret_cast<void*>(sample[0]);if(F(p,0,uintptr_t)!=base+0x6dbd868)return 0;
    pad_memory_services services{F(p,0x48,void*),F(p,0x160,void*),F(p,0x168,void*)};
    if(!services.queue_context||!services.storage||!services.resources)return 0;
    *out=services;return 1;
}
struct pad_memory_job {
    uintptr_t base,sample[2],stream[2];
    std::shared_ptr<void> memory;
    const float *pcm;uint64_t total,done;
    enum pad_render_status status;bool cancel,taken;
};
static void release_pair(uintptr_t base,uintptr_t p[2]){
    uintptr_t r=p[1];p[0]=p[1]=0;
    if(r)reinterpret_cast<void(*)(void*)>(base+0x123bca0)(reinterpret_cast<void*>(r));
}
static void close_input(pad_memory_job*j){
    uintptr_t f=j->stream[0],p=j->stream[1];j->stream[0]=j->stream[1]=0;
    if(p&&f)reinterpret_cast<void(*)(void*)>(f)(reinterpret_cast<void*>(p));
    j->pcm=nullptr;
}
static void fail(pad_memory_job*j,enum pad_render_status status){
    release_pair(j->base,j->sample);j->memory.reset();close_input(j);j->status=status;
}
extern "C" void pad_memory_destroy(pad_memory_job*j){
    if(j){fail(j,j->status);delete j;}
}
extern "C" pad_memory_job *pad_memory_begin(uintptr_t base,const pad_memory_services*s,
    const pad_output_reference*a,const char*name,size_t budget,enum pad_render_status*error){
    enum pad_render_status status=PAD_RENDER_INVALID;pad_memory_job*j=nullptr;
    if(!base||!s||!s->queue_context||!s->storage||!s->resources||!a||!name||!*name||std::strlen(name)>127)goto rejected;
    if(!budget)budget=PAD_RENDER_MAX_PCM_BYTES;
    /* Existing output + new native vector; compare before multiplying. */
    if(budget>PAD_RENDER_MAX_PCM_BYTES||!a->frames||a->frames>budget/(4*sizeof(float))){status=PAD_RENDER_LIMIT;goto rejected;}
    j=new(std::nothrow)pad_memory_job{};if(!j){status=PAD_RENDER_RESOURCE;goto rejected;}
    j->base=base;j->status=PAD_RENDER_WORKING;j->total=a->frames;
    status=pad_output_stream_open(base,a,j->stream);if(status!=PAD_RENDER_WORKING)goto rejected;
    j->pcm=a->pcm;
    try{
        pad_memory_factory(s->queue_context,s->storage,s->resources,j->sample,base+0x2943b40);
        if(!j->sample[0]||!j->sample[1]){status=PAD_RENDER_RESOURCE;goto rejected;}
        void*sample=reinterpret_cast<void*>(j->sample[0]);
        void*reader=reinterpret_cast<void*>(j->stream[1]);
        uintptr_t metadata[2]={F(reader,8,uintptr_t),F(reader,16,uintptr_t)};
        reinterpret_cast<void(*)(void*,const uintptr_t*)>(base+0x2944be0)(sample,metadata);
        void*raw=::operator new(0x78);bool constructed=false;
        try{
            reinterpret_cast<void(*)(void*,const void*,const void*)>(base+0x293c1a0)
                (raw,reinterpret_cast<char*>(sample)+0x48,reinterpret_cast<char*>(sample)+0x130);
            constructed=true;
            /* Native Sample deleter first cancels provider work, then queues
             * deletion on its owner. Preserve that policy, never delete a
             * provider from a future real-time reader's release callback. */
            j->memory=std::shared_ptr<void>(raw,[base](void*p){reinterpret_cast<void(*)(void*)>(base+0x2941400)(p);});
        }catch(...){if(!constructed)::operator delete(raw);throw;}
        int reserved=reinterpret_cast<int(*)(void*,void*,void*)>(base+0x293cf60)
            (raw,F(sample,0x130,void*),s->resources);
        if(reserved){status=PAD_RENDER_RESOURCE;goto rejected;}
        auto*buffer=F(raw,0x28,std::vector<float>*);
        if(!buffer||!buffer->empty()||buffer->capacity()!=a->frames*2){status=PAD_RENDER_INVALID;goto rejected;}
        /* Name Value uses its ordinary native setter, not a raw string-field
         * overwrite. It is not yet a SamplePool registration. */
        uint64_t text=0;reinterpret_cast<void(*)(void*,const char*)>(base+0xf535c0)(&text,name);
        try{reinterpret_cast<void(*)(void*,const uint64_t*)>(base+0x13bf450)(reinterpret_cast<char*>(sample)+0x58,&text);}
        catch(...){reinterpret_cast<void(*)(void*)>(base+0xf0a290)(&text);throw;}
        reinterpret_cast<void(*)(void*)>(base+0xf0a290)(&text);
        if(error)*error=PAD_RENDER_WORKING;
        return j;
    }catch(...){status=PAD_RENDER_RESOURCE;}
rejected:
    pad_memory_destroy(j);if(error)*error=status;return nullptr;
}
extern "C" enum pad_render_status pad_memory_step(pad_memory_job*j,size_t frames){
    if(!j)return PAD_RENDER_INVALID;
    if(j->status!=PAD_RENDER_WORKING)return j->status;
    if(j->cancel){fail(j,PAD_RENDER_CANCELLED);return j->status;}
    if(!frames)return PAD_RENDER_INVALID;
    if(frames>PAD_RENDER_MAX_STEP_FRAMES)frames=PAD_RENDER_MAX_STEP_FRAMES;
    if(frames>j->total-j->done)frames=(size_t)(j->total-j->done);
    try{
        const float*first=j->pcm+j->done*2;
        for(size_t i=0;i<frames*2;++i)if(!std::isfinite(first[i])){fail(j,PAD_RENDER_AUDIO_ERROR);return j->status;}
        auto*buffer=F(j->memory.get(),0x28,std::vector<float>*);
        buffer->insert(buffer->end(),first,first+frames*2);j->done+=frames;
        if(j->done==j->total){
            uintptr_t provider[2];static_assert(sizeof(j->memory)==sizeof(provider),"Pinned shared_ptr ABI");std::memcpy(provider,&j->memory,sizeof(provider));
            reinterpret_cast<void(*)(void*,const uintptr_t*)>(j->base+0x2943820)(reinterpret_cast<void*>(j->sample[0]),provider);
            close_input(j);j->status=PAD_RENDER_DONE;
        }
    }catch(...){fail(j,PAD_RENDER_RESOURCE);}
    return j->status;
}
extern "C" void pad_memory_cancel(pad_memory_job*j){if(j)j->cancel=true;}
extern "C" void pad_memory_progress(const pad_memory_job*j,uint64_t*d,uint64_t*t){if(d)*d=j?j->done:0;if(t)*t=j?j->total:0;}
extern "C" int pad_memory_take(pad_memory_job*j,uintptr_t out[2]){
    if(!out)return 0;
    out[0]=out[1]=0;
    if(!j||j->status!=PAD_RENDER_DONE||j->taken)return 0;
    out[0]=j->sample[0];out[1]=j->sample[1];j->sample[0]=j->sample[1]=0;j->taken=true;return 1;
}
