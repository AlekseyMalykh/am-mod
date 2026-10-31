/* Production UI-owner sampler and native lighting registry; GUI refresh
 * receivers are fixtures. No fake pointer is retained after destruction. */
#define PHONE_RGB_TEST 1
#include "src/phone_native.c"
#include <assert.h>
static unsigned calls,enabled,mode,origin;
void step_native_levels_state(unsigned e,unsigned m,unsigned o){++calls;enabled=e;mode=m;origin=o;}
static int native_refresh(void*p,unsigned pad){(void)p;(void)pad;return 0;}
static void stub_refresh(uintptr_t address){
 uint32_t code[]={0x58000050,0xd61f0200};void*fn=native_refresh;
 memcpy((void*)address,code,8);memcpy((void*)(address+8),&fn,8);
}
int main(void){
 void*memory=mmap(NULL,0x7500000,7,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(memory!=MAP_FAILED);base=(uintptr_t)memory;
 stub_refresh(base+phone_hooks[H_SIXTEEN_REFRESH].trampoline);stub_refresh(base+phone_hooks[H_CHOP_REFRESH].trampoline);
 __builtin___clear_cache(memory,(char*)memory+0x7500000);
 music_lighting_init(base);music_reset();
 _Alignas(8) unsigned char layer[0x100]={0},newer[0x100]={0},properties[0x300]={0},second[0x300]={0};
 FIELD(layer,0,uintptr_t)=FIELD(newer,0,uintptr_t)=base+0x6e10a60;
 FIELD(layer,0xe8,void*)=properties;FIELD(layer,0xf8,void*)=properties;FIELD(newer,0xe8,void*)=second;
 levels_state_tick();assert(!enabled&&!mode&&!origin);
 music_lighting_created(layer);FIELD(properties,0x2f8,unsigned char)=1;FIELD(properties,0x88,int)=1;FIELD(properties,0x228,int)=3;
 levels_state_tick();assert(enabled&&mode==1&&origin==3);
 /* No refresh callback on exit: the actual native properties are enough. */
 FIELD(properties,0x2f8,unsigned char)=0;FIELD(properties,0x228,int)=-1;
 levels_state_tick();assert(!enabled&&!origin);
 unsigned before=calls;sixteen_refresh(layer,7);chop_refresh(layer,7);assert(calls==before);
 FIELD(properties,0x2f8,unsigned char)=1;FIELD(properties,0x228,int)=3;
 for(unsigned m=0;m<10;++m){FIELD(properties,0x88,int)=(int)m;levels_state_tick();assert(enabled&&mode==m&&origin==3);}
 FIELD(properties,0x2f8,unsigned char)=0;levels_state_tick();assert(!enabled);
 FIELD(properties,0x2f8,unsigned char)=1;FIELD(properties,0x88,int)=10;levels_state_tick();assert(!enabled);
 FIELD(properties,0x88,int)=1;FIELD(properties,0x228,int)=-1;levels_state_tick();assert(!enabled);
 FIELD(properties,0x228,int)=3;levels_state_tick();assert(enabled);
 /* A newer off source must override an older on source. */
 music_lighting_created(newer);levels_state_tick();assert(!enabled);
 music_lighting_destroyed(newer);memset(newer,0xa5,sizeof(newer));memset(second,0xa5,sizeof(second));
 levels_state_tick();assert(enabled&&mode==1);
 FIELD(layer,0,uintptr_t)=0;levels_state_tick();assert(!enabled);
 FIELD(layer,0,uintptr_t)=base+0x6e10a60;levels_state_tick();assert(enabled);
 music_lighting_destroyed(layer);memset(layer,0xa5,sizeof(layer));memset(properties,0xa5,sizeof(properties));
 levels_state_tick();assert(!enabled&&!mode&&!origin);
 munmap(memory,0x7500000);
 puts("PASS production Levels state sampler: off without refresh, off reference=-1, all ten modes, invalid state fail closed, refresh hooks cannot overwrite UI state, newest live source, removal before poisoned lifetime");
 return 0;
}
