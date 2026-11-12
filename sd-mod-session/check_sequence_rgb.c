#define MPC_SEQUENCE_COLOURS 1
#define main previous_rgb_proof
#include "check_workflow_rgb.c"
#undef main
int music_pad_colours_used(void){return 0;}
int music_pad_colour(unsigned index){(void)index;return 0;}
int step_native_active(void){return 0;}
int step_native_sound_layout(void){return 0;}

int music_ui_pad_settings_visible(void){return 0;}
int music_ui_palette_light(unsigned pad,uint64_t *out){(void)pad;(void)out;return 0;}
int note_native_sample_visible(void){return 0;}
int music_lighting_levels_state(unsigned*on,unsigned*mode,unsigned*origin){*on=*mode=*origin=0;return 1;}
static int seq_visible,seq_shift,seq_bank;
static int seq_selected=-1;
static unsigned seq_colours[128],seq_present[8]={0x7fff,0x7fff};
const uint32_t music_sequence_colours[MUSIC_SEQUENCE_COLOURS]={0x40ff40,0x32c780,0xffc247,0x8830ff,0xff20b0,0xff0000,0xff6000,0xffe000,0x2030ff,0x98ff00,0x00ffb0,0xffffff,0x00c8ff,0xff70b8};
int music_sequence_colour(unsigned index){assert(index<128);return (int)seq_colours[index];}
int music_ui_sequence_visible(void){return seq_visible;}
unsigned music_ui_sequence_presence(unsigned bank){assert(bank<8);return seq_present[bank];}
int music_ui_sequence_selected(void){return seq_selected;}
unsigned step_native_transport_state(void){return 2;}
int64_t step_native_clock_ticks(uint64_t*s,unsigned*r){if(s)*s=0;if(r)*r=0;return 0;}
int music_ui_owns(unsigned cc){return seq_shift&&cc==0x10;}
int step_native_browser_bank(int value){assert(value==-1);return seq_bank;}
static uint32_t dim(uint32_t c){return (((c>>16)&255)*192/255<<16)|(((c>>8)&255)*192/255<<8)|((c&255)*192/255);}
int main(void){
 assert(!previous_rgb_proof());
 void*p=mmap(NULL,0x7500000,7,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(p!=MAP_FAILED);base=(uintptr_t)p;
 stub(phone_hooks[H_RGB].trampoline,capture);stub(0x23b00c0,convert);__builtin___clear_cache(p,(char*)p+0x7500000);
 for(unsigned i=0;i<16;++i)rgb((void*)0x1234,i==15?0:0xffffff,i);
 seq_visible=1;seq_bank=0;seq_colours[3]=3;send_step_lights();assert(sent[3]==dim(0x8830ff)&&sent[4]==dim(music_identity_rgb(0))&&sent[15]==0);
 seq_colours[3]=1;send_step_lights();assert(sent[3]==dim(music_identity_rgb(1))&&sent[4]==dim(music_identity_rgb(0)));
 seq_bank=1;seq_colours[19]=4;send_step_lights();assert(sent[3]==dim(0xff20b0));
 seq_shift=1;send_step_lights();assert(sent[3]==dim(0xff20b0));
 seq_shift=0;send_step_lights();assert(sent[3]==dim(0xff20b0));
 atomic_store(&native_buttons_down[0x31],1);atomic_store(&native_button_lights[0x2b],3);
 observe_mute_indicator(3);
 send_step_lights();assert(sent[3]==dim(0xff20b0)); /* Shift cue is not Mute. */
 atomic_store(&native_buttons_down[0x31],0);
 send_step_lights();assert(sent[3]==dim(0xff20b0)); /* Release before LED reset: no green frame. */
 atomic_store(&native_button_lights[0x2b],1);observe_mute_indicator(1);
 observe_mute_indicator(3);send_step_lights();assert(sent[3]==0xffffff);
 observe_mute_indicator(1);send_step_lights();assert(sent[3]==dim(0xff20b0));
 atomic_store(&native_buttons_down[0x09],1);send_step_lights();assert(sent[3]==dim(0xff20b0));
 atomic_store(&native_buttons_down[0x09],0);send_step_lights();assert(sent[3]==dim(0xff20b0));
 /* Native green on an unused slot must not become coloured, including Shift. */
 seq_present[1]&=~(1u<<4);rgb((void*)0x1234,0x00ff00,4);send_step_lights();assert(sent[4]==0);
 seq_shift=1;send_step_lights();assert(sent[4]==0&&sent[3]==dim(0xff20b0));seq_shift=0;
 /* Other occupied pads keep their assigned hue and stable baseline. */
 seq_colours[19]=5;rgb((void*)0x1234,0x001000,3);send_step_lights();assert(sent[3]==dim(music_identity_rgb(5)));
 rgb((void*)0x1234,0x00ff00,3);send_step_lights();assert(sent[3]==dim(music_identity_rgb(5)));
 for(unsigned c=0;c<MUSIC_SEQUENCE_COLOURS;++c){seq_colours[19]=c;send_step_lights();assert(sent[3]==dim(music_identity_rgb(c)));}
 seq_selected=20;send_step_lights();assert(sent[4]!=0); /* Selected empty remains available. */
 seq_selected=19;send_step_lights();assert(sent[4]==0);
 struct sequence_timing timing={120,960,3840};
 for(unsigned ticks=0;ticks<4*960;ticks+=240){
  assert(sequence_brightness(1,19,19,19,ticks,0,&timing)==(ticks%960==0?255u:96u));
  assert(sequence_brightness(1,20,19,19,ticks,0,&timing)==192);
  assert(sequence_brightness(0,20,19,-1,ticks,0,&timing)==0);
  assert(sequence_brightness(0,20,19,19,ticks,1,&timing)==(ticks%480==0?255u:96u));
 }
 seq_visible=0;send_step_lights();assert(sent[3]==0x00ff00);
 puts("PASS sequence RGB: Stock + twelve colours, playing blinks with the beat, stable occupied pads, nonzero pulse background, model-empty slots off including Shift, live K1 refresh, bank offset, Mute/Erase ownership, stock restore.");return 0;
}
