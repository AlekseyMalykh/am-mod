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

/* Shift badges ("S") come from the native-UI pair table in values-code
 * (ui_shift_pairs: screen, visible anchor, Shift alternative, K column; 19
 * rows). Two Sample rows are added at runtime: Mix K1 Volume (the mod's Pad
 * Colour under Shift) and Amp Env K2 Decay (native Shift alternative at
 * +0x5a18). The rows go into the zero padding right after the table and the
 * two row counts become 21; nothing is written unless every check matches. */
int values_shift_pairs_install(uintptr_t base){
    const uintptr_t table=base+0x6d3b800+0xb68,extra=table+19*24;
    const uintptr_t sites[2]={base+0x6d3b800+0x790,base+0x6d3b800+0x840};
    if((int64_t)table+F((void*)table,0,int64_t)!=(int64_t)(base+0x6e212a0)||F((void*)table,8,uint32_t)!=0x2bb8||F((void*)table,12,uint32_t)!=0x2f08)return 0;
    for(unsigned i=0;i<2;++i){
        uint32_t adr=F((void*)sites[i],0,uint32_t);
        int64_t imm=(int64_t)((((adr>>5)&0x7ffff)<<2)|((adr>>29)&3));if(imm&(1<<20))imm-=1<<21;
        if((adr&0x9f00001fu)!=0x1000000au||sites[i]+imm!=table||F((void*)sites[i],4,uint32_t)!=0x5280026bu)return 0;
    }
    for(unsigned i=0;i<48;++i)if(F((void*)extra,i,unsigned char))return 0;
    void *page=(void*)(extra&~(uintptr_t)4095);
    if((extra+48)>((uintptr_t)page+4096))return 0;
    if(mprotect(page,4096,PROT_READ|PROT_WRITE))return 0;
    const uint32_t rows[2][4]={{0x4638,0x4638,0,0},{0x5378,0x5a18,1,0}};
    for(unsigned i=0;i<2;++i){uintptr_t e=extra+i*24;F((void*)e,0,int64_t)=(int64_t)(base+0x6e212a0)-(int64_t)e;memcpy((void*)(e+8),rows[i],16);}
    __builtin___clear_cache((char*)extra,(char*)extra+48);
    if(mprotect(page,4096,PROT_READ|PROT_EXEC))_exit(121);
    for(unsigned i=0;i<2;++i)if(!patch_word(sites[i]+4,0x528002abu /* mov w11,#21 */))_exit(121);
    fprintf(stderr,"VALUES_SHIFT_PAIRS ADDED mix_k1_pad_colour amp_env_decay\n");
    return 1;
}

/* The K3 lock image sits at the right edge only for Sample Trim "Loop"
 * (+0x38f8, a compare in values-code). Under Shift the native shows its
 * alternative (+0x3c48, "Zoom: Loop") whose lock then sat next to the value.
 * Route that compare through a small trampoline accepting both. */
static uint32_t b_to(uintptr_t from,uintptr_t to){return 0x14000000u|((uint32_t)(((int64_t)to-(int64_t)from)/4)&0x3ffffffu);}
static uint32_t bne_to(uintptr_t from,uintptr_t to){return 0x54000001u|((((uint32_t)(((int64_t)to-(int64_t)from)/4))&0x7ffffu)<<5);}
int values_loop_lock_install(uintptr_t base){
    const uintptr_t start=base+0x6d3b800,end=start+0xd30;uintptr_t site=0;
    for(uintptr_t a=start;a+20<=end;a+=4)
        if(F((void*)a,0,uint32_t)==0xd2871f0au&&F((void*)a,4,uint32_t)==0xeb0a013fu&&(F((void*)a,8,uint32_t)&0xff00001fu)==0x54000001u&&
           F((void*)a,12,uint32_t)==0x52800029u&&F((void*)a,16,uint32_t)==0xb9008fe9u){if(site)return 0;site=a;}
    if(!site)return 0;
    int64_t imm=(int64_t)((F((void*)site,8,uint32_t)>>5)&0x7ffff);if(imm&(1<<18))imm-=1<<19;
    const uintptr_t ready=site+8+imm*4,set=site+12;
    void *page=MAP_FAILED;
    for(uintptr_t off=0x7a00000;off<0x7f00000;off+=0x10000){
        page=mmap((void*)(base+off),4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
        if(page!=MAP_FAILED)break;
    }
    if(page==MAP_FAILED)return 0;
    uintptr_t t=(uintptr_t)page;
    int64_t far=(int64_t)t-(int64_t)site;if(far< -0x8000000||far>=0x8000000){munmap(page,4096);return 0;}
    uint32_t code[9]={0xd2871f0au,0xeb0a013fu,0,0,0xd287890au /* mov x10,#0x3c48 */,0xeb0a013fu,0,0,0};
    code[2]=bne_to(t+8,t+16);code[3]=b_to(t+12,set);
    code[6]=bne_to(t+24,t+32);code[7]=b_to(t+28,set);code[8]=b_to(t+32,ready);
    memcpy(page,code,sizeof(code));__builtin___clear_cache((char*)page,(char*)page+sizeof(code));
    if(mprotect(page,4096,PROT_READ|PROT_EXEC)){munmap(page,4096);return 0;}
    if(!patch_word(site,b_to(site,t)))_exit(121);
    fprintf(stderr,"VALUES_LOOP_LOCK SHIFT_ZOOM_RIGHT_EDGE\n");
    return 1;
}
