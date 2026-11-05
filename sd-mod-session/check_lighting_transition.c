#define _GNU_SOURCE
#include "src/music_lighting.c"
#include <assert.h>
#include <string.h>
#include <sys/mman.h>
static uint64_t generation=1;
uint64_t music_generation(void){return generation;}
static unsigned queued;static int accept=1;
uint64_t music_published(void){return 0;}
int music_pad_lighting_mode(void){return 1;}
uint64_t music_mod_settings_snapshot(void){return 0;}
static void noop(void*p){(void)p;}
static int enqueue(void*q,const void*a,uintptr_t token){(void)q;(void)a;(void)token;++queued;return accept;}
static void stub(uintptr_t off,void*fn){uint32_t w[]={0x58000050,0xd61f0200};memcpy((void*)(base+off),w,8);memcpy((void*)(base+off+8),&fn,8);}
int main(void){
 void*b=mmap(NULL,0x7500000,7,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(b!=MAP_FAILED);base=(uintptr_t)b;
 stub(0xf40e50,noop);stub(0xf40e90,noop);stub(0x2b59300,enqueue);__builtin___clear_cache(b,(char*)b+0x7500000);
 unsigned char layer[0x100]={0},props[0x300]={0},queue[0x250]={0};
 FIELD(layer,0,uintptr_t)=base+0x6e10a60;FIELD(layer,0xe8,void*)=props;FIELD(layer,0x40,void*)=queue;
 music_lighting_created(layer);props[0x2f8]=1;FIELD(props,0x88,int)=1;music_lighting_tick();assert(queued==1);
 music_lighting_tick();assert(queued==1);
 FIELD(props,0x88,int)=8;music_lighting_tick();assert(queued==2);
 ++generation;music_lighting_tick();assert(queued==3); /* Extract/colour metadata updates inside Chop. */
 props[0x2f8]=0;music_lighting_tick();assert(queued==4);
 music_lighting_tick();assert(queued==4);
 props[0x2f8]=1;accept=0;music_lighting_tick();assert(queued==5);accept=1;music_lighting_tick();assert(queued==6);
 music_lighting_destroyed(layer);music_lighting_tick();assert(queued==6);
 puts("PASS native lighting refresh queued on Tune/Chop/off transitions; steady layout quiet, full-queue retry, destroyed layer excluded");return 0;
}
