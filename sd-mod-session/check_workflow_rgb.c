/* Test the production RGB interception and refresh, including stale naming
 * state. No device receiver: record the exact original-sender calls. */
#define PHONE_RGB_TEST 1
#include "src/phone_native.c"
#include <assert.h>
#include "check_palette_receiver.h"
void*note_native_context(void){return NULL;}
static int custom_palette;
static uint64_t palette_epoch=1, test_colours[16];
static unsigned calls, counts[16];static uint32_t sent[16];
int music_pad_lighting_mode(void){return 1;}
int workflow_light(unsigned pad,uint64_t*out){if(!custom_palette||pad>=16)return 0;*out=test_colours[pad];return 1;}
void step_native_observe_rgb(unsigned pad,unsigned rgb){(void)pad;(void)rgb;}
int workflow_native_occupied(void){return -1;}
int workflow_native_selected(void){return -1;}
int workflow_native_pad_colours(void){return 0;}
uint64_t workflow_palette_generation(void){return palette_epoch;}
int step_native_light(unsigned pad,uint64_t*out){(void)pad;(void)out;return 0;}
static uint32_t convert(const uint64_t*c){return (uint32_t)*c;}
static void capture(void*s,uint32_t colour,unsigned pad){assert(s==(void*)0x1234&&pad<16);++calls;++counts[pad];sent[pad]=colour;}
static void stub(uintptr_t off,void*fn){uint32_t w[]={0x58000050,0xd61f0200};memcpy((void*)(base+off),w,8);memcpy((void*)(base+off+8),&fn,8);}
int main(void){
 void*p=mmap(NULL,0x7500000,PROT_READ|PROT_WRITE|PROT_EXEC,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(p!=MAP_FAILED);base=(uintptr_t)p;
 stub(phone_hooks[H_RGB].trampoline,capture);stub(0x23b00c0,convert);__builtin___clear_cache((char*)p,(char*)p+0x7500000);
 for(unsigned i=0;i<16;++i){test_colours[i]=i<4?0x32c780:i==4?0xffc247:i==5?0xff4040:i==6?0x39c1d6:0x9a45ff;rgb((void*)0x1234,0x1000+i,i);}
 assert(calls==16&&sent[0]==0x1000);
 custom_palette=1;atomic_store(&led_enabled,1); /* stale native naming must lose */
 for(unsigned i=0;i<16;++i){rgb((void*)0x1234,0x2000+i,i);assert(sent[i]==test_colours[i]);}
 unsigned n=calls;send_step_lights();assert(calls==n+16);n=calls;send_step_lights();assert(calls==n);
 /* Native bank switch can clear LEDs while the custom bank palette has the
  * same colours. New workflow generation must reassert all sixteen. */
 ++palette_epoch;send_step_lights();assert(calls==n+16);
 n=calls;test_colours[4]=0x9a45ff;test_colours[7]=0xffc247;send_step_lights();assert(calls==n+2&&sent[4]==0x9a45ff&&sent[7]==0xffc247);
 /* Closing custom UI restores stock colours even when generation changes. */
 atomic_store(&led_enabled,0);custom_palette=0;++palette_epoch;n=calls;send_step_lights();assert(calls==n+16);
 for(unsigned i=0;i<16;++i)assert(sent[i]==0x2000+i);
 puts("PASS production RGB sender: workflow precedes naming, bank refresh reasserts unchanged colours, target delta, stock restore");
 munmap(p,0x7500000);return 0;
}
