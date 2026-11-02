#include "pad_eq_history.h"
#include "music_settings.h"
#include <mutex>
#include <unordered_map>
#include <sys/mman.h>
#include <unistd.h>
#define F(p,o,T) (*reinterpret_cast<T*>(reinterpret_cast<char*>(p)+(o)))
static uintptr_t base;
struct Saved {unsigned pad;uint64_t eq;};
static std::mutex mutex;
static std::unordered_map<void*,Saved>saved;
/* CopyPadCommand may address several destinations; Extract has one. Keep
 * snapshots for the lifetime of the native Undo command, including Redo.
 * Batch commands call native bodies directly and own their own snapshots. */
struct Colours {uint64_t low=0,high=0;unsigned source=0;unsigned char targets[128]{};};
static std::unordered_map<void*,Colours>colours;
static bool target(const Colours&s,unsigned p){return ((p<64?s.low:s.high)>>(p%64))&1u;}
static bool capture_colours(void*p,bool extract,Colours&out){
    try{
        std::lock_guard<std::mutex>lock(mutex);
        auto i=colours.find(p);if(i!=colours.end()){out=i->second;return true;}
        unsigned source;
        if(extract){
            void*instrument=F(p,0x68,void*);if(!instrument)return false;
            source=F(instrument,0x2168,unsigned);unsigned to=F(p,0x58,unsigned);
            if(to>=128)return false;
            if(to<64)out.low=UINT64_C(1)<<to;else out.high=UINT64_C(1)<<(to-64);
        }else{
            source=F(p,0x68,unsigned);out.low=F(p,0x70,uint64_t);out.high=F(p,0x78,uint64_t);
        }
        if(source>=128||(!out.low&&!out.high))return false;
        out.source=(unsigned)music_pad_colour(source);
        for(unsigned pad=0;pad<128;++pad)if(target(out,pad))out.targets[pad]=(unsigned char)music_pad_colour(pad);
        colours.emplace(p,out);return true;
    }catch(...){return false;} /* Do not mutate a destination without its Undo metadata. */
}
static int colour_perform(void*p,bool extract){
    Colours s;if(!capture_colours(p,extract,s))return 1;
    int status=reinterpret_cast<int(*)(void*)>(base+(extract?0x143f0b0:0x1987530))(p);
    if(!status)for(unsigned pad=0;pad<128;++pad)if(target(s,pad))music_set_pad_colour(pad,s.source);
    return status;
}
static int colour_undo(void*p,bool extract){
    Colours s;bool found;
    {std::lock_guard<std::mutex>lock(mutex);auto i=colours.find(p);found=i!=colours.end();if(found)s=i->second;}
    int status=reinterpret_cast<int(*)(void*)>(base+(extract?0x143ee90:0x1987890))(p);
    if(!status&&found)for(unsigned pad=0;pad<128;++pad)if(target(s,pad))music_set_pad_colour(pad,s.targets[pad]);
    return status;
}
static void colour_forget(void*p){std::lock_guard<std::mutex>lock(mutex);colours.erase(p);}
static int copy_perform(void*p){return colour_perform(p,false);}
static int copy_undo(void*p){return colour_undo(p,false);}
static int extract_perform(void*p){return colour_perform(p,true);}
static int extract_undo(void*p){return colour_undo(p,true);}
static void copy_destroy(void*p){colour_forget(p);reinterpret_cast<void(*)(void*)>(base+0x1986790)(p);}
static void copy_delete(void*p){colour_forget(p);reinterpret_cast<void(*)(void*)>(base+0x1986890)(p);}
static void extract_destroy(void*p){colour_forget(p);reinterpret_cast<void(*)(void*)>(base+0x143fe20)(p);}
static void extract_delete(void*p){colour_forget(p);reinterpret_cast<void(*)(void*)>(base+0x143feb0)(p);}
static int perform(void*command){
    void*instrument=F(command,0x60,void*);if(!instrument)return 1;
    unsigned pad=F(instrument,0x2168,unsigned);if(pad>=128)return 1;
    /* Reserve metadata before the native command can mutate anything. A
     * failed allocation must leave both native destination and EQ intact. */
    try{std::lock_guard<std::mutex>lock(mutex);if(!saved.count(command))saved.emplace(command,Saved{pad,music_get_pad_eq(pad)});}catch(...){return 1;}
    int status=reinterpret_cast<int(*)(void*)>(base+0x14913b0)(command);
    if(!status)music_set_pad_eq(pad,0);
    return status;
}
static int undo(void*command){
    Saved previous{};{std::lock_guard<std::mutex>lock(mutex);auto i=saved.find(command);if(i==saved.end())return reinterpret_cast<int(*)(void*)>(base+0x1491040)(command);previous=i->second;}
    int status=reinterpret_cast<int(*)(void*)>(base+0x1491040)(command);
    if(!status)music_set_pad_eq(previous.pad,previous.eq);
    return status;
}
static void forget(void*p){std::lock_guard<std::mutex>lock(mutex);saved.erase(p);}
static void destroy(void*p){forget(p);reinterpret_cast<void(*)(void*)>(base+0x1491600)(p);}
static void delete_command(void*p){forget(p);reinterpret_cast<void(*)(void*)>(base+0x1491670)(p);}
extern "C" int pad_eq_history_install(uintptr_t b){
    if(base)return base==b;
    if(!b||sysconf(_SC_PAGESIZE)!=4096)return 0;
    auto*v=reinterpret_cast<uintptr_t*>(b+0x6e07e58);
    if(v[0]!=b+0x1491600||v[1]!=b+0x1491670||v[8]!=b+0x14913b0||v[9]!=b+0x1491040)return 0;
    auto*c=reinterpret_cast<uintptr_t*>(b+0x6e405e8);
    auto*e=reinterpret_cast<uintptr_t*>(b+0x6e07640);
    if(c[0]!=b+0x1986790||c[1]!=b+0x1986890||c[8]!=b+0x1987530||c[9]!=b+0x1987890||
       e[0]!=b+0x143fe20||e[1]!=b+0x143feb0||e[8]!=b+0x143f0b0||e[9]!=b+0x143ee90)return 0;
    uintptr_t page=reinterpret_cast<uintptr_t>(v)&~uintptr_t(4095);
    uintptr_t copy_page=reinterpret_cast<uintptr_t>(c)&~uintptr_t(4095);
    if(mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE))return 0;
    if(mprotect(reinterpret_cast<void*>(copy_page),4096,PROT_READ|PROT_WRITE)){
        if(mprotect(reinterpret_cast<void*>(page),4096,PROT_READ))_exit(121);
        return 0;
    }
    base=b;v[0]=reinterpret_cast<uintptr_t>(destroy);v[1]=reinterpret_cast<uintptr_t>(delete_command);v[8]=reinterpret_cast<uintptr_t>(perform);v[9]=reinterpret_cast<uintptr_t>(undo);
    c[0]=reinterpret_cast<uintptr_t>(copy_destroy);c[1]=reinterpret_cast<uintptr_t>(copy_delete);c[8]=reinterpret_cast<uintptr_t>(copy_perform);c[9]=reinterpret_cast<uintptr_t>(copy_undo);
    e[0]=reinterpret_cast<uintptr_t>(extract_destroy);e[1]=reinterpret_cast<uintptr_t>(extract_delete);e[8]=reinterpret_cast<uintptr_t>(extract_perform);e[9]=reinterpret_cast<uintptr_t>(extract_undo);
    if(mprotect(reinterpret_cast<void*>(page),4096,PROT_READ))_exit(121);
    if(mprotect(reinterpret_cast<void*>(copy_page),4096,PROT_READ))_exit(121);
    return 1;
}
