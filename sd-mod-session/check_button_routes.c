/* Production native button forwarding, workflow boundary and entry chord.
 * Native MIDI receiver is a recording fixture; full BoolProperty test is in
 * check_workflow.c with actual159f2f0/13904f0. */
#define PHONE_RGB_TEST 1
#include "src/phone_native.c"
#include <assert.h>
static unsigned calls,requested;static unsigned state_by_code[112];
static int allow_preview;static unsigned preview_workers;
int workflow_preview_worker(void*p){(void)p;return allow_preview;}
void workflow_preview_played(void*p){(void)p;}
static void preview_receiver(void*p){++preview_workers;void*r=FIELD(p,0,void*);__atomic_fetch_sub((unsigned*)((char*)r+8),1,__ATOMIC_RELEASE);}
int workflow_button(void*e,uint64_t t){(void)e;(void)t;return 1;}
void note_native_button_requested(unsigned c,unsigned d){assert((c==0x31||c==0x09)&&!d);++requested;}
void step_native_requested(unsigned c,unsigned d){assert((c==0x31||c==0x09)&&!d);++requested;}
static void receive(void*e){unsigned c=FIELD(e,8,unsigned char),d=FIELD(e,9,unsigned char)!=0;assert(c<112);state_by_code[c]=d;++calls;}
static void stub(uintptr_t off,void*fn){uint32_t w[]={0x58000050,0xd61f0200};memcpy((void*)(base+off),w,8);memcpy((void*)(base+off+8),&fn,8);}
int main(void){void*p=mmap(NULL,0x7500000,7,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(p!=MAP_FAILED);base=(uintptr_t)p;stub(phone_hooks[H_BUTTONS].trampoline,receive);__builtin___clear_cache((char*)p,(char*)p+0x7500000);
 stub(phone_hooks[H_CONTROLLER].trampoline,receive);__builtin___clear_cache((char*)p,(char*)p+0x7500000);
 stub(phone_hooks[H_PRIVATE_PREVIEW_WORKER].trampoline,preview_receiver);__builtin___clear_cache((char*)p,(char*)p+0x7500000);
 unsigned char ref[16]={0},payload[80]={0};FIELD(payload,0,void*)=ref;FIELD(ref,8,unsigned)=1;private_preview_worker(payload);assert(!preview_workers&&!FIELD(ref,8,unsigned));
 allow_preview=1;FIELD(ref,8,unsigned)=1;private_preview_worker(payload);assert(preview_workers==1&&!FIELD(ref,8,unsigned));
 unsigned char volume[16]={0};volume[8]=0x13;volume[9]=127;assert(forward_main_volume(volume)&&calls==1);volume[9]=0;assert(forward_main_volume(volume)&&calls==2);volume[8]=0x12;assert(!forward_main_volume(volume)&&calls==2);calls=0;
 unsigned char event[16]={0};event[8]=0x09;event[9]=1;forward_buttons(event);assert(state_by_code[9]);
 event[9]=0;workflow_buttons(event,1);assert(!state_by_code[9]&&calls==2&&!atomic_load(&native_buttons_down[9]));workflow_buttons(event,1);assert(calls==2);
 /* A button begun IN Batch must not escape into a native action on release. */
 event[8]=0x0e;event[9]=1;workflow_buttons(event,2);event[9]=0;workflow_buttons(event,3);assert(calls==2);
 /* B2 releases the native entry chord immediately, using its original
  * parser functors; delayed physical releases are swallowed exactly once. */
 event[8]=0x31;event[9]=1;forward_buttons(event);event[8]=0x09;forward_buttons(event);assert(state_by_code[0x31]&&state_by_code[9]);
 unsigned before=calls;phone_release_copy_gesture();assert(calls==before+2&&!state_by_code[0x31]&&!state_by_code[9]&&requested==4);
 event[9]=0;forward_buttons(event);event[8]=0x31;forward_buttons(event);assert(calls==before+2);
 /* A fresh Shift press after the handoff works normally. */
 event[9]=1;forward_buttons(event);event[9]=0;forward_buttons(event);assert(calls==before+4&&!state_by_code[0x31]);
 puts("PASS production button routing: native-down/native-up pairing across workflow generations, no orphan native B3 release, B2 immediate Shift/Erase release and suppression of delayed physical releases");munmap(p,0x7500000);return 0;}
