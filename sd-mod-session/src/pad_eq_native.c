#define _GNU_SOURCE
#include "pad_eq_native.h"
#include "pad_eq.h"
#include <stdatomic.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <math.h>
#include <stdio.h>
#if ATOMIC_LLONG_LOCK_FREE != 2
#error EQ diagnostics require lock-free counters
#endif
#define F(p,o,T) (*(T*)((char*)(p)+(o)))
static uintptr_t image;
static void(*original)(void*,void*,unsigned,unsigned);
static _Atomic uint64_t program_identity;
static _Atomic uint64_t calls,configured,matched,processed;
static uint64_t identity(void*p){
    if(!p)return 0;
    const unsigned char*s=F(p,0xdc8,const unsigned char*);if(!s)return 0;
    uint64_t hash=UINT64_C(1469598103934665603);unsigned n=0;
    for(;n<128&&s[n];++n)hash=(hash^s[n])*UINT64_C(1099511628211);
    return n&&n<128?hash:0;
}
void pad_eq_native_program(void*p){uint64_t id=identity(p);if(atomic_exchange_explicit(&program_identity,id,memory_order_acq_rel)!=id)fprintf(stderr,"PAD_EQ PROGRAM_BOUND valid=%u\n",id!=0);}
void pad_eq_native_stats_read(struct pad_eq_native_stats*s){if(s){s->calls=atomic_load_explicit(&calls,memory_order_relaxed);s->configured=atomic_load_explicit(&configured,memory_order_relaxed);s->matched=atomic_load_explicit(&matched,memory_order_relaxed);s->processed=atomic_load_explicit(&processed,memory_order_relaxed);}}
void pad_eq_native_report(void){struct pad_eq_native_stats s;pad_eq_native_stats_read(&s);fprintf(stderr,"PAD_EQ AUDIO calls=%llu configured=%llu matched=%llu processed=%llu bound=%u\n",(unsigned long long)s.calls,(unsigned long long)s.configured,(unsigned long long)s.matched,(unsigned long long)s.processed,atomic_load_explicit(&program_identity,memory_order_acquire)!=0);}
void pad_eq_native_clear(void){pad_eq_native_report();atomic_store_explicit(&program_identity,0,memory_order_release);}
static void process(void*instrument,void*buffer,unsigned frames,unsigned effects){
    original(instrument,buffer,frames,effects);
    atomic_fetch_add_explicit(&calls,1,memory_order_relaxed);
    uint64_t expected=atomic_load_explicit(&program_identity,memory_order_acquire);
    if(!expected||!instrument||!buffer||frames>8192)return;
    unsigned pad=F(instrument,0x2168,unsigned);if(pad>=128)return;
    int published=pad_eq_published(pad)!=0,watched=pad_eq_spectrum_wanted(pad);
    if(!published&&!watched)return;
    if(published)atomic_fetch_add_explicit(&configured,1,memory_order_relaxed);
    if(identity(F(instrument,0x2310,void*))!=expected)return;
    atomic_fetch_add_explicit(&matched,1,memory_order_relaxed);
    void*rate_property=F(instrument,0x638,void*);if(!rate_property)return;
    double rate=F(rate_property,0x50,float);int channels=F(buffer,0,int),capacity=F(buffer,4,int);
    if(channels<1||channels>2||capacity<0||frames>(unsigned)capacity||!isfinite(rate)||rate<8000||rate>192000)return;
    if(published){pad_eq_audio(pad,(uintptr_t)instrument,rate,F(buffer,16,float**),(unsigned)channels,frames,F(buffer,0x128,unsigned char)!=0);
    atomic_fetch_add_explicit(&processed,1,memory_order_relaxed);}
    if(watched&&!F(buffer,0x128,unsigned char))pad_eq_spectrum_capture(pad,F(buffer,16,float**),(unsigned)channels,frames,rate);
}
int pad_eq_native_install(uintptr_t base){
    if(image)return image==base;
    if(!base||sysconf(_SC_PAGESIZE)!=4096)return 0;
    const uint32_t expected[]={0xd503233f,0xd10643ff,0xd0024b04,0xf941cc84};
    void*entry=(void*)(base+0x2802f90);if(memcmp(entry,expected,sizeof(expected)))return 0;
    /* BTI, original PAC/SP prologue, absolute relocated ADRP value, guard
     * load, absolute continuation. No executable data in the audio buffer. */
    uint32_t code[]={0xd503245f,0xd503233f,0xd10643ff,0x580000a4,0xf941cc84,0x580000b0,0xd61f0200,0xd503201f};
    void*trampoline=mmap(NULL,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);if(trampoline==MAP_FAILED)return 0;
    memcpy(trampoline,code,sizeof(code));F(trampoline,32,uintptr_t)=base+0x7164000;F(trampoline,40,uintptr_t)=base+0x2802fa0;
    __builtin___clear_cache(trampoline,(char*)trampoline+48);
    if(mprotect(trampoline,4096,PROT_READ|PROT_EXEC)){munmap(trampoline,4096);return 0;}
    uintptr_t page=(uintptr_t)entry&~(uintptr_t)4095;
    if(mprotect((void*)page,4096,PROT_READ|PROT_WRITE)){munmap(trampoline,4096);return 0;}
    original=(void(*)(void*,void*,unsigned,unsigned))trampoline;image=base;
    const uint32_t jump[]={0x58000050,0xd61f0200};memcpy(entry,jump,8);F(entry,8,uintptr_t)=(uintptr_t)process;
    __builtin___clear_cache(entry,(char*)entry+16);
    if(mprotect((void*)page,4096,PROT_READ|PROT_EXEC))_exit(121);
    return 1;
}
