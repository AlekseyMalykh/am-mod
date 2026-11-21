/* Disposable-lab-only experiment. This is NOT the shipping import adapter.
 * No constructor action; one explicit /run request is consumed on the real
 * native UI Timer. Build flag is absent from normal builds. */
#include "program_pgm.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <new>
#include <unistd.h>
extern "C" {
#include "note_native.h"
int batch_copy_import_program_submit(uintptr_t,void*,void*,const unsigned*,const unsigned*,unsigned);
void step_layer_sample(void*,int,uintptr_t*,uintptr_t);
}
#define F(p,o,T) (*reinterpret_cast<T*>(reinterpret_cast<char*>(p)+(o)))
extern "C" void pad_assignment_pool_find(void*,const uint64_t*,uintptr_t*,uintptr_t);
static unsigned check_samples(uintptr_t b,void*core,const am_pgm_program&pgm){
    void*properties=F(core,0x4b8,void*);void*pool=F(properties,0x1278,void*);
    unsigned found=0,total=0;
    for(unsigned pad=0;pad<64;++pad)for(unsigned layer=0;layer<4;++layer){
        const char*name=pgm.pads[pad].layers[layer].name;
        if(!*name)continue;
        ++total;uint64_t key=0;uintptr_t sample[2]={};
        reinterpret_cast<void(*)(void*,const char*)>(b+0xf535c0)(&key,name);
        pad_assignment_pool_find(pool,&key,sample,b+0x292ed50);
        reinterpret_cast<void(*)(void*)>(b+0xf0a290)(&key);
        int frames=sample[0]?reinterpret_cast<int(*)(void*)>(b+0x2941f80)(reinterpret_cast<void*>(sample[0])):0;
        if(frames>0)++found;
        std::fprintf(stderr,"PGM_NATIVE_PROBE SAMPLE pad=%u layer=%u name=%s frames=%d\n",pad,layer,name,frames);
        if(sample[1])reinterpret_cast<void(*)(void*)>(b+0x123bca0)(reinterpret_cast<void*>(sample[1]));
    }
    std::fprintf(stderr,"PGM_NATIVE_PROBE SAMPLES found=%u expected=%u\n",found,total);
    return found==total?found:0;
}
static void report(uintptr_t b,void*core,const char*stage){
    void*properties=F(core,0x4b8,void*);
    void*project=F(properties,0x1288,void*);
    void*list=reinterpret_cast<void*(*)(void*)>(b+0x2925db0)(project);
    uintptr_t begin=F(list,8,uintptr_t),end=F(list,16,uintptr_t);
    std::fprintf(stderr,"PGM_NATIVE_PROBE %s project=%p tracks=%llu\n",stage,project,
        static_cast<unsigned long long>(end>=begin?(end-begin)/8:999));
    if(end<begin||end-begin>1024*8)return;
    for(uintptr_t at=begin;at<end;at+=8){
        void*t=*reinterpret_cast<void**>(at);
        std::fprintf(stderr,"PGM_NATIVE_PROBE track=%llu ptr=%p vptr=%llx\n",
            static_cast<unsigned long long>((at-begin)/8),t,
            static_cast<unsigned long long>(F(t,0,uintptr_t)-b));
    }
}
extern "C" void program_native_probe_tick(uintptr_t b){
    static unsigned ticks=0;static bool done=false;
    if(done||(++ticks%20))return;
    FILE*f=std::fopen("/run/am-pgm-native-probe.request","r");if(!f)return;
    char path[1024]={};char*got=std::fgets(path,sizeof(path),f);std::fclose(f);
    done=true;unlink("/run/am-pgm-native-probe.request");
    if(!got){done=false;std::fprintf(stderr,"PGM_NATIVE_PROBE EMPTY_REQUEST retry_allowed\n");return;}
    path[std::strcspn(path,"\r\n")]=0;
    const char*prefix="/media/MPCSAMPLE/MPC-Sample/Samples/Export/PGMTrial/";
    if(std::strncmp(path,prefix,std::strlen(prefix))||std::strstr(path,"..")){
        std::fprintf(stderr,"PGM_NATIVE_PROBE REJECT_PATH\n");return;
    }
    unsigned char bytes[AM_PGM_BYTES+1];f=std::fopen(path,"rb");
    if(!f){std::fprintf(stderr,"PGM_NATIVE_PROBE OPEN_FAILED path=%s\n",path);return;}
    size_t n=std::fread(bytes,1,sizeof(bytes),f);std::fclose(f);
    am_pgm_program pgm;am_pgm_error error;
    if(am_pgm_read(bytes,n,&pgm,&error)!=AM_PGM_OK){std::fprintf(stderr,"PGM_NATIVE_PROBE REJECT_PGM\n");return;}
    void*app=F(reinterpret_cast<void*>(b),0x71e3dc8,void*);
    if(!app||F(app,0,uintptr_t)!=b+0x6e07288)return;
    void*audio=F(app,0xd8,void*);if(!audio||F(audio,0,uintptr_t)!=b+0x6e0c430)return;
    void*core=F(audio,0x148,void*);if(!core||F(core,0,uintptr_t)!=b+0x6e0cf38)return;
    std::fprintf(stderr,"PGM_NATIVE_PROBE START path=%s core=%p\n",path,core);std::fflush(stderr);
    report(b,core,"BEFORE");
    uint64_t file=0;reinterpret_cast<void(*)(void*,const char*)>(b+0xf535c0)(&file,path);
    /* The full-MPC UI helper at 1dca610 reads full-engine field offsets.
     * They differ in AC50Engine. Never call it with an AC50 owner.
     * Exercise the standalone reader with AC50 services instead.
     * Attaching requires a second explicit VM-only request file. */
    void*resolver=F(core,0x560,void*);
    void*queues=F(core,0x1d0,void*);
    void*factory=F(core,0x558,void*);
    if(!resolver||F(resolver,0,uintptr_t)!=b+0x6d9c990||!queues||
       F(queues,0,uintptr_t)!=b+0x6dbfa08||!factory){
        std::fprintf(stderr,"PGM_NATIVE_PROBE REJECT_SERVICES\n");
        reinterpret_cast<void(*)(void*)>(b+0xf0a290)(&file);return;
    }
    struct NativeError{int64_t code;std::string message;};
    static_assert(sizeof(NativeError)==40,"Pinned error ABI");
    void*loader=nullptr;bool constructed=false;
    try{
        loader=::operator new(0x60);
        reinterpret_cast<void(*)(void*,void*,void*,void*)>(b+0x1c83510)
            (loader,resolver,queues,factory);
        constructed=true;
        std::fprintf(stderr,"PGM_NATIVE_PROBE CONSTRUCTED resolver=%p factory=%p\n",resolver,factory);
        std::fflush(stderr);
        uint64_t empty=0;reinterpret_cast<void(*)(void*)>(b+0xf1c620)(&empty);
        auto errors=reinterpret_cast<std::vector<NativeError>(*)(void*,const void*,const void*,int)>
            (b+0x1c857e0)(loader,&file,&empty,0);
        reinterpret_cast<void(*)(void*)>(b+0xf0a290)(&empty);
        std::fprintf(stderr,"PGM_NATIVE_PROBE READ errors=%zu program=%p\n",errors.size(),F(loader,0x50,void*));
        for(const auto&e:errors)std::fprintf(stderr,"PGM_NATIVE_PROBE ERROR code=%lld message=%s\n",
            static_cast<long long>(e.code),e.message.c_str());
        report(b,core,"RETURNED_DETACHED");
        void*program=F(loader,0x50,void*);
        if(errors.empty()&&program&&check_samples(b,core,pgm)&&
           access("/run/am-pgm-native-probe.attach",F_OK)==0){
            unlink("/run/am-pgm-native-probe.attach");
            void*commands=F(core,0x588,void*),*things=note_native_context();
            if(commands&&things&&F(commands,0,uintptr_t)==b+0x6d95318){
                void*target=F(things,0xb8,void*);
                uint8_t occupied[128]={},mapping[64];unsigned src[64],dst[64],count=0;
                for(unsigned i=0;i<128;++i){
                    void*pad=reinterpret_cast<void*(*)(void*,int)>(b+0x286bbe0)(target,i);
                    occupied[i]=!pad||reinterpret_cast<int(*)(void*)>(b+0x2804760)(pad);
                }
                if(!am_pgm_plan(&pgm,16,occupied,mapping)){
                    std::fprintf(stderr,"PGM_NATIVE_PROBE IMPORT_REJECT occupied_or_range\n");
                }else{
                    for(unsigned i=0;i<64;++i)if(mapping[i]!=255){src[count]=i;dst[count++]=mapping[i];}
                    /* Only these three borrowed fields are used by Batch
                     * prepare/dispatch; no fake context reaches native code. */
                    alignas(8) unsigned char context[0xd0]={};
                    F(context,0x68,void*)=F(commands,0x60,void*);
                    F(context,0x78,void*)=things;F(context,0xc8,void*)=F(core,0x4b8,void*);
                    std::fprintf(stderr,"PGM_NATIVE_PROBE ATTACH_BEGIN pads=%u first=16\n",count);std::fflush(stderr);
                    int result=batch_copy_import_program_submit(b,context,program,src,dst,count);
                    std::fprintf(stderr,"PGM_NATIVE_PROBE IMPORT_RETURN submitted=%d\n",result);
                    unsigned matched=0,total=0;
                    for(unsigned row=0;row<count;++row){
                        void*from=reinterpret_cast<void*(*)(void*,int)>(b+0x286bbe0)(program,src[row]);
                        void*to=reinterpret_cast<void*(*)(void*,int)>(b+0x286bbe0)(target,dst[row]);
                        for(unsigned layer=0;layer<4;++layer)if(pgm.pads[src[row]].layers[layer].name[0]){
                            uintptr_t a[2]={},z[2]={};++total;
                            step_layer_sample(from,layer,a,b+0x2803b90);step_layer_sample(to,layer,z,b+0x2803b90);
                            if(a[0]&&a[0]==z[0])++matched;
                            std::fprintf(stderr,"PGM_NATIVE_PROBE ASSIGNED source=%u target=%u layer=%u matched=%d\n",src[row],dst[row],layer,a[0]&&a[0]==z[0]);
                            if(a[1])reinterpret_cast<void(*)(void*)>(b+0x123bca0)(reinterpret_cast<void*>(a[1]));
                            if(z[1])reinterpret_cast<void(*)(void*)>(b+0x123bca0)(reinterpret_cast<void*>(z[1]));
                        }
                    }
                    std::fprintf(stderr,"PGM_NATIVE_PROBE ASSIGNMENTS matched=%u expected=%u\n",matched,total);
                    report(b,core,"AFTER_IMPORT");
                }
            }
        }
    }catch(...){std::fprintf(stderr,"PGM_NATIVE_PROBE EXCEPTION\n");}
    if(constructed)reinterpret_cast<void(*)(void*)>(b+0x1c862f0)(loader);
    if(loader)::operator delete(loader);
    reinterpret_cast<void(*)(void*)>(b+0xf0a290)(&file);std::fflush(stderr);
}
