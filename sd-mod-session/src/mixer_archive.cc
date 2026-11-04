#include "mixer_archive.h"
#include <atomic>
#include <cstring>
#include <exception>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>

namespace {
constexpr const char *key="amMixerV1";
struct State {
    mixer_project project{};
    unsigned mode=MIXER_ARCHIVE_LEGACY;
    uint64_t revision=0;
    std::string opaque;
    State(){mixer_defaults(&project);}
};
std::mutex mutex;
std::shared_ptr<const State> current;
uintptr_t image=0;
std::atomic<uint64_t> saved{0}, source_generation{1};
std::atomic<unsigned> loading{0};
struct SourceChange {
    SourceChange() {
        loading.fetch_add(1,std::memory_order_acq_rel);
        source_generation.fetch_add(1,std::memory_order_acq_rel);
    }
    ~SourceChange() {
        source_generation.fetch_add(1,std::memory_order_acq_rel);
        loading.fetch_sub(1,std::memory_order_release);
    }
};
template<class T> T field(const void*p,size_t offset){T result;std::memcpy(&result,static_cast<const char*>(p)+offset,sizeof(T));return result;}
struct Bytes {const char *data;size_t size;};
Bytes string_value(const void*value)
{
    unsigned flags=field<unsigned short>(value,0x16);
    if(!(flags&(1u<<10)))throw std::runtime_error("Mixer extension must be a JSON string");
    if(flags&(1u<<12)){
        unsigned spare=field<unsigned char>(value,21);
        if(spare>21)throw std::runtime_error("Invalid inline Mixer archive string");
        return {static_cast<const char*>(value),21-spare};
    }
    size_t length=field<uint32_t>(value,0);const char*text=field<const char*>(value,8);
    if(!text&&length)throw std::runtime_error("Invalid Mixer archive string");
    return {text?text:"",length};
}
std::shared_ptr<State> capture(void*wrapper)
{
    auto next=std::make_shared<State>();
    void*archive=wrapper;
    for(unsigned i=0;i<4;++i){if(!archive)throw std::runtime_error("Mixer input archive unavailable");archive=field<void*>(archive,8);}
    if(!archive)throw std::runtime_error("Mixer input archive unavailable");
    uintptr_t cursor=field<uintptr_t>(archive,0x150);
    if(cursor<0x38)throw std::runtime_error("Mixer archive has no object frame");
    const void*frame=reinterpret_cast<const void*>(cursor-0x38);
    if(field<unsigned>(frame,0x30)!=1)throw std::runtime_error("Mixer archive is outside project object");
    uintptr_t begin=field<uintptr_t>(frame,0),end=field<uintptr_t>(frame,8);
    if(end<begin||(end-begin)%48||(!begin&&end))throw std::runtime_error("Invalid Mixer archive members");
    bool found=false;
    for(uintptr_t p=begin;p<end;p+=48){
        Bytes name=string_value(reinterpret_cast<const void*>(p));
        if(name.size!=std::strlen(key)||std::memcmp(name.data,key,name.size))continue;
        if(found)throw std::runtime_error("Duplicate Mixer project extension");
        found=true;Bytes value=string_value(reinterpret_cast<const void*>(p+24));
        enum mixer_result result=mixer_decode(value.data,value.size,&next->project);
        if(result==MIXER_OK)next->mode=MIXER_ARCHIVE_KNOWN;
        else {
            /* Preserve even malformed strings, embedded NULs and future
             * versions verbatim. No strlen/capacity-based truncation. */
            next->opaque.assign(value.data,value.size);next->mode=MIXER_ARCHIVE_OPAQUE;
        }
    }
    return next;
}
struct Transaction {std::shared_ptr<State> pending;std::exception_ptr failure;bool captured=false;};
thread_local Transaction*transaction=nullptr;
void publish(std::shared_ptr<State> next)
{
    std::lock_guard<std::mutex> lock(mutex);
    if(current&&current->revision==UINT64_MAX)throw std::overflow_error("Mixer project revision exhausted");
    next->revision=current?current->revision+1:1;current=std::move(next);
}
std::shared_ptr<const State> snapshot()
{
    std::lock_guard<std::mutex> lock(mutex);
    if(!current)throw std::runtime_error("Mixer archive owner not initialized");
    return current;
}
}
extern "C" void mixer_archive_init(uintptr_t base)
{
    if(!base)throw std::invalid_argument("Mixer archive requires native image");
    auto initial=std::make_shared<State>();
    std::lock_guard<std::mutex> lock(mutex);
    if(image&&image!=base)throw std::runtime_error("Mixer archive image changed");
    image=base;if(!current){initial->revision=1;current=std::move(initial);}
}
extern "C" uint64_t mixer_archive_generation(){return source_generation.load(std::memory_order_acquire);}
extern "C" int mixer_archive_loading(){return loading.load(std::memory_order_acquire)!=0;}
extern "C" void mixer_archive_reset(){
    SourceChange changing;
    publish(std::make_shared<State>());
}
extern "C" int mixer_archive_snapshot(mixer_project*out,uint64_t*revision,unsigned*mode)
{
    if(!out||!revision||!mode)return 0;
    auto state=snapshot();*out=state->project;*revision=state->revision;*mode=state->mode;return 1;
}
extern "C" mixer_archive_edit mixer_archive_replace(const mixer_project*project,uint64_t revision)
{
    if(!mixer_project_valid(project))return MIXER_EDIT_INVALID;
    auto next=std::make_shared<State>();next->project=*project;next->mode=MIXER_ARCHIVE_KNOWN;
    std::lock_guard<std::mutex> lock(mutex);
    if(!current||current->revision!=revision)return MIXER_EDIT_STALE;
    if(current->mode==MIXER_ARCHIVE_OPAQUE)return MIXER_EDIT_READONLY;
    if(revision==UINT64_MAX)return MIXER_EDIT_INVALID;
    next->revision=revision+1;current=std::move(next);return MIXER_EDIT_OK;
}
extern "C" uint64_t mixer_archive_saved_count(){return saved.load(std::memory_order_acquire);}
extern "C" void mixer_archive_load(void*data,void*archive,void(*original)(void*,void*))
{
    SourceChange changing;
    original(data,archive);
    if(!transaction)return;
    /* Do not unwind through the existing C music_archive_read() scope: it
     * owns its own malloc/TLS transaction and is not exception-safe. Report
     * this failure from our outer C++ read scope after that owner unwinds. */
    try{
        if(transaction->captured)throw std::runtime_error("Multiple Mixer project objects in one load");
        transaction->captured=true;transaction->pending=capture(archive);
    }catch(...){transaction->failure=std::current_exception();}
}
extern "C" int mixer_archive_read(void*data,void*json,int(*original)(void*,void*))
{
    SourceChange changing;
    /* Concurrent Read vs New/edit is serialized by the existing native
     * project queue. Do not retain a project mutex across native callbacks.
     * Nested reads are ambiguous: reject before they can change native data. */
    if(transaction)throw std::runtime_error("Nested Mixer project read");
    Transaction pending;pending.pending=std::make_shared<State>();
    struct Scope {Transaction*old;explicit Scope(Transaction*t):old(transaction){transaction=t;}~Scope(){transaction=old;}}scope(&pending);
    int ok=original(data,json);
    if(pending.failure)std::rethrow_exception(pending.failure);
    if(ok)publish(std::move(pending.pending));
    return ok;
}
extern "C" void mixer_archive_save(void*data,void*archive,void(*original)(void*,void*))
{
    auto state=snapshot();std::string payload;
    if(state->mode==MIXER_ARCHIVE_OPAQUE)payload=state->opaque;
    else if(state->mode==MIXER_ARCHIVE_KNOWN){
        size_t size=0;
        if(mixer_encode(&state->project,nullptr,0,&size)!=MIXER_OK)throw std::runtime_error("Mixer snapshot is invalid");
        payload.resize(size+1);
        if(mixer_encode(&state->project,&payload[0],payload.size(),nullptr)!=MIXER_OK)throw std::runtime_error("Mixer encoding failed");
        payload.resize(size);
    }
    /* Finish all our allocations before asking native code to serialize.
     * Native writer remains responsible for output/compression/commit/errors. */
    original(data,archive);
    if(state->mode!=MIXER_ARCHIVE_LEGACY){
        struct {const char*data;size_t size,capacity,unused;} text={payload.data(),payload.size(),payload.size(),0};
        auto write=reinterpret_cast<void(*)(void*,const char*,const void*)>(image+0x1d27d90);
        write(archive,key,&text);
    }
    saved.fetch_add(1,std::memory_order_release);
}
