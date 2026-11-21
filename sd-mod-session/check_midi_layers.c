#include "src/midi_route.h"
#include "src/midi_voice.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static char text[AM_MIDI_PAYLOAD],legacy[AM_MIDI_PAYLOAD];
static struct am_midi_voices voices;
static struct am_midi_packet sent[32];static unsigned sent_count;static int queue_blocked;
static int enqueue(void *u,const struct am_midi_packet*p){(void)u;if(queue_blocked)return 0;assert(sent_count<32);sent[sent_count++]=*p;return 1;}
int main(void){
 am_midi_reset();struct am_midi_map pad={0},level={0},read;
 pad.out_kind=AM_MIDI_NOTE;pad.out_number=67;pad.out_channel=3;assert(am_midi_map_set(14,&pad));
 int old=am_midi_encode(legacy,sizeof legacy);assert(old==2063&&!memcmp(legacy,"AMM1",4));
 struct am_midi_event e={.source_note=50,.velocity=101,.tune=-12,.slice=3};struct am_midi_target target;
 assert(am_midi_route_event(14,AM_MIDI_LAYER_PAD,AM_MIDI_USB,1,&e,&target));assert(target.on.data[0]==0x92&&target.on.data[1]==67);
 assert(am_midi_route_event(14,AM_MIDI_LAYER_LEVELS,AM_MIDI_USB,1,&e,&target));assert(target.on.data[0]==0x92&&target.on.data[1]==57&&target.off.data[1]==57);
 level.out_kind=AM_MIDI_NOTE;level.out_number=60;level.out_channel=7;assert(am_midi_layer_set(14,&level,1));
 assert(am_midi_route_event(14,AM_MIDI_LAYER_LEVELS,AM_MIDI_USB,1,&e,&target));assert(target.on.data[0]==0x96&&target.on.data[1]==50);
 struct am_midi_target held=target;level.out_channel=9;assert(am_midi_layer_set(14,&level,1));assert(held.off.data[0]==0x86&&held.off.data[1]==50);
 unsigned enabled;assert(am_midi_layer_get(14,&read,&enabled)&&enabled&&read.out_channel==9);
 int n=am_midi_encode(text,sizeof text);assert(n==4367&&!memcmp(text,"AMM2",4));
 am_midi_reset();assert(am_midi_restore(text));assert(am_midi_layer_get(14,&read,&enabled)&&enabled&&read.out_channel==9);
 assert(am_midi_map_get(14,&read)&&read.out_channel==3);
 text[12]=text[12]=='0'?'1':'0';assert(!am_midi_restore(text));assert(am_midi_layer_get(14,&read,&enabled)&&enabled&&read.out_channel==9);
 assert(am_midi_restore(legacy));assert(am_midi_layer_get(14,&read,&enabled)&&!enabled);
 assert(am_midi_layer_resolve(14,AM_MIDI_LAYER_LEVELS,&read)&&read.out_number==67);
 level.out_kind=AM_MIDI_NOTE;level.out_number=127;level.out_channel=7;e.tune=120;e.slice=128;
 assert(am_midi_layer_set(14,&level,1));assert(!am_midi_route_event(14,AM_MIDI_LAYER_LEVELS,AM_MIDI_USB,1,&e,&target));
 e.tune=0;e.slice=0;level.out_kind=AM_MIDI_CC;level.out_number=74;
 assert(am_midi_layer_set(14,&level,1)&&am_midi_route_event(14,1,1,1,&e,&target));assert(target.on.data[0]==0xb6&&target.off.data[1]==74&&!target.off.data[2]);
 level.out_kind=AM_MIDI_PROGRAM;assert(am_midi_layer_set(14,&level,1)&&am_midi_route_event(14,1,1,1,&e,&target));assert(target.on.size==2&&!target.off.size);
 level.out_kind=AM_MIDI_OFF;assert(am_midi_layer_set(14,&level,1)&&!am_midi_route_event(14,1,1,1,&e,&target));
 level.out_kind=AM_MIDI_NOTE;level.out_port=AM_MIDI_EXTERNAL;assert(am_midi_layer_set(14,&level,1)&&am_midi_route_event(14,1,AM_MIDI_USB,1,&e,&target));
 assert(target.on.data[0]==0x90&&target.on.data[1]==50);level.in_kind=AM_MIDI_INPUT_NOTE;assert(!am_midi_layer_set(14,&level,1));
 /* Exercise real voice ownership for independently typed output targets. */
 struct am_midi_target note_target={1,{3,{0x92,67,100}},{3,{0x82,67,0}}};
 struct am_midi_target cc_target={1,{3,{0xb2,67,90}},{3,{0xb2,67,0}}};
 assert(am_midi_voice_target_on(&voices,1,1,&note_target,enqueue,NULL)==1);
 assert(am_midi_voice_target_on(&voices,2,1,&cc_target,enqueue,NULL)==1);
 assert(sent_count==2&&am_midi_voice_pressure(&voices,2,100,enqueue,NULL)==0);
 assert(am_midi_voice_off(&voices,2,enqueue,NULL)==1&&sent_count==3&&sent[2].message.data[0]==0xb2);
 note_target.on.data[1]=note_target.off.data[1]=72; /* Later edits cannot change voice 1. */
 assert(am_midi_voice_off(&voices,1,enqueue,NULL)==1&&sent[3].message.data[0]==0x82&&sent[3].message.data[1]==67);
 assert(am_midi_voice_target_on(&voices,3,1,&cc_target,enqueue,NULL)==1&&am_midi_voice_target_on(&voices,4,1,&cc_target,enqueue,NULL)==1);
 unsigned before=sent_count;assert(am_midi_voice_off(&voices,3,enqueue,NULL)==1&&sent_count==before);
 queue_blocked=1;assert(am_midi_voice_off(&voices,4,enqueue,NULL)==-1);queue_blocked=0;
 assert(am_midi_voices_retry(&voices,enqueue,NULL)==1&&sent_count==before+1&&sent[before].message.data[0]==0xb2&&!sent[before].message.data[2]);
 struct am_midi_target pc_target={1,{2,{0xc6,10,0}},{0,{0,0,0}}};
 assert(am_midi_voice_target_on(&voices,5,1,&pc_target,enqueue,NULL)==1);before=sent_count;
 assert(am_midi_voice_off(&voices,5,enqueue,NULL)==1&&sent_count==before);
 puts("PASS independent Pad/Levels routes, inherited channel/base, Tune+Chop, captured typed releases, separate Note/CC gates, CC queue retry, Program lifetime, port scope and transactional AMM1/AMM2 (native transport integration separate)");return 0;
}
