#define _GNU_SOURCE
#define PHONE_RGB_TEST 1
#define MPC_CHORDS 1
#include "src/voice_guard.h"
#include "src/phone_native.c"
#include <assert.h>
static int locked;static unsigned recall_calls,regular_calls;
void voice_guard_lock(void){assert(!locked);locked=1;}
void voice_guard_unlock(void){assert(locked);locked=0;}
int workflow_button(void*e,uint64_t ticket){(void)e;(void)ticket;return 0;}
static void receiver(void*e){
 unsigned code=FIELD(e,8,unsigned char);
 if(code==0x49&&atomic_load(&native_buttons_down[0x31])){assert(!locked);++recall_calls;}
 else{assert(locked);++regular_calls;}
}
static void send(unsigned code,unsigned down){unsigned char e[16]={[8]=code,[9]=down};forward_buttons(e);assert(!locked);}
int main(void){
 void*m=mmap(NULL,0x7500000,7,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(m!=MAP_FAILED);base=(uintptr_t)m;
 uintptr_t at=base+phone_hooks[H_BUTTONS].trampoline;uint32_t code[]={0x58000050,0xd61f0200};void*fn=receiver;memcpy((void*)at,code,8);memcpy((void*)(at+8),&fn,8);__builtin___clear_cache(m,(char*)m+0x7500000);
 ui_known=1;ui_thread=pthread_self();
 send(0x31,1);atomic_store(&native_shift_requested,0); /* Physical release already requested, not yet forwarded. */
 send(0x49,1);send(0x49,0);send(0x31,0);send(0x49,1);send(0x51,1);
 assert(recall_calls==2&&regular_calls==4);
 puts("PASS Recall uses ordered native Shift state and reaches native receiver without holding render lock; ordinary record and Stop keep voice guard");return 0;
}
