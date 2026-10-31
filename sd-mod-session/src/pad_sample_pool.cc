#include "pad_sample_pool.h"
#include <cstdint>
extern "C" void pad_memory_pool_add(void*,const uintptr_t*,uintptr_t*,uintptr_t);
static bool valid(uintptr_t b,void*p,const uintptr_t*s){return b&&p&&s&&s[0]&&s[1]&&*static_cast<uintptr_t*>(p)==b+0x6dbd2f0;}
extern "C" int pad_sample_pool_contains(uintptr_t b,void*p,const uintptr_t s[2]){
    if(!valid(b,p,s))return 0;
    uint64_t result=reinterpret_cast<uint64_t(*)(void*,const uintptr_t*)>(b+0x292eb00)(p,s);
    return ((result>>32)&255)!=0;
}
extern "C" int pad_sample_pool_register(uintptr_t b,void*p,const uintptr_t s[2]){
    if(!valid(b,p,s))return 0;
    if(pad_sample_pool_contains(b,p,s))return 1;
    uintptr_t added[2]={};bool ok=false;
    try{pad_memory_pool_add(p,s,added,b+0x2931440);ok=added[0]==s[0]&&pad_sample_pool_contains(b,p,s);}
    catch(...){ok=false;}
    if(added[1])reinterpret_cast<void(*)(void*)>(b+0x123bca0)(reinterpret_cast<void*>(added[1]));
    if(!ok&&pad_sample_pool_contains(b,p,s))pad_sample_pool_remove(b,p,s);
    return ok;
}
extern "C" int pad_sample_pool_remove(uintptr_t b,void*p,const uintptr_t s[2]){
    if(!valid(b,p,s))return 0;
    if(!pad_sample_pool_contains(b,p,s))return 1;
    try{reinterpret_cast<void(*)(void*,const uintptr_t*)>(b+0x2930010)(p,s);}
    catch(...){return 0;}
    return !pad_sample_pool_contains(b,p,s);
}
