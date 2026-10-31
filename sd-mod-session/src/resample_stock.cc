#include "resample_stock.h"
#include "resample_file_probe.h"
#ifdef MPC_FAST_RESAMPLE
#include "resample_fast_native.h"
#include "resample_fault.h"
#endif
#include <atomic>
#include <mutex>
#include <functional>
#include <memory>
#include <cstring>
#include <string>
#include <cerrno>
#include <sys/mman.h>
#include <unistd.h>
#define F(p,o,T) (*reinterpret_cast<T*>(reinterpret_cast<char*>(p)+(o)))
namespace {
using Export=void(*)(void*,void*,void*,unsigned,unsigned,unsigned,void*);
using PadExport=void(*)(void*,void*,void*,unsigned,unsigned,unsigned,void*,unsigned,unsigned);
std::atomic<uintptr_t> image{0};
std::mutex install_mutex;
#ifdef MPC_FAST_RESAMPLE
std::atomic<unsigned> terminal_error{0};
#endif
/* Nontrivial return type pins native String's indirect-result x8 ABI. Its
 * ownership passes to the native caller, which uses its own destructor. */
struct NativeString {uintptr_t value=0;~NativeString(){}};
NativeString sample_name(const char*,const void*);
void sample_trigger(void*engine,int64_t tick,const void*event,void*track,unsigned source,unsigned mask,unsigned on){
    (void)source;(void)mask;
    uintptr_t b=image.load(std::memory_order_acquire);
    void*things=engine?F(engine,0x3c0,void*):nullptr;
    void*program=things?F(things,0xb8,void*):nullptr;
    void*sample_track=things?F(things,0x140,void*):nullptr;
    unsigned midi=event?static_cast<const unsigned char*>(event)[48]:128;
    int pad=program&&midi<128?F(program,0x14a8+midi*4,int):-1;
    if(!program||!sample_track||F(sample_track,0xbb8,void*)!=program||pad<0||pad>=128||F(program,0x12a8+pad*4,int)!=(int)midi){
        std::fprintf(stderr,"RESAMPLE SAMPLE_TRIGGER_REJECTED midi=%u program=%u pad=%d\n",midi,program!=nullptr,pad);return;
    }
    /* Export already supplies a MIDI note, not a raw hardware pad. Target
     * the validated AC50 track explicitly: ordinary211b090 depends on a
     * live-input route registry which need not contain the offline engine.
     * Keep the export mask so2115070 does not remap this MIDI a second time.
     * Source0 uses the same accepted Sample input policy as live audition. */
    void*policy=F(engine,0x5d8,void*);
    float velocity;std::memcpy(&velocity,static_cast<const char*>(event)+16,4);
    std::fprintf(stderr,"RESAMPLE SAMPLE_TRIGGER pad=%d midi=%u receiver=SampleTrack track_changed=%u velocity=%.6g input_blocked=%u\n",pad,midi,track!=sample_track,(double)velocity,policy?F(policy,0x208,unsigned char)&1:0);
    reinterpret_cast<void(*)(void*,int64_t,const void*,void*,unsigned,unsigned,unsigned)>(b+0x211aa30)(engine,tick,event,sample_track,0,0x7ffffff8,on);
}
void*trigger_relay(uintptr_t b){
    /* A BL relay within +/-128MiB, without replacing any existing mapping.
     * Both the page and the one callsite belong only to the cloned process. */
    int last_error=0;
    /* Native startup may have filled the range above the ELF with its heap.
     * Search below first; MAP_FIXED_NOREPLACE never evicts an existing page. */
    for(unsigned attempt=0;attempt<107;++attempt){
        intptr_t off=attempt<80?-intptr_t(attempt+1)*0x100000:
            intptr_t(0x7600000)+(attempt-80)*0x100000;
        if(off<0&&b<=uintptr_t(-off)+65536)continue;
        uintptr_t address=b+off;intptr_t delta=intptr_t(address)-intptr_t(b+0x18b9b6c);
        if((address&4095)||delta%4||delta<-(INT64_C(1)<<27)||delta>=(INT64_C(1)<<27))continue;
        void*hint=reinterpret_cast<void*>(address);
        void*p=mmap(hint,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
        if(p==MAP_FAILED){last_error=errno;continue;}
        if(p!=hint){munmap(p,4096);continue;}
        uint32_t code[]={0xd503245f,0x58000070,0xd61f0200,0xd503201f};
        std::memcpy(p,code,sizeof(code));F(p,16,uintptr_t)=reinterpret_cast<uintptr_t>(sample_trigger);
        std::memcpy(static_cast<char*>(p)+32,code,sizeof(code));F(p,48,uintptr_t)=reinterpret_cast<uintptr_t>(sample_name);
        __builtin___clear_cache(static_cast<char*>(p),static_cast<char*>(p)+56);
        if(mprotect(p,4096,PROT_READ|PROT_EXEC)){last_error=errno;munmap(p,4096);continue;}
        std::fprintf(stderr,"RESAMPLE RELAY_READY offset=%lld\n",(long long)off);return p;
    }std::fprintf(stderr,"RESAMPLE INSTALL_ERROR stage=relay errno=%d\n",last_error);return nullptr;
}
int install_error(const char*stage){std::fprintf(stderr,"RESAMPLE INSTALL_ERROR stage=%s errno=%d\n",stage,errno);return 0;}
struct Scope {void *properties;unsigned source;bool single,called,rejected;void(*completed)(void*,int);void*user;std::string source_name;};
struct Completion {
    void(*callback)(void*,int);void*user;std::atomic<bool> delivered{false},invoking{false};
    void finish(int invoked){if(!delivered.exchange(true,std::memory_order_acq_rel))callback(user,invoked);}
    ~Completion(){finish(0);}
};
thread_local Scope *scope=nullptr;
NativeString sample_name(const char*prefix,const void*sequence_name){
    uintptr_t b=image.load(std::memory_order_acquire);
    if(!scope||!scope->single)return reinterpret_cast<NativeString(*)(const char*,const void*)>(b+0xf4ce00)(prefix,sequence_name);
    NativeString out;
    reinterpret_cast<void(*)(void*,const char*)>(b+0xf535c0)(&out,scope->source_name.c_str());
    std::fprintf(stderr,"RESAMPLE SAMPLE_NAME source_name_used=1\n");return out;
}
struct ScopedCall {explicit ScopedCall(Scope*s){scope=s;}~ScopedCall(){scope=nullptr;}};
void export_dispatch(void*runner,void*path,void*properties,unsigned option,unsigned rate,unsigned depth,void*callback){
    uintptr_t b=image.load(std::memory_order_acquire);
    if(!scope||properties!=scope->properties){reinterpret_cast<Export>(b+0x18c62f0)(runner,path,properties,option,rate,depth,callback);return;}
    /* Never turn a rejected single-pad operation into a sequence render.
     * Native setup is synchronous up to export submission, and Scope is
     * thread-local: concurrent unrelated exports keep the original path. */
    if(scope->called||
       (scope->single&&F(properties,0x500,unsigned)!=scope->source)){
        scope->rejected=true;return;
    }
    std::function<void()> watched;
    if(scope->completed){
        static_assert(sizeof(watched)==32,"Pinned native std::function<void()> ABI");
        auto token=std::make_shared<Completion>();token->callback=scope->completed;token->user=scope->user;
        auto original=*static_cast<std::function<void()>*>(callback);
        const char*name=path?F(path,0,const char*):nullptr;
        std::string file=name&&strnlen(name,4096)<4096?name:"";bool single=scope->single;
        watched=[original,token,file,single](){
            if(token->invoking.exchange(true,std::memory_order_acq_rel))return;
#ifdef MPC_FAST_RESAMPLE
            resample_fault_stage(12);
            if(single&&!resample_fast_native_error())resample_fast_native_prepare_import(file.c_str());
#endif
            auto p=resample_file_probe(file.c_str());
            std::fprintf(stderr,"RESAMPLE FILE_AUDIO single=%u valid=%u format=%u channels=%u rate=%u bits=%u bytes=%u scanned=%u peak=%.9g path=%s\n",single,p.valid,p.format,p.channels,p.rate,p.bits,p.bytes,p.scanned,p.peak,file.c_str());
#ifdef MPC_FAST_RESAMPLE
            if(single){
                unsigned rejected=resample_fast_native_error()?1:!p.valid?2:
                    p.scanned==p.bytes&&p.peak==0?3:0;
                if(rejected){
                    terminal_error.store(rejected,std::memory_order_release);
                    std::fprintf(stderr,"RESAMPLE IMPORT_REJECTED reason=%u destination_unchanged=1\n",rejected);
                    /* Native assignment is owned by this callback. Disposing
                     * its captures without invoking it matches cancellation:
                     * no reset of the destination, import or Undo command. */
                    token->finish(0);return;
                }
            }
#endif
#ifdef MPC_FAST_RESAMPLE
            resample_fault_stage(13);
#endif
            original();token->finish(1);
#ifdef MPC_FAST_RESAMPLE
            resample_fault_stage(14);
#endif
        };
        callback=&watched;
    }
#ifdef MPC_FAST_RESAMPLE
    if(scope->single){
        const char*file=path?F(path,0,const char*):nullptr;
        /*18c57f4 passes runner+b0 as x1 to18b81f0. It is the same service
         * context passed through18bb.../18b9ca0 to the render callsite. */
        void*services=runner?F(runner,0xb0,void*):nullptr;
        if(!resample_fast_native_arm(services,scope->source,file)){scope->rejected=true;return;}
    }
#endif
    scope->called=true;
    if(scope->single)reinterpret_cast<PadExport>(b+0x18c63b0)(runner,path,properties,0,rate,depth,callback,1,1);
    else reinterpret_cast<Export>(b+0x18c62f0)(runner,path,properties,option,rate,depth,callback);
}
}
extern "C" const char*resample_stock_error(){
#ifdef MPC_FAST_RESAMPLE
    switch(terminal_error.load(std::memory_order_acquire)){
        case 1:return resample_fast_native_error();
        case 2:return "Sample render did not produce a valid WAV";
        case 3:return "Sample render is silent; destination unchanged";
    }
#endif
    return nullptr;
}
extern "C" int resample_stock_install(uintptr_t b){
    if(!b)return 0;
    std::lock_guard<std::mutex> lock(install_mutex);
    if(image.load(std::memory_order_acquire))return image.load()==b&&F(reinterpret_cast<void*>(b),0x7167218,uintptr_t)==reinterpret_cast<uintptr_t>(export_dispatch);
    /* RELRO slot used by native2a02ee0 only. Keep neighbouring Song/Track
     * exports intact; never patch firmware files or running instructions. */
    auto*slot=reinterpret_cast<uintptr_t*>(b+0x7167218);
    if(*slot!=b+0x18c62f0||slot[1]!=b+0x18c6330)return install_error("export-slot");
    /* The same native Undo group now serves Seq and Sample. Its shared
     * title must describe both, including Undo after a cancelled attempt. */
    auto*title=reinterpret_cast<char*>(b+0x45a10e0);
    if(std::strcmp(title,"Resample Sequence")&&std::strcmp(title,"Resample"))return install_error("undo-title");
    /* Scope the correction to the dedicated single-pad export call. */
    auto*trigger=reinterpret_cast<uint32_t*>(b+0x18b9b6c);
    if(*trigger!=0x942183b1)return install_error("sample-callsite");
    auto*name_call=reinterpret_cast<uint32_t*>(b+0x153edc8);
    if(*name_call!=0x97e8380e)return install_error("name-callsite");
    long size=sysconf(_SC_PAGESIZE);if(size<=0||(size&(size-1)))return 0;
    struct Page {uintptr_t address;int protection;const char*stage;};
    Page pages[]={{reinterpret_cast<uintptr_t>(trigger)&~(uintptr_t(size)-1),PROT_READ|PROT_EXEC,"sample-page"},
        {reinterpret_cast<uintptr_t>(name_call)&~(uintptr_t(size)-1),PROT_READ|PROT_EXEC,"name-page"},
        {reinterpret_cast<uintptr_t>(title)&~(uintptr_t(size)-1),PROT_READ,"title-page"},
        {reinterpret_cast<uintptr_t>(slot)&~(uintptr_t(size)-1),PROT_READ,"slot-page"}};
    void*relay=trigger_relay(b);if(!relay)return 0;
    intptr_t name_delta=reinterpret_cast<uintptr_t>(relay)+32-reinterpret_cast<uintptr_t>(name_call);
    if(name_delta%4||name_delta<-(INT64_C(1)<<27)||name_delta>=(INT64_C(1)<<27)){munmap(relay,4096);return install_error("name-relay-range");}
    for(unsigned i=0;i<4;++i)if(mprotect(reinterpret_cast<void*>(pages[i].address),size,PROT_READ|PROT_WRITE)){
        install_error(pages[i].stage);for(unsigned j=0;j<i;++j)if(mprotect(reinterpret_cast<void*>(pages[j].address),size,pages[j].protection))_exit(121);
        munmap(relay,4096);return 0;
    }
    image.store(b,std::memory_order_release);
    intptr_t delta=reinterpret_cast<uintptr_t>(relay)-reinterpret_cast<uintptr_t>(trigger);
    *trigger=0x94000000u|(uint32_t(delta/4)&0x03ffffffu);
    __builtin___clear_cache(reinterpret_cast<char*>(trigger),reinterpret_cast<char*>(trigger+1));
    *name_call=0x94000000u|(uint32_t(name_delta/4)&0x03ffffffu);
    __builtin___clear_cache(reinterpret_cast<char*>(name_call),reinterpret_cast<char*>(name_call+1));
    std::memcpy(title,"Resample\0",9);
    __atomic_store_n(slot,reinterpret_cast<uintptr_t>(export_dispatch),__ATOMIC_RELEASE);
    for(auto&p:pages)if(mprotect(reinterpret_cast<void*>(p.address),size,p.protection))_exit(121);
    std::fprintf(stderr,"RESAMPLE ADAPTER_READY\n");
    return 1;
}
extern "C" int resample_stock_start_named_observed(uintptr_t b,void*mode,int single,unsigned source,unsigned destination,const char*source_name,void(*completed)(void*,int),void*user){
    if(!b||image.load(std::memory_order_acquire)!=b||scope||!mode||source>=128||destination>=128)return 0;
    /* Itanium offset-to-top confirms the complete/native base relationship;
     * native153eab0/153ed50 run on mode+48, not the complete mode. */
    if(F(mode,0,uintptr_t)!=b+0x6e10598||F(mode,0x48,uintptr_t)!=b+0x6e105f0||F(mode,0x90,uintptr_t)!=b+0x6e10678)return 0;
    void*behavior=static_cast<char*>(mode)+0x48;
    if(F(behavior,0x80,unsigned))return 0;
    void*properties=F(behavior,0xe8,void*),*things=F(behavior,0xb0,void*);
    if(!properties||!things||!F(things,0x1c8,void*)||!F(properties,0x1278,void*))return 0;
    /*29f8310 otherwise opens a synchronous native Stop confirmation from
     * this queued owner. The mod confirms and waits on UI before submission.
     * Recheck the same native isPlaying interface to close the Play race. */
    void*transport=F(behavior,0x110,void*);
    if(transport){auto vt=F(transport,0,uintptr_t);auto playing=vt?F(reinterpret_cast<void*>(vt),0x18,uintptr_t):0;
        if(!playing||(reinterpret_cast<unsigned(*)(void*)>(playing)(transport)&1u))return 0;}
    if(single&&(F(things,0x5f8,unsigned)!=source||F(properties,0x500,unsigned)!=source||
        !F(properties,0xb8,void*)||F(properties,0xb8,void*)!=F(things,0xb8,void*)))return 0;
#ifdef MPC_FAST_RESAMPLE
    if(single){
        terminal_error.store(0,std::memory_order_release);
        if(!resample_fast_native_ready(b))return 0;
        if(!resample_fault_install(b))std::fprintf(stderr,"RESAMPLE FAULT_DIAGNOSTIC_UNAVAILABLE\n");
        resample_fault_stage(1);
    }
#endif
    std::string name=source_name&&*source_name?source_name:"Sample";name+=" Resample";
    Scope request{properties,source,single!=0,false,false,completed,user,name};ScopedCall guard(&request);
    /* Stock creates its output file, executes its renderer, loads Sample,
     * resets destination processing and performs assignment in its own Undo
     * group via153f200. We do not construct a MemorySample or invoke an
     * assignment on the audio thread. Owner serializes async completion. */
    reinterpret_cast<void(*)(void*,unsigned)>(b+0x153ed50)(behavior,destination);
    return request.called&&!request.rejected;
}
extern "C" int resample_stock_start_observed(uintptr_t b,void*mode,int single,unsigned source,unsigned destination,void(*completed)(void*,int),void*user){return resample_stock_start_named_observed(b,mode,single,source,destination,nullptr,completed,user);}
extern "C" int resample_stock_start(uintptr_t b,void*mode,int single,unsigned source,unsigned destination){return resample_stock_start_observed(b,mode,single,source,destination,nullptr,nullptr);}
