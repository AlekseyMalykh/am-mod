#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <sys/mman.h>
#include "src/knob_reset.c"
#include "src/phone_events.h"
static unsigned writes;static double last_value;static void*last_slider;
static unsigned char screen[0xa000],impl[0x300];static void*shown;
static int visible(void*p){return p==screen||p==shown;}
static int enabled(void*p){return p==shown;}
static int blocked(void*p){(void)p;return 0;}
static void setter(void*p,double value,int notification){assert(p==shown&&notification==1);++writes;last_slider=p;last_value=value;}
static void stub(uintptr_t off,void*fn){uint32_t words[]={0x58000050,0xd61f0200};memcpy((void*)(image+off),words,8);memcpy((void*)(image+off+8),&fn,8);}
static void slider(unsigned off,unsigned column,double lo,double hi){
 shown=screen+off;F(shown,0,uintptr_t)=image+0x706e5d0;F(shown,0x18,void*)=screen;
 F(shown,0x20,int)=column==2?213:(int)column*106;F(shown,0x24,int)=198;F(shown,0x28,int)=106;F(shown,0x2c,int)=38;
 F(shown,0x160,void*)=impl;F(impl,0x98,double)=lo;F(impl,0xa0,double)=hi;knob_reset_observe(shown);
}
int main(void){
 image=(uintptr_t)mmap(NULL,0x7500000,7,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(image!=(uintptr_t)MAP_FAILED);knob_reset_init(image);
 stub(0x112fd50,visible);stub(0x10f98d0,enabled);stub(0x112f6b0,blocked);stub(0x11c1d60,setter);__builtin___clear_cache((char*)image,(char*)image+0x7500000);
 F(screen,0,uintptr_t)=image+0x6e212a0;
 assert(!knob_reset_claim(0x10));knob_reset_button(0x23,1);uint64_t normal=knob_reset_ticket();assert(normal&2);
 assert(knob_reset_claim(0x10)&&knob_reset_claim(0x12)&&!knob_reset_claim(0x13));
 assert(knob_reset_begin(normal,0x10)&&!knob_reset_begin(normal,0x10));assert(knob_reset_begin(normal,0x11));
 knob_reset_button(0x31,1);uint64_t shift=knob_reset_ticket();assert(shift==(normal|1));assert(knob_reset_begin(shift,0x10)&&!knob_reset_begin(shift,0x10));
 knob_reset_button(0x31,0);assert(knob_reset_ticket()==normal&&!knob_reset_begin(normal,0x10));
 knob_reset_button(0x23,0);assert(!knob_reset_claim(0x10));knob_reset_button(0x23,1);assert(knob_reset_begin(knob_reset_ticket(),0x10));
 /* All bank pads/buttons remain outside this module: only three knob CCs
  * can be claimed, and the modifier function has no native-output callback. */
 for(unsigned i=0;i<128;++i)assert(knob_reset_claim(i)==(i>=0x10&&i<=0x12));
 slider(0x3258,1,0,100);assert(knob_reset_native(1,0)&&writes==1&&last_value==100&&last_slider==shown);
 slider(0x35a8,1,0,100);assert(knob_reset_native(1,1)&&last_value==0); /* Shift zoom differs from End. */
 slider(0x4988,2,0,1);assert(knob_reset_native(2,0)&&last_value==.5);
 slider(0x60b8,0,-24,24);assert(knob_reset_native(0,0)&&last_value==0);
 slider(0x7b38,0,0,127);assert(knob_reset_native(0,0)&&last_value==127);
 slider(0x4638,0,0,1);assert(knob_reset_native(0,0)&&last_value>.7&&last_value<.71);
 unsigned before=writes;F(impl,0xa0,double)=.5;assert(!knob_reset_native(0,0)&&writes==before);
 knob_reset_forget(screen);assert(!knob_reset_native(0,0));
 for(unsigned n=0;n<5000;++n){struct phone_event e={0};e.kind=PHONE_KNOB_RESET;e.workflow_ticket=6;e.native[8]=0x10+n%3;e.native[9]=n%128;assert(phone_events_push(&e));}
 assert(phone_events_pending()==3);struct phone_event e;for(unsigned i=0;i<3;++i)assert(phone_events_pop(&e));assert(!phone_events_pop(&e));
 e=(struct phone_event){.kind=PHONE_KNOB_RESET,.workflow_ticket=6};e.native[8]=0x10;assert(phone_events_push(&e));
 e.kind=PHONE_BUTTON;assert(phone_events_push(&e));e.kind=PHONE_KNOB_RESET;assert(phone_events_push(&e));assert(phone_events_pending()==3);
 while(phone_events_pop(&e)){}
 puts("PASS hold-bank modifier never owns button/pads/Main Volume; per-knob once per gesture and separate Shift snapshots; native End=100/Pan=.5/Cutoff=127/Volume defaults, current visible alternate, synchronous setter, range and lifetime guards");
 puts("PASS 5000 held-knob movements coalesce to three reset commands; a real action remains an ordering barrier");
 munmap((void*)image,0x7500000);return 0;
}
