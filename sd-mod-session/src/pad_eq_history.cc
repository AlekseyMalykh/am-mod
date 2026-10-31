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
    uintptr_t page=reinterpret_cast<uintptr_t>(v)&~uintptr_t(4095);
    if(mprotect(reinterpret_cast<void*>(page),4096,PROT_READ|PROT_WRITE))return 0;
    base=b;v[0]=reinterpret_cast<uintptr_t>(destroy);v[1]=reinterpret_cast<uintptr_t>(delete_command);v[8]=reinterpret_cast<uintptr_t>(perform);v[9]=reinterpret_cast<uintptr_t>(undo);
    if(mprotect(reinterpret_cast<void*>(page),4096,PROT_READ))_exit(121);
    return 1;
}
