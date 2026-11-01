/* Exercise the production sender and timer using captured native LED output.
 * Hidden Sample represents FX Select (or any other non-Sample screen). */
#define _GNU_SOURCE
#define PHONE_RGB_TEST 1
#define MPC_SEQUENCE_COLOURS 1
#include "src/values_bipolar.h"
#include "src/phone_native.c"
#include <assert.h>
static int sample_visible=1,seq_visible,bank,levels_on,steps_on,events_visible,inside_rgb;
static unsigned transport,queries,calls;
static int steps_lighting;
static uint64_t steps_lights[16];
static uint32_t sent[16];
int note_native_sample_visible(void){assert(!inside_rgb);++queries;return sample_visible;}
int music_ui_sequence_visible(void){return seq_visible;}
unsigned music_ui_sequence_presence(unsigned b){(void)b;return 0xffff;}
int music_ui_sequence_selected(void){return -1;}
unsigned step_native_transport_state(void){return transport;}
int64_t step_native_clock_ticks(uint64_t*s,unsigned*r){if(s)*s=0;if(r)*r=0;return 0;}
int step_native_browser_bank(int v){assert(v==-1);return bank;}
int step_native_active(void){return steps_on;}
int step_native_sound_layout(void){return events_visible;}
int music_lighting_levels_state(unsigned*on,unsigned*mode,unsigned*origin){*on=(unsigned)levels_on;*mode=0;*origin=0;return 1;}
int workflow_light(unsigned p,uint64_t*out){(void)p;(void)out;return 0;}
uint64_t workflow_palette_generation(void){return 0;}
int step_native_custom(void){return steps_lighting;}
int step_native_button_light(unsigned code,unsigned*out){(void)code;(void)out;return 0;}
int step_native_light(unsigned p,uint64_t*out){if(!steps_lighting||p>=16)return 0;*out=steps_lights[p];return 1;}
static uint32_t convert(const uint64_t*c){return (uint32_t)*c;}
static void capture(void*s,uint32_t colour,unsigned pad){assert(s==(void*)0x1234&&pad<16);++calls;sent[pad]=colour;}
static void stub(uintptr_t off,void*fn){uint32_t w[]={0x58000050,0xd61f0200};memcpy((void*)(base+off),w,8);memcpy((void*)(base+off+8),&fn,8);}
static void native_rgb(unsigned pad,uint32_t colour){unsigned before=queries;inside_rgb=1;rgb((void*)0x1234,colour,pad);inside_rgb=0;assert(queries==before);}
static unsigned button_values[128];
static void indexed_capture(void*s,unsigned value,unsigned code){assert(s==(void*)0x1234&&code<128);button_values[code]=value;}
static unsigned white_count(void){unsigned n=0;for(unsigned i=0;i<16;++i)n+=sent[i]==0xffffff;return n;}
int main(void){
 void*p=mmap(NULL,0x7500000,PROT_READ|PROT_WRITE|PROT_EXEC,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(p!=MAP_FAILED);base=(uintptr_t)p;
 stub(phone_hooks[H_RGB].trampoline,capture);stub(0x23b00c0,convert);__builtin___clear_cache(p,(char*)p+0x7500000);
 music_reset();assert(music_set_pad_colour(0,5)); /* Red. */
 for(unsigned i=0;i<16;++i)native_rgb(i,i==0?0xffff00:0x303030);
 send_step_lights();assert(sent[0]==0xff0000&&sent[1]==0x303030);
 native_rgb(0,0x008000);assert(sent[0]==0x800000); /* Preserve native intensity. */
 /* Opening FX Select must restore native LEDs even without a native repaint. */
 sample_visible=0;unsigned before=calls;send_step_lights();assert(calls>before&&sent[0]==0x008000);
 native_rgb(0,0x104020);assert(sent[0]==0x104020);send_step_lights();assert(sent[0]==0x104020);
 /* Return to Sample, keeping the stored Pad Colour and current native level. */
 sample_visible=1;send_step_lights();assert(sent[0]==0x400000&&music_pad_colour(0)==5);
 /* Native Mute and Erase colours retain priority. */
 atomic_store(&native_buttons_down[0x23],1);send_step_lights();assert(sent[0]==0x104020);
 atomic_store(&native_buttons_down[0x23],0);send_step_lights();assert(sent[0]==0x400000);
 atomic_store(&native_buttons_down[0x09],1);send_step_lights();assert(sent[0]==0x104020);
 atomic_store(&native_buttons_down[0x09],0);send_step_lights();assert(sent[0]==0x400000);
 /* Ordinary Seq uses its own palette; recording retains the Sample layout. */
 sample_visible=0;seq_visible=1;transport=0;assert(music_set_sequence_colour(0,12));send_step_lights();assert(sent[0]==0x0096c0);
 transport=4;send_step_lights();assert(sent[0]==0x400000);
 seq_visible=0;sample_visible=0;send_step_lights();assert(sent[0]==0x104020);
 events_visible=steps_on=1;send_step_lights();assert(sent[0]==0x400000);
 events_visible=steps_on=0;send_step_lights();assert(sent[0]==0x104020);
 /* Bank and Stock stay native; custom Sample colour remains bank-specific. */
 sample_visible=1;bank=1;send_step_lights();assert(sent[0]==0x104020);
 bank=0;send_step_lights();assert(sent[0]==0x400000);
 levels_on=1;send_step_lights();assert(sent[0]==0x104020);levels_on=0;
 steps_on=1;send_step_lights();assert(sent[0]==0x104020);steps_on=0;
 send_step_lights();assert(sent[0]==0x400000);
 /* Button CC IDs overlap pad numbers but never drive the pad RGB array. */
 stub(phone_hooks[H_BUTTON_LIGHT].trampoline,indexed_capture);__builtin___clear_cache((char*)base,(char*)base+0x7500000);
 sample_visible=0;steps_on=steps_lighting=1;steps_lights[12]=0xffffff;
 send_step_lights();button_light((void*)0x1234,1,0);assert(white_count()==1&&button_values[0]==1);
 for(unsigned step=0;step<16;++step){
  unsigned physical=(3-step/4)*4+step%4;
  memset(steps_lights,0,sizeof(steps_lights));steps_lights[physical]=0xffffff;send_step_lights();
  /* Physical button LEDs must not become artificial pad/cursor updates. */
  for(unsigned i=0;i<16;++i)button_light((void*)0x1234,1,i);
  assert(white_count()==1&&sent[physical]==0xffffff);
 }
 button_light((void*)0x1234,1,0x50);assert(button_values[0x50]==1);
 puts("PASS MCU CC button IDs0..15 preserve physical button LEDs and never recolour pad RGB or create duplicate Steps cursors");
 puts("PASS Pad Colour RGB: Sample/FX Select round-trip, native restore without repaint, direct sender, bank, Stock, Mute/Erase, Levels/Steps and Seq recording");
 munmap(p,0x7500000);return 0;
}
