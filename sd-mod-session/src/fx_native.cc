#include "fx_native.h"
#include "fx_catalog.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <memory>
#include <new>
#include <utility>
#include <atomic>
#define F(p,o,T) (*reinterpret_cast<T*>(reinterpret_cast<char*>(p)+(o)))
namespace {
std::atomic<void*> registry{nullptr};
struct Object {};
struct NativeDelete {void operator()(Object*p)const{if(p){auto*v=F(p,0,uintptr_t*);reinterpret_cast<void(*)(void*)>(v[1])(p);}}};
using NativePtr=std::unique_ptr<Object,NativeDelete>;
struct Text {const char*data;};
using Callback=std::function<void(NativePtr,const Text&)>;
struct PlayHead {
    double bpm=120,rate=48000;unsigned numerator=4,denominator=4;uint64_t frame=0;
    bool live=false;
    alignas(8) unsigned char current[88]{};
    virtual ~PlayHead(){}
    virtual bool position(void*out){
        if(live){std::memcpy(out,current,88);return true;}
        std::memset(out,0,88);F(out,0,double)=bpm;F(out,8,int)=(int)numerator;F(out,12,int)=(int)denominator;
        double ppq=frame/rate*bpm/60,bar=numerator*4./denominator;
        F(out,16,int64_t)=(int64_t)frame;F(out,24,double)=frame/rate;F(out,40,double)=ppq;
        F(out,48,double)=std::floor(ppq/bar)*bar;
        /* Offline render advances its own transport. A stopped playhead
         * would silently disable tempo-gated processors such as Pumper. */
        F(out,60,unsigned char)=1;return true;
    }
};
struct Processor {
    NativePtr object;
    PlayHead playhead;
    float left[256]{},right[256]{};
    float*channels[2]={left,right};
    alignas(8) unsigned char audio[320]{};
    alignas(8) unsigned char midi[16]{};
    void *position_user=nullptr;
    int (*position)(void*,void*,uint64_t*)=nullptr;
    uint64_t block=0,offset=0;
    bool have_block=false;
    ~Processor(){object.reset();} // playhead is still alive during native destruction
};
static void*create(void*user,uint32_t id,double rate,const float*parameters){
    auto*c=static_cast<fx_native_context*>(user);
    /* Vintage's stock parameter callback251aef0 reaches the global plug-in
     * bank (200fb40), rather than an isolated DSP. Do not create that wrapper
     * in the independent resampling chain. */
    if(!id||!fx_catalog_find(id)||!parameters||!c||!c->base||!std::isfinite(rate)||rate<8000||rate>192000||!std::isfinite(c->bpm)||c->bpm<20||c->bpm>999||!c->numerator||!c->denominator)return nullptr;
    auto*p=new(std::nothrow)Processor{};if(!p)return nullptr;
    try{
        alignas(8) unsigned char description[112]{};F(description,72,uint32_t)=id;
        for(void*format:{c->air_format,c->mpc_format}){
            if(!format)continue;
            uintptr_t expected=format==c->air_format?0x6eaccd8:0x6eacf78;
            if(F(format,0,uintptr_t)!=c->base+expected)continue;
            /* ACVX uses process-wide rate lookup tables. Never rebuild them
             * for one sample while the sequencer is using them. AIR and the
             * other MPC processors have independent rate preparation. */
            if(expected==0x6eacf78&&(id&0xffffff00u)==0x41435600u){
                float engine_rate=F(reinterpret_cast<void*>(c->base+0x7173b80),0,float);
                if(!std::isfinite(engine_rate)||std::fabs(rate-engine_rate)>.5){std::fprintf(stderr,"RESAMPLE FX_RATE_MISMATCH id=%08x source=%.0f engine=%.0f\n",id,rate,(double)engine_rate);continue;}
            }
            Callback callback=[&](NativePtr object,const Text&){p->object=std::move(object);};
            auto*v=F(format,0,uintptr_t*);
            reinterpret_cast<void(*)(void*,const void*,double,int,Callback)>(v[13])(format,description,rate,256,std::move(callback));
            if(p->object)break;
        }
        if(!p->object)throw 0;
        auto*v=F(p->object.get(),0,uintptr_t*);
        unsigned n=(unsigned)reinterpret_cast<int(*)(void*)>(v[24])(p->object.get());if(!n||n>FX_CHAIN_PARAMETERS)throw 0;
        for(unsigned i=0;i<n;++i)reinterpret_cast<void(*)(void*,int,float)>(v[40])(p->object.get(),(int)i,parameters[i]);
        p->playhead.bpm=c->bpm;p->playhead.rate=rate;p->playhead.numerator=c->numerator;p->playhead.denominator=c->denominator;
        p->position=c->position;p->position_user=c->position_user;p->playhead.live=c->position!=nullptr;
        if(p->playhead.live)std::memcpy(p->playhead.current,c->initial_position,88);
        reinterpret_cast<void(*)(void*,void*)>(c->base+0xe9fe30)(p->object.get(),&p->playhead);
        reinterpret_cast<void(*)(void*,double,int)>(v[4])(p->object.get(),rate,256);
        F(p->audio,0,int)=2;F(p->audio,16,float**)=p->channels;
        std::fprintf(stderr,"RESAMPLE NATIVE_FX_READY id=%08x parameters=%u\n",id,n);return p;
    }catch(...){delete p;std::fprintf(stderr,"RESAMPLE NATIVE_FX_FAILED id=%08x\n",id);return nullptr;}
}
static void destroy(void*,void*processor){delete static_cast<Processor*>(processor);}
static void process(void*,void*processor,float*data,size_t frames){
    auto*p=static_cast<Processor*>(processor);auto*v=F(p->object.get(),0,uintptr_t*);
    alignas(8) unsigned char native_position[88]{};
    if(p->position){
        uint64_t block;
        if(!p->position(p->position_user,native_position,&block))return;
        if(!p->have_block||p->block!=block){p->block=block;p->offset=0;p->have_block=true;}
    }
    for(size_t at=0;at<frames;){size_t n=frames-at;if(n>256)n=256;
        if(p->position){
            std::memcpy(p->playhead.current,native_position,88);
            /* fx_exchange also splits calls at256. Offset belongs to each
             * processor/block, never a global counter shared by channels. */
            if(native_position[60]&1){
                double advance=(p->offset+at)/p->playhead.rate;
                F(p->playhead.current,16,int64_t)+=(int64_t)(p->offset+at);
                F(p->playhead.current,24,double)+=advance;
                double ppq=F(p->playhead.current,40,double)+advance*F(native_position,0,double)/60.;
                F(p->playhead.current,40,double)=ppq;
                double bar=F(native_position,8,unsigned)*4./F(native_position,12,unsigned);
                F(p->playhead.current,48,double)=std::floor(ppq/bar)*bar;
            }
        }
        for(size_t i=0;i<n;++i){p->left[i]=data[(at+i)*2];p->right[i]=data[(at+i)*2+1];}
        F(p->audio,4,int)=(int)n;
        reinterpret_cast<void(*)(void*,void*,void*)>(v[7])(p->object.get(),p->audio,p->midi);
        for(size_t i=0;i<n;++i){data[(at+i)*2]=p->left[i];data[(at+i)*2+1]=p->right[i];}
        p->playhead.frame+=n;at+=n;
    }
    if(p->position)p->offset+=frames;
}
}
extern "C" int fx_native_context_from_manager(uintptr_t base,void*manager,fx_native_context*out){
    if(!out)return 0;
    *out={};
    /* The successful ctor1ff3ab0 installs PluginManager6da1f90.
     *6e78360 is the PluginLoader base, written on destructor/unwind paths. */
    if(!base||!manager||F(manager,0,uintptr_t)!=base+0x6da1f90)return 0;
    /* PluginManager ctor1ff3ab0 constructs AudioPluginFormatManager at70.
     * Its JUCE OwnedArray is {data,allocated,count}; count lives at7c. */
    void**formats=F(manager,0x70,void**);int count=F(manager,0x7c,int);
    if(!formats||count<1||count>32)return 0;
    out->base=base;
    for(int i=0;i<count;++i){void*p=formats[i];if(!p)continue;uintptr_t type=F(p,0,uintptr_t);
        if(type==base+0x6eaccd8)out->air_format=p;
        if(type==base+0x6eacf78)out->mpc_format=p;
    }
    /* Tempo must be supplied explicitly from a proven owner snapshot.
     * Do not silently render tempo-synced effects at a guessed 120 BPM. */
    return out->air_format&&out->mpc_format;
}
extern "C" void fx_native_backend(fx_native_context*c,fx_backend*out){if(out)*out={c,create,destroy,process};}
extern "C" int fx_native_application_formats(uintptr_t base,fx_native_context*out){
    if(!out)return 0;
    *out={};if(!base)return 0;
    void*app=F(reinterpret_cast<void*>(base),0x71e3dc8,void*);
    if(!app||F(app,0,uintptr_t)!=base+0x6e07288)return 0;
    void*audio=F(app,0xd8,void*);
    if(!audio||F(audio,0,uintptr_t)!=base+0x6e0c430)return 0;
    void*core=F(audio,0x148,void*);
    return core&&F(core,0,uintptr_t)==base+0x6e0cf38&&
        fx_native_context_from_manager(base,F(core,0x4d0,void*),out);
}
extern "C" void fx_native_registry_bind(void*p){registry.store(p,std::memory_order_release);}
extern "C" int fx_native_context_capture(uintptr_t base,fx_native_context*out){
    if(!out)return 0;
    *out={};void*s=registry.load(std::memory_order_acquire);if(!base||!s){std::fprintf(stderr,"RESAMPLE FX_CAPTURE_REJECTED stage=registry\n");return 0;}
    /* These services are owned by the application, not registered under
     * their concrete RTTI in the Sample UI registry. Never call its inserting
     * lookup143ad50 for unregistered types. Pinned1.3.0.12 owner chain:
     * feec30 writes JUCEApplicationBase instance71e3dc8; AC50 ctor14379b0
     * installs6e07288, creates AudioIO at d8 and UI registry at f8;
     * AudioIO ctor14ea860 installs6e0c430 and stores AC50Core at148;
     * core ctor14ee2b0 installs6e0cf38, playhead490, PluginManager4d0.
     * Capture runs on the control owner while the application is alive. */
    void*application=F(reinterpret_cast<void*>(base),0x71e3dc8,void*);
    if(!application||F(application,0,uintptr_t)!=base+0x6e07288||F(application,0xf8,void*)!=s){
        std::fprintf(stderr,"RESAMPLE FX_CAPTURE_REJECTED stage=application_owner\n");return 0;
    }
    void*audio=F(application,0xd8,void*);
    if(!audio||F(audio,0,uintptr_t)!=base+0x6e0c430){std::fprintf(stderr,"RESAMPLE FX_CAPTURE_REJECTED stage=audio_owner\n");return 0;}
    void*core=F(audio,0x148,void*);
    if(!core||F(core,0,uintptr_t)!=base+0x6e0cf38){std::fprintf(stderr,"RESAMPLE FX_CAPTURE_REJECTED stage=core_owner\n");return 0;}
    void*manager=F(core,0x4d0,void*);void*playhead=F(core,0x490,void*);
    if(!fx_native_context_from_manager(base,manager,out)){
        uintptr_t type=manager?F(manager,0,uintptr_t):0;
        std::fprintf(stderr,"RESAMPLE FX_CAPTURE_REJECTED stage=manager present=%u type=%llx\n",manager!=nullptr,(unsigned long long)(type>=base&&type-base<0x7500000?type-base:0));
        *out={};return 0;
    }
    if(!playhead||F(playhead,0,uintptr_t)!=base+0x6d8a418){
        uintptr_t type=playhead?F(playhead,0,uintptr_t):0;
        std::fprintf(stderr,"RESAMPLE FX_CAPTURE_REJECTED stage=playhead present=%u type=%llx\n",playhead!=nullptr,(unsigned long long)(type>=base&&type-base<0x7500000?type-base:0));
        *out={};return 0;
    }
    /* Actual MpcPlayHead::getCurrentPosition13ad070 copies88 bytes from
     * its position, exactly the structure consumed by AIR245f740. */
    alignas(8) unsigned char position[88]{};
    if(!reinterpret_cast<int(*)(void*,void*)>(base+0x13ad070)(playhead,position)){*out={};return 0;}
    out->bpm=F(position,0,double);out->numerator=F(position,8,unsigned);out->denominator=F(position,12,unsigned);
    if(!std::isfinite(out->bpm)||out->bpm<20||out->bpm>999||!out->numerator||!out->denominator){std::fprintf(stderr,"RESAMPLE FX_CAPTURE_REJECTED stage=tempo\n");*out={};return 0;}
    return 1;
}
