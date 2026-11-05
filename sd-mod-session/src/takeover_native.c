#define _GNU_SOURCE
#include "takeover_native.h"
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#define F(p,o,T) (*(T*)((char*)(p)+(o)))
static uintptr_t image;
static void (*native_process)(void*,float);
static _Atomic unsigned pending_handoff;
static _Thread_local unsigned input_knob;
void takeover_native_context_changed(void){atomic_store(&pending_handoff,7u);}
void takeover_native_forward(void *event,void(*receive)(void*)){
 unsigned previous=input_knob,cc=F(event,8,unsigned char);
 input_knob=cc>=0x10&&cc<=0x12?1u<<(cc-0x10):0;
 receive(event);input_knob=previous;
}
static void process(void *binding,float physical){
 /* Custom EQ does not feed the native binding's hardware history. On return,
  * reset only the first active Scaled connection for each physical knob using
  * the native constructor's unset-history values. Never set the parameter. */
 if(binding&&input_knob&&(atomic_load(&pending_handoff)&input_knob)&&
    F(binding,0xdc,int)==1&&(F(binding,0x9b,unsigned char)&1)){
  void*slider=F(binding,0x60,void*);
  if(slider&&((int(*)(void*))(image+0x10f98d0))(slider)){
   unsigned old=atomic_fetch_and(&pending_handoff,~input_knob);
   if(old&input_knob){F(binding,0x144,float)=-1.f;F(binding,0x148,float)=-1.f;}
  }
 }
 native_process(binding,physical);
}
static uint32_t branch(uintptr_t from,uintptr_t to)
{
    return 0x14000000u|((uint32_t)(((int64_t)to-(int64_t)from)/4)&0x3ffffffu);
}
static int install_handoff(uintptr_t base)
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
    fprintf(stderr,"TAKEOVER CONTEXT_HANDOFF_INSTALLED native_math_preserved\n");
    return 1;
}

int takeover_native_check(uintptr_t base)
{
    if(native_process)return image==base;
    if(!base)return 0;
    const uintptr_t offsets[]={0x15f5690,0x15e70c0};
    const uint32_t frames[]={0xa9b67bfd,0xa9b77bfd};
    for(unsigned i=0;i<2;++i){
        uint32_t words[3];memcpy(words,(const void*)(base+offsets[i]),sizeof(words));
        if(words[0]!=0xd503233f||words[1]!=frames[i]||words[2]!=0x910003fd)return 0;
    }
    fprintf(stderr,"TAKEOVER NATIVE_PROCESSORS_PRESERVED RotaryFixedConnection TrimParameter\n");
    return install_handoff(base);
}
