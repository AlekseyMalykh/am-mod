#define _GNU_SOURCE
/* Centre-based K bar for symmetric ranges (Pan, Semi/Fine Tune).
 * The session's K-panel painter (values.S, at 0x6d3b800) fills continuous
 * bars from the left. For a range symmetric around zero this moves the fill
 * to start at a centre mark, so the default (0, "C") reads as centred.
 * Patched at runtime: the zero-width skip (cbz) becomes a NOP and the fill
 * call goes through a relay that passes the slider and the fill colour. */
#include "values_bipolar.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#define F(p,o,T) (*(T*)((char*)(p)+(o)))
static uintptr_t image;
struct range{double lo,hi;};
/* The painter keeps the slider's name as a juce::String at sp+0x80 of its
 * frame (drawn just before); the relay passes its address. A JUCE String
 * is a pointer to its UTF-8 text. */
static int named(const char*const*name,const char*want){
    const char*t=name?*name:NULL;
    return t&&!strcmp(t,want);
}
static void values_fill(void*g,int x,int y,int w,int h,void*slider,uint32_t colour,const char*const*name){
    void(*rect)(void*,int,int,int,int)=(void(*)(void*,int,int,int,int))(image+0x102e000);
    void(*ink)(void*,uint32_t)=(void(*)(void*,uint32_t))(image+0x102d5e0);
    struct range r=slider?((struct range(*)(void*))(image+0x1123460))(slider):(struct range){0,0};
    double span=r.hi-r.lo;
    /* Pan is stored 0..1 (C at the middle), Tune-like values around 0. */
    if(named(name,"Pan")||(span>0&&r.lo<0&&fabs(r.lo+r.hi)<=span*1e-3)){
        int centre=x+49,value=x+w;
        if(value>centre)rect(g,centre,y,value-centre,h);
        else if(value<centre)rect(g,value,y,centre-value,h);
        ink(g,0xffa0a0a0u);rect(g,centre-1,y-2,2,h+4);ink(g,colour);
    }else if(w>0)rect(g,x,y,w,h);
}
static int patch_word(uintptr_t at,uint32_t value){
    void *page=(void*)(at&~(uintptr_t)4095);
    if(mprotect(page,4096,PROT_READ|PROT_WRITE))return 0;
    F((void*)at,0,uint32_t)=value;__builtin___clear_cache((char*)at,(char*)at+4);
    if(mprotect(page,4096,PROT_READ|PROT_EXEC))_exit(121);
    return 1;
}
int values_bipolar_install(uintptr_t base){
    if(image)return image==base;
    if(!base||sysconf(_SC_PAGESIZE)!=4096)return 0;
    const uintptr_t skip=base+0x6d3bd28,call=base+0x6d3bd3c;
    if(F((void*)skip,0,uint32_t)!=0x34000583u||F((void*)call,0,uint32_t)!=(0x94000000u|((uint32_t)((0x102e000-0x6d3bd3c)/4)&0x3ffffffu)))return 0;
    void *relay=MAP_FAILED;
    for(uintptr_t off=0x7800000;off<0x7f00000;off+=0x10000){
        relay=mmap((void*)(base+off),4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
        if(relay!=MAP_FAILED)break;
    }
    if(relay==MAP_FAILED)return 0;
    const uint32_t code[]={0xaa1403e5u /* mov x5,x20 */,0x2a1903e6u /* mov w6,w25 */,
        0x910203e7u /* add x7,sp,#0x80 */,0x58000070u /* ldr x16,#12 */,0xd61f0200u /* br x16 */,0xd503201fu};
    memcpy(relay,code,sizeof(code));F(relay,24,uintptr_t)=(uintptr_t)values_fill;
    __builtin___clear_cache(relay,(char*)relay+32);
    if(mprotect(relay,4096,PROT_READ|PROT_EXEC)){munmap(relay,4096);return 0;}
    int64_t d=(int64_t)(uintptr_t)relay-(int64_t)call;
    if(d< -0x8000000||d>=0x8000000){munmap(relay,4096);return 0;}
    image=base;
    if(!patch_word(skip,0xd503201fu))return 0;
    if(!patch_word(call,0x94000000u|((uint32_t)(d/4)&0x3ffffffu)))_exit(121);
    fprintf(stderr,"VALUES_BIPOLAR CENTRE_BAR_INSTALLED\n");
    return 1;
}
