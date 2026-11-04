/* Test-owned native services. Factories, prepare, DSP, playhead setter and
 * processor destructors remain the pinned native implementations. */
#include "src/fx_native.h"
#include "src/mixer_live.h"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <map>
#include <sys/mman.h>
extern "C" int mixer_fx_test_audio(void);
namespace {
uintptr_t image;
template<class T>T&f(void*p,unsigned offset){return *reinterpret_cast<T*>(static_cast<char*>(p)+offset);}
alignas(16) unsigned char presets[256],air[24],mpc[624],manager[0x248],playhead[0x100],app[0x100],audio[0x198],core[0x7b8];
void *preset_pointer=presets,*formats[2]={air,mpc},*prior_app;
uintptr_t preset_methods[3];
struct Text {
    const char*data;
    Text(){reinterpret_cast<void(*)(void*)>(image+0xf1c620)(this);}
    ~Text(){reinterpret_cast<void(*)(void*)>(image+0xf0a290)(this);}
};
static Text directory(void*,const void*){return {};}
struct Method {uintptr_t *table,process,destroy;} methods[64];
struct Instance {void *object;bool alive;uint64_t block,offset;bool seen;} instances[512];
unsigned method_count,instance_count,destroy_count,process_count;
uint64_t epoch;
void *setter_clone;
uintptr_t air_type;
static Method&method(void *object)
{auto *table=f<uintptr_t*>(object,0);for(unsigned i=0;i<method_count;++i)if(methods[i].table==table)return methods[i];assert(false);return methods[0];}
static Instance&instance(void *object)
{for(unsigned i=instance_count;i;--i)if(instances[i-1].object==object&&instances[i-1].alive)return instances[i-1];assert(false);return instances[0];}
static void process(void *object,void *buffer,void *midi)
{
    assert(mixer_fx_test_audio());++process_count;
    auto &state=instance(object);alignas(8) unsigned char actual[88]{},native[88]{};uint64_t block;
    assert(mixer_live_native_position(epoch,native,&block));
    void *clock=f<void*>(object,16);auto *v=f<uintptr_t*>(clock,0);
    assert(reinterpret_cast<int(*)(void*,void*)>(v[2])(clock,actual));
    if(!state.seen||state.block!=block){state.offset=0;state.block=block;state.seen=true;}
    double seconds=(native[60]&1)?state.offset/48000.:0;
    int64_t frame=f<int64_t>(native,16)+((native[60]&1)?(int64_t)state.offset:0);
    assert(f<int64_t>(actual,16)==frame&&f<double>(actual,0)==f<double>(native,0));
    assert(f<unsigned>(actual,8)==f<unsigned>(native,8)&&f<unsigned>(actual,12)==f<unsigned>(native,12));
    assert(actual[60]==native[60]&&actual[61]==native[61]&&actual[62]==native[62]);
    assert(std::fabs(f<double>(actual,24)-f<double>(native,24)-seconds)<1e-9);
    assert(std::fabs(f<double>(actual,40)-f<double>(native,40)-seconds*f<double>(native,0)/60)<1e-9);
    auto callback=method(object).process;
    reinterpret_cast<void(*)(void*,void*,void*)>(callback)(object,buffer,midi);
    state.offset+=(unsigned)f<int>(buffer,4);
}
static void destroy(void *object)
{
    assert(!mixer_fx_test_audio());auto callback=method(object).destroy;
    instance(object).alive=false;++destroy_count;reinterpret_cast<void(*)(void*)>(callback)(object);
}
static void set_playhead(void *object,void *clock)
{
    assert(!mixer_fx_test_audio());reinterpret_cast<void(*)(void*,void*)>(setter_clone)(object,clock);
    if(!clock)return;
    assert(instance_count<512);instances[instance_count++]={object,true,0,0,false};
    auto *table=f<uintptr_t*>(object,0);unsigned i=0;
    for(;i<method_count;++i)if(methods[i].table==table)break;
    if(i==method_count){
        assert(method_count<64);methods[method_count++]={table,table[7],table[1]};
        table[7]=reinterpret_cast<uintptr_t>(process);table[1]=reinterpret_cast<uintptr_t>(destroy);
    }
}
}
extern "C" void mixer_fx_fixture_position(uint64_t source,double bpm,unsigned numerator,unsigned denominator,int64_t frame,int playing,double ppq)
{
    epoch=source;std::memset(playhead+8,0,88);f<double>(playhead,8)=bpm;
    f<unsigned>(playhead,16)=numerator;f<unsigned>(playhead,20)=denominator;f<int64_t>(playhead,24)=frame;
    f<double>(playhead,32)=frame/48000.;f<double>(playhead,48)=ppq;
    f<double>(playhead,56)=std::floor(ppq/(numerator*4./denominator))*(numerator*4./denominator);
    playhead[68]=playing?1:0;playhead[70]=1; // isPlaying, isLooping.
}
extern "C" void mixer_fx_fixture_begin(uintptr_t base)
{
    image=base;reinterpret_cast<void(*)(float)>(base+0xd8d320)(48000.f);
    preset_methods[2]=reinterpret_cast<uintptr_t>(directory);f<uintptr_t*>(presets,0)=preset_methods;
    new(presets+8)std::map<int,int>();new(presets+0x68)std::map<int,int>();
    reinterpret_cast<void(*)(void*,void*)>(base+0x2530ee0)(air,&preset_pointer);
    air_type=f<uintptr_t>(air,0);
    f<uintptr_t>(mpc,0)=base+0x6eacf78;f<void*>(mpc,32)=&preset_pointer;
    f<uintptr_t>(manager,0)=base+0x6da1f90;f<void*>(manager,0x70)=formats;f<int>(manager,0x7c)=2;
    f<uintptr_t>(app,0)=base+0x6e07288;f<void*>(app,0xd8)=audio;
    f<uintptr_t>(audio,0)=base+0x6e0c430;f<void*>(audio,0x148)=core;
    f<uintptr_t>(core,0)=base+0x6e0cf38;f<void*>(core,0x4d0)=manager;f<void*>(core,0x490)=playhead;
    f<uintptr_t>(playhead,0)=base+0x6d8a418;
    auto &slot=*reinterpret_cast<void**>(base+0x71e3dc8);prior_app=slot;slot=app;
    mixer_fx_fixture_position(1,123,7,8,0,0,0);
    setter_clone=mmap(nullptr,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(setter_clone!=MAP_FAILED);
    std::memcpy(setter_clone,reinterpret_cast<void*>(base+0xe9fe30),12);
    __builtin___clear_cache(static_cast<char*>(setter_clone),static_cast<char*>(setter_clone)+12);
    assert(!mprotect(setter_clone,4096,PROT_READ|PROT_EXEC));
    uint32_t code[]={0x58000050,0xd61f0200};void *fn=reinterpret_cast<void*>(set_playhead);
    std::memcpy(reinterpret_cast<void*>(base+0xe9fe30),code,8);std::memcpy(reinterpret_cast<void*>(base+0xe9fe38),&fn,8);
    __builtin___clear_cache(reinterpret_cast<char*>(base+0xe9fe30),reinterpret_cast<char*>(base+0xe9fe40));
}
extern "C" void mixer_fx_fixture_counts(unsigned *created,unsigned *destroyed,unsigned *processed)
{*created=instance_count;*destroyed=destroy_count;*processed=process_count;}
extern "C" void mixer_fx_fixture_factory_ready(int ready)
{f<uintptr_t>(air,0)=ready?air_type:0;f<uintptr_t>(mpc,0)=ready?image+0x6eacf78:0;}
extern "C" void mixer_fx_fixture_clock_ready(int ready)
{f<uintptr_t>(playhead,0)=ready?image+0x6d8a418:0;}
extern "C" void mixer_fx_fixture_end(void)
{
    assert(instance_count==destroy_count);
    for(unsigned i=0;i<method_count;++i){methods[i].table[7]=methods[i].process;methods[i].table[1]=methods[i].destroy;}
    std::memcpy(reinterpret_cast<void*>(image+0xe9fe30),setter_clone,12);
    __builtin___clear_cache(reinterpret_cast<char*>(image+0xe9fe30),reinterpret_cast<char*>(image+0xe9fe40));
    munmap(setter_clone,4096);
    reinterpret_cast<void(*)(void*)>(image+0x2530e30)(air);
    reinterpret_cast<std::map<int,int>*>(presets+8)->~map();reinterpret_cast<std::map<int,int>*>(presets+0x68)->~map();
    *reinterpret_cast<void**>(image+0x71e3dc8)=prior_app;
}
