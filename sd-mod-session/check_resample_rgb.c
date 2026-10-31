#define MPC_RESAMPLE_TRIAL 1
#define MPC_STOCK_RESAMPLE 1
#define main workflow_rgb_proof
#include "check_workflow_rgb.c"
#undef main
static int resample_colours;static unsigned selected;static uint64_t resample_epoch=1;
int resample_ui_light(unsigned pad,uint64_t*out){if(!resample_colours||pad>=16)return 0;*out=pad==selected?0x39c1d6:0x9a45ff;return 1;}
uint64_t resample_ui_palette_generation(void){return resample_epoch;}
int main(void){
 assert(!workflow_rgb_proof());void*p=mmap(NULL,0x7500000,7,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(p!=MAP_FAILED);base=(uintptr_t)p;
 stub(phone_hooks[H_RGB].trampoline,capture);stub(0x23b00c0,convert);__builtin___clear_cache((char*)p,(char*)p+0x7500000);
 for(unsigned i=0;i<16;++i)rgb((void*)0x1234,0x3000+i,i);
 atomic_store(&led_enabled,1);resample_colours=1;selected=3;++resample_epoch;unsigned n=calls;send_step_lights();assert(calls==n+16&&sent[3]==0x39c1d6&&sent[4]==0x9a45ff);
 selected=9;++resample_epoch;n=calls;send_step_lights();assert(calls==n+16&&sent[9]==0x39c1d6&&sent[3]==0x9a45ff);
 rgb((void*)0x1234,0x4009,9);assert(sent[9]==0x39c1d6);
 resample_colours=0;atomic_store(&led_enabled,0);++resample_epoch;n=calls;send_step_lights();assert(calls==n+16&&sent[9]==0x4009&&sent[3]==0x3003);
 puts("PASS production Resample RGB: selected target overrides stale naming/native writes, palette movement refreshes hardware, close restores stock lights");munmap(p,0x7500000);return 0;
}
