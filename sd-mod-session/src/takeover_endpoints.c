#define _GNU_SOURCE
#include "takeover_endpoints.h"
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#define F(p,o,T) (*(T*)((char*)(p)+(o)))
static uintptr_t image;
static void (*native_process)(void*,float);
static _Atomic unsigned reports;
static void process(void *binding,float physical)
{
    /* Firmware 1.3.0.12: Scaled=1, Pickup=0, Jump=2. The native Scaled
     * processor ignores the first movement after reset, and zero delta can
     * leave a stale parameter at an end stop. Treat exact physical limits
     * as a complete Scaled movement, using its own range/skew/quantisation
     * and parameter setter. Interior motion and other modes are unchanged. */
    if (binding && (physical<=1.f/127.f || physical>=126.f/127.f)) {
        int mode=F(binding,0xdc,int),active=F(binding,0x9b,unsigned char)&1;
        int exact=physical==0.f||physical==1.f,enabled=-1;
        if(exact&&mode==1&&active){
            void *slider=F(binding,0x60,void*);
            enabled=slider?((int(*)(void*))(image+0x10f98d0))(slider):0;
        }
        if(atomic_fetch_add(&reports,1)<96)
            fprintf(stderr,"TAKEOVER EDGE input=%g mode=%d active=%d enabled=%d exact=%d previous_hw=%g previous_scaled=%g\n",
                physical,mode,active,enabled,exact,F(binding,0x144,float),F(binding,0x148,float));
        if (enabled==1) {
            F(binding,0x144,float)=1.f-physical;
            F(binding,0x148,float)=-1.f;
        }
    }
    native_process(binding,physical);
}
static uint32_t branch(uintptr_t from,uintptr_t to)
{
    return 0x14000000u|((uint32_t)(((int64_t)to-(int64_t)from)/4)&0x3ffffffu);
}
int takeover_endpoints_install(uintptr_t base)
{
    uintptr_t entry=base+0x15f5690,site=entry+4;
    if (native_process) return image==base;
    /* Keep PACIASP at the indirect entry (also a BTI landing instruction).
     * Only its following STP is replaced; no PC-relative code is relocated. */
    if (F((void*)entry,0,uint32_t)!=0xd503233fu ||
        F((void*)entry,4,uint32_t)!=0xa9b67bfdu ||
        F((void*)entry,8,uint32_t)!=0x910003fdu) return 0;
    void *relay=MAP_FAILED;
    for(uintptr_t off=0x7900000;off<0x7f00000;off+=0x10000){
        relay=mmap((void*)(base+off),4096,PROT_READ|PROT_WRITE,
                   MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
        if(relay!=MAP_FAILED)break;
    }
    if(relay==MAP_FAILED)return 0;
    int64_t delta=(int64_t)(uintptr_t)relay-(int64_t)site;
    if(delta< -0x8000000||delta>=0x8000000){munmap(relay,4096);return 0;}
    /* Undo the entry's PAC before tail-entering the normal C wrapper. Its
     * call to the trampoline starts a fresh, balanced native PAC/frame. */
    uint32_t words[]={0xd50323bf,0x58000070,0xd61f0200,0xd503201f};
    memcpy(relay,words,sizeof(words));F(relay,16,uintptr_t)=(uintptr_t)process;
    const uint32_t original[]={0xd503245f,0xd503233f,0xa9b67bfd};
    memcpy((char*)relay+32,original,sizeof(original));
    F(relay,44,uint32_t)=branch((uintptr_t)relay+44,entry+8);
    __builtin___clear_cache(relay,(char*)relay+48);
    if(mprotect(relay,4096,PROT_READ|PROT_EXEC)){munmap(relay,4096);return 0;}
    void *page=(void*)(site&~(uintptr_t)4095);
    if(mprotect(page,4096,PROT_READ|PROT_WRITE)){munmap(relay,4096);return 0;}
    image=base;native_process=(void(*)(void*,float))((uintptr_t)relay+32);
    F((void*)site,0,uint32_t)=branch(site,(uintptr_t)relay);
    __builtin___clear_cache((char*)site,(char*)site+4);
    if(mprotect(page,4096,PROT_READ|PROT_EXEC))_exit(121);
    fprintf(stderr,"TAKEOVER SCALED_ENDPOINTS_INSTALLED native_Pickup_Jump_preserved\n");
    return 1;
}
