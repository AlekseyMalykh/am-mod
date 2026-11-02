/* Exercise the production sender and timer using captured native LED output.
 * Hidden Sample represents FX Select (or any other non-Sample screen). */
#define _GNU_SOURCE
#define PHONE_RGB_TEST 1
#define MPC_SEQUENCE_COLOURS 1
#define MPC_COMPRESSOR 1
#include "src/values_bipolar.h"
#include "src/phone_native.c"
#include <assert.h>
static int sample_visible=1,seq_visible,bank,levels_on,steps_on,events_visible,inside_rgb,compressor_visible;
static unsigned transport,queries,calls,levels_type,levels_origin;static int native_workflow;
static int seq_selected=-1;static unsigned seq_present=0xffff;
static int64_t seq_tick;static uint64_t seq_serial;
static int steps_lighting;
static uint64_t steps_lights[16];
static uint32_t sent[16];
int note_native_sample_visible(void){assert(!inside_rgb);++queries;return sample_visible;}
int compressor_ui_active(void){return compressor_visible;}
int music_ui_sequence_visible(void){return seq_visible;}
unsigned music_ui_sequence_presence(unsigned b){(void)b;return seq_present;}
int music_ui_sequence_selected(void){return seq_selected;}
unsigned step_native_transport_state(void){return transport;}
int64_t step_native_clock_ticks(uint64_t*s,unsigned*r){if(s)*s=seq_serial;if(r)*r=(transport&3)==3;return seq_tick;}
int step_native_browser_bank(int v){assert(v==-1);return bank;}
int step_native_active(void){return steps_on;}
int step_native_sound_layout(void){return events_visible;}
int music_lighting_levels_state(unsigned*on,unsigned*mode,unsigned*origin){*on=(unsigned)levels_on;*mode=levels_type;*origin=levels_origin;return 1;}
int workflow_light(unsigned p,uint64_t*out){(void)p;(void)out;return 0;}
void step_native_observe_rgb(unsigned pad,unsigned rgb){(void)pad;(void)rgb;}
int workflow_native_occupied(void){return 3;}
int workflow_native_pad_colours(void){return native_workflow;}
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
static double test_bpm=120;static unsigned test_numerator=4,test_denominator=4;
static int test_position(void*p,void*out){assert(!inside_rgb&&p==(void*)(base+0x4000));memset(out,0,88);FIELD(out,0,double)=test_bpm;FIELD(out,8,unsigned)=test_numerator;FIELD(out,12,unsigned)=test_denominator;return 1;}
static unsigned scale(unsigned c,unsigned level){return (((c>>16)&255)*level/255<<16)|(((c>>8)&255)*level/255<<8)|((c&255)*level/255);}
static void __attribute__((unused)) pulse_checks(void){
 struct sequence_timing t={120,960,3840};
 for(unsigned b=0;b<5;++b){
  t.bpm=(double[]){20,60,120,137,300}[b];
  for(unsigned mode=0;mode<3;++mode){
   unsigned flashes=0,last=96;
   for(unsigned tick=0;tick<8*t.beat;tick+=15){
    unsigned v=sequence_brightness(1,3,3,mode?3:-1,tick,mode==2,&t);
    assert(v==96||v==255);if(v==255&&last!=255)++flashes;last=v;
    assert(sequence_brightness(1,2,3,mode?3:-1,tick,0,&t)==192);
    assert(sequence_brightness(0,2,3,mode?3:-1,tick,0,&t)==0);
   }
   assert(flashes==(mode==0?2u:mode==1?8u:16u));
  }
 }
 t.bpm=120;
 assert(sequence_brightness(0,3,3,-1,0,0,&t)==255);
 assert(sequence_brightness(0,3,3,-1,1000,0,&t)==96);
 assert(sequence_brightness(0,4,3,3,480,1,&t)==255);
 assert(sequence_brightness(0,4,3,3,720,1,&t)==96);
 assert(sequence_brightness(1,3,3,3,960,0,&t)==255); /* Handoff: ordinary beat. */
 assert(sequence_brightness(1,3,3,3,1440,0,&t)==96);
 struct sequence_clock c={0};
 assert(sequence_clock_time(&c,1000,0,1,800,120)==0);
 assert(sequence_clock_time(&c,1500,0,1,800,120)==960);
 assert(sequence_clock_time(&c,2000,0,1,800,60)==1440); /* Live stopped tempo. */
 assert(sequence_clock_time(&c,2100,1,2,3840,120)==3840);
 assert(sequence_clock_time(&c,2350,1,2,3840,120)==4320); /* Between native updates. */
 assert(sequence_clock_time(&c,2400,1,3,4800,120)==4800);
 assert(sequence_clock_time(&c,2450,1,4,0,120)==0); /* Loop/seek resets phase. */
 assert(sequence_clock_time(&c,2500,0,5,0,120)==0); /* Stop is a new bar. */
 /* Native tempo/meter ABI; invalid owners fall back without dereferencing. */
 FIELD((void*)base,0x71e3dc8,void*)=(void*)(base+0x1000);
 FIELD((void*)(base+0x1000),0,uintptr_t)=base+0x6e07288;FIELD((void*)(base+0x1000),0xd8,void*)=(void*)(base+0x2000);
 FIELD((void*)(base+0x2000),0,uintptr_t)=base+0x6e0c430;FIELD((void*)(base+0x2000),0x148,void*)=(void*)(base+0x3000);
 FIELD((void*)(base+0x3000),0,uintptr_t)=base+0x6e0cf38;FIELD((void*)(base+0x3000),0x490,void*)=(void*)(base+0x4000);
 FIELD((void*)(base+0x4000),0,uintptr_t)=base+0x6d8a418;
 stub(0x13ad070,test_position);__builtin___clear_cache((char*)base,(char*)base+0x7500000);
 test_bpm=137;test_numerator=3;t=sequence_timing();assert(t.bpm==137&&t.beat==960&&t.bar==2880);
 test_numerator=6;test_denominator=8;t=sequence_timing();assert(t.beat==480&&t.bar==2880);
 assert(sequence_brightness(1,3,3,3,480,0,&t)==255);
 assert(sequence_brightness(1,4,3,3,240,1,&t)==255);
 test_denominator=3;test_bpm=NAN;t=sequence_timing();assert(t.bpm==120&&t.beat==960&&t.bar==3840);
 test_denominator=4;test_numerator=4;test_bpm=120;
 sample_visible=0;seq_visible=1;transport=3;seq_selected=0;seq_present=3;
 assert(music_set_sequence_colour(1,12));
 for(unsigned hue=1;hue<MUSIC_SEQUENCE_COLOURS;++hue){
  assert(music_set_sequence_colour(0,hue));seq_tick=0;++seq_serial;send_step_lights();
  assert(sent[0]==music_identity_rgb(hue)&&sent[1]==scale(music_sequence_colours[12],192));
  seq_tick=480;++seq_serial;send_step_lights();
  assert(sent[0]==scale(music_identity_rgb(hue),96)&&sent[1]==scale(music_sequence_colours[12],192));
 }
 /* Native breathing identifies an occupied or empty queued target, while
  * its assigned colour and double-rate pulse remain under our UI clock. */
 for(unsigned empty=0;empty<2;++empty){
  seq_present=empty?1:3;
  for(unsigned v=10;v<=30;v+=10){native_rgb(1,v<<8);seq_tick=480;++seq_serial;send_step_lights();}
  assert(sent[1]==music_sequence_colours[12]);
  seq_tick=720;++seq_serial;send_step_lights();assert(sent[1]==scale(music_sequence_colours[12],96));
 }
 transport=2;seq_selected=-1;seq_present=0xffff;send_step_lights();
 assert(sent[0]==scale(music_sequence_colours[music_sequence_colour(0)],192)&&sent[1]==scale(music_sequence_colours[12],192));
 seq_visible=0;
 puts("PASS Seq pulse timing: stopped bar, playing beat, queued half-beat, visible background, all assigned hues, unchanged other-pad brightness, empty queue, tempo20..300, 3/4 and6/8, native clock interpolation/loop/stop and live tempo");
}
int main(void){
 void*p=mmap(NULL,0x7500000,7,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(p!=MAP_FAILED);base=(uintptr_t)p;
 stub(phone_hooks[H_RGB].trampoline,capture);stub(0x23b00c0,convert);stub(phone_hooks[H_BUTTON_LIGHT].trampoline,indexed_capture);
 __builtin___clear_cache(p,(char*)p+0x7500000);music_reset();
 for(unsigned raw=0;raw<MUSIC_SEQUENCE_COLOURS;++raw){
  assert(music_set_pad_colour(0,raw));native_rgb(0,0x008000);send_step_lights();
  assert(sent[0]==scale(music_identity_rgb(raw),128));
  sample_visible=0;send_step_lights();assert(sent[0]==0x008000);sample_visible=1;
 }
 native_workflow=1;sample_visible=0;steps_lighting=1;steps_lights[0]=0x2030ff;send_step_lights();
 atomic_store(&pad_colour_cursor_white,1);native_rgb(0,0xffffff);native_rgb(2,0xffffff);
 assert(sent[0]==PAD_DANGER&&sent[2]==PAD_FREE);
 uint64_t selected_light;assert(pad_colour_light(0,&selected_light));
 atomic_store(&pad_colour_cursor_white,0);uint64_t dimmed;assert(pad_colour_light(0,&dimmed));
 assert((uint32_t)dimmed==PAD_DANGER&&dimmed!=selected_light);
 native_workflow=2;send_step_lights();native_rgb(0,0xff0000);native_rgb(2,0xff0000);
 assert(sent[0]==PAD_DANGER&&sent[2]==PAD_FREE); /* native red marks a destination, not its occupancy */
 steps_lighting=0;native_workflow=0;sample_visible=1;levels_on=1;send_step_lights();assert(sent[0]==0xff0000);levels_on=0;
 levels_on=1;levels_type=8;levels_origin=1;assert(music_set_pad_colour(1,13));send_step_lights();
 atomic_store(&pad_colour_cursor_white,1);native_rgb(0,0xff7700);assert(sent[0]==PAD_SELECT);
 atomic_store(&pad_colour_cursor_white,0);native_rgb(0,0xff7700);assert(sent[0]==music_identity_rgb(13));
 native_rgb(2,0x000080);assert(sent[2]==music_identity_rgb(13));native_rgb(3,0);assert(sent[3]==0);
 levels_on=0;levels_type=0;levels_origin=0;
 sample_visible=0;seq_visible=1;seq_selected=-1;transport=0;seq_present=3;
 for(unsigned raw=0;raw<MUSIC_SEQUENCE_COLOURS;++raw){assert(music_set_sequence_colour(0,raw));send_step_lights();assert(sent[0]==scale(music_identity_rgb(raw),192));}
 seq_visible=0;send_step_lights();assert(sent[0]==0xff7700);
 steps_on=steps_lighting=1;memset(steps_lights,0,sizeof(steps_lights));steps_lights[12]=PAD_SELECT;
 send_step_lights();for(unsigned i=0;i<16;++i)button_light((void*)0x1234,1,i);assert(white_count()==1);
 puts("PASS five identity colours and all legacy IDs; native/browser occupancy independent of selected RGB; red/green targets remain semantic, Levels/FX ownership, button LEDs isolated");
 munmap(p,0x7500000);return 0;
}
