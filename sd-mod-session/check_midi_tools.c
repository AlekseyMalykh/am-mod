#include "src/midi_tools.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct am_midi_message parsed[16];static unsigned count;
static void emit(void*u,const struct am_midi_message*m){(void)u;assert(count<16);parsed[count++]=*m;}
int main(void){
 struct am_midi_parser p={0};const unsigned char bytes[]={0x90,60,100,0xf8,61,0,0xc2,12,0xf0,1,2,0xf8,0xf7,0xf2,1,2};
 for(unsigned i=0;i<sizeof bytes;++i)am_midi_parse(&p,bytes[i],emit,NULL);
 assert(count==7&&parsed[0].data[0]==0x90&&parsed[1].data[0]==0xf8&&parsed[2].data[0]==0x90&&!parsed[2].data[2]&&parsed[2].data[1]==61&&parsed[3].size==2&&parsed[4].data[0]==0xf8&&parsed[5].data[0]==0xf0&&parsed[6].data[0]==0xf2);
 int notes[128];for(unsigned i=0;i<128;++i)notes[i]=(int)i;notes[5]=41;notes[41]=-1;am_midi_native_notes(notes);am_midi_reset();
 struct am_midi_map m={AM_MIDI_INPUT_NOTE,AM_MIDI_USB,1,60,AM_MIDI_NOTE,AM_MIDI_USB,3,67};assert(am_midi_map_set(5,&m));
 struct am_midi_map duplicate=m;assert(!am_midi_map_set(6,&duplicate));duplicate.in_port=AM_MIDI_EXTERNAL;assert(am_midi_map_set(6,&duplicate));
 struct am_midi_router input={0},external={0},output={0};struct am_midi_message a={3,{0x90,60,99}},b;
 assert(am_midi_translate(&input,AM_MIDI_IN,AM_MIDI_USB,&a,&b)&&b.data[1]==41);
 assert(am_midi_translate(&external,AM_MIDI_IN,AM_MIDI_EXTERNAL,&a,&b)&&b.data[1]==6);
 a.data[1]=41;assert(am_midi_translate(&output,AM_MIDI_OUT,AM_MIDI_USB,&a,&b)&&b.data[0]==0x92&&b.data[1]==67);
 m.out_number=72;m.out_channel=6;assert(am_midi_map_set(5,&m));
 a.data[0]=0xa0;assert(am_midi_translate(&output,AM_MIDI_OUT,AM_MIDI_USB,&a,&b)&&b.data[0]==0xa2&&b.data[1]==67&&b.data[2]==99);
 a.data[1]=60;assert(am_midi_translate(&input,AM_MIDI_IN,AM_MIDI_USB,&a,&b)&&b.data[0]==0xa0&&b.data[1]==41);
 a.data[0]=0x80;a.data[1]=41;
 assert(am_midi_translate(&output,AM_MIDI_OUT,AM_MIDI_USB,&a,&b)&&b.data[0]==0x82&&b.data[1]==67);
 char encoded[AM_MIDI_PAYLOAD],broken[AM_MIDI_PAYLOAD];assert(am_midi_encode(encoded,sizeof encoded)==2063);
 am_midi_reset();assert(am_midi_restore(encoded));struct am_midi_map restored;assert(am_midi_map_get(5,&restored)&&!memcmp(&restored,&m,sizeof m));
 memcpy(broken,encoded,sizeof broken);broken[27]=broken[27]=='0'?'1':'0';assert(!am_midi_restore(broken));assert(am_midi_map_get(5,&restored)&&!memcmp(&restored,&m,sizeof m));
 assert(!am_midi_restore("AMM2 "));assert(am_midi_restore(NULL));
 m=(struct am_midi_map){0,0,0,0,AM_MIDI_CC,0,1,74};assert(am_midi_map_set(5,&m));memset(&output,0,sizeof output);a=(struct am_midi_message){3,{0x90,41,77}};
 assert(am_midi_translate(&output,AM_MIDI_OUT,AM_MIDI_USB,&a,&b)&&b.data[0]==0xb0&&b.data[1]==74&&b.data[2]==77);
 a.data[0]=0xa0;assert(!am_midi_translate(&output,AM_MIDI_OUT,AM_MIDI_USB,&a,&b));
 a.data[0]=0x80;assert(am_midi_translate(&output,AM_MIDI_OUT,AM_MIDI_USB,&a,&b)&&b.data[0]==0xb0&&!b.data[2]);
 m.out_kind=AM_MIDI_NOTE;m.out_channel=3;m.out_number=67;assert(am_midi_map_set(5,&m));a=(struct am_midi_message){3,{0x90,41,90}};assert(am_midi_translate(&output,1,1,&a,&b));count=0;am_midi_router_panic(&output,0,emit,NULL);assert(count==3&&parsed[0].data[0]==0x82&&parsed[0].data[1]==67&&parsed[1].data[1]==64&&parsed[2].data[1]==123);
 am_midi_log_clear();am_midi_log_pause(0);for(unsigned i=0;i<AM_MIDI_LOG+20;++i)am_midi_observe(AM_MIDI_IN,AM_MIDI_USB,&a);
 struct am_midi_record log[AM_MIDI_LOG];uint64_t latest;assert(am_midi_snapshot(log,AM_MIDI_LOG,&latest)==AM_MIDI_LOG);assert(log[AM_MIDI_LOG-1].serial-log[0].serial==AM_MIDI_LOG-1);
 am_midi_observe(AM_MIDI_IN,AM_MIDI_SURFACE,&a);uint64_t after;am_midi_snapshot(log,1,&after);assert(after==latest);
 am_midi_log_pause(1);am_midi_observe(AM_MIDI_OUT,AM_MIDI_USB,&a);am_midi_snapshot(log,1,&after);assert(after==latest);
 am_midi_reset();am_midi_learn(9,1);a=(struct am_midi_message){3,{0xb3,71,100}};am_midi_observe(AM_MIDI_IN,AM_MIDI_EXTERNAL,&a);am_midi_learn_tick();assert(!am_midi_learn_active());am_midi_map_get(9,&m);assert(m.in_kind==AM_MIDI_INPUT_CC&&m.in_port==AM_MIDI_EXTERNAL&&m.in_channel==4&&m.in_number==71);


 /* Incoming-note modes: per-channel pad + Tune/Chop; explicit maps win; ch without a pad = stock. */
 am_midi_reset();am_midi_native_notes(notes);
 struct am_midi_mode md;am_midi_mode_get(&md);assert(md.mode==AM_MIDI_MODE_DRUM&&md.root==60&&!am_midi_mode_active());
 memset(md.pad,0,sizeof md.pad);memset(md.kind,0,sizeof md.kind);md.mode=AM_MIDI_MODE_CHANNELS;md.pad[1]=6;md.pad[2]=9;md.kind[2]=AM_MIDI_KEY_CHOP;assert(am_midi_mode_set(&md)&&am_midi_mode_active());
 struct am_midi_router mr={0};int tune,chop;
 a=(struct am_midi_message){3,{0x91,72,100}};assert(am_midi_translate(&mr,AM_MIDI_IN,AM_MIDI_USB,&a,&b)&&b.data[0]==0x91&&b.data[1]==41&&b.data[2]==100);
 assert(am_midi_tune_take(41,&tune,&chop)&&tune==12&&!chop&&!am_midi_tune_take(41,&tune,&chop));
 a.data[1]=36;assert(am_midi_translate(&mr,AM_MIDI_IN,AM_MIDI_USB,&a,&b)&&b.data[1]==41);assert(am_midi_tune_take(41,&tune,&chop)&&tune==-24);
 a.data[1]=96;assert(am_midi_translate(&mr,AM_MIDI_IN,AM_MIDI_USB,&a,&b)&&b.data[1]==41);assert(am_midi_tune_take(41,&tune,&chop)&&tune==36);
 a=(struct am_midi_message){3,{0x81,72,0}};assert(!am_midi_translate(&mr,AM_MIDI_IN,AM_MIDI_USB,&a,&b));
 a.data[1]=36;assert(!am_midi_translate(&mr,AM_MIDI_IN,AM_MIDI_USB,&a,&b));
 a.data[1]=96;assert(am_midi_translate(&mr,AM_MIDI_IN,AM_MIDI_USB,&a,&b)&&b.data[0]==0x81&&b.data[1]==41); /* last voice releases the pad */
 a=(struct am_midi_message){3,{0x92,62,90}};assert(am_midi_translate(&mr,AM_MIDI_IN,AM_MIDI_USB,&a,&b)&&b.data[1]==8);assert(am_midi_tune_take(8,&tune,&chop)&&tune==3&&chop); /* chop: slice 1+2 */
 a.data[1]=59;assert(!am_midi_translate(&mr,AM_MIDI_IN,AM_MIDI_USB,&a,&b)); /* below Root: no slice */
 a.data[1]=76;assert(!am_midi_translate(&mr,AM_MIDI_IN,AM_MIDI_USB,&a,&b)); /* above 16 keys */
 a=(struct am_midi_message){3,{0x90,60,90}};assert(am_midi_translate(&mr,AM_MIDI_IN,AM_MIDI_USB,&a,&b)&&b.data[1]==60); /* channel 1: stock */
 m=(struct am_midi_map){AM_MIDI_INPUT_NOTE,0,2,48,AM_MIDI_NATIVE,0,0,0};assert(am_midi_map_set(11,&m));
 a=(struct am_midi_message){3,{0x91,48,90}};{struct am_midi_router fresh={0};assert(am_midi_translate(&fresh,AM_MIDI_IN,AM_MIDI_USB,&a,&b)&&b.data[1]==11&&!am_midi_tune_take(11,&tune,&chop));} /* explicit map wins */
 {char saved[AM_MIDI_PAYLOAD];assert(am_midi_encode(saved,sizeof saved)==4439&&!memcmp(saved,"AMM4 ",5));
  am_midi_reset();am_midi_mode_get(&md);assert(md.mode==AM_MIDI_MODE_DRUM&&!md.pad[1]);
  assert(am_midi_restore(saved));am_midi_mode_get(&md);assert(md.mode==AM_MIDI_MODE_CHANNELS&&md.pad[1]==6&&md.pad[2]==9&&md.kind[2]==AM_MIDI_KEY_CHOP&&md.root==60);
  saved[5+2178*2]='f';assert(!am_midi_restore(saved));
  am_midi_reset();assert(am_midi_encode(saved,sizeof saved)==2063);}
 /* Selected-pad keyboard routing: fixed/omni channel, release captured before
  * selection/mode changes, and the project payload preserves the choice. */
 am_midi_reset();am_midi_native_notes(notes);am_midi_selected_pad(5);
 am_midi_mode_get(&md);md.mode=AM_MIDI_MODE_SELECTED;md.channel=2;assert(am_midi_mode_set(&md));
 {struct am_midi_router sr={0};char saved[AM_MIDI_PAYLOAD];
  a=(struct am_midi_message){3,{0x91,67,100}};
  assert(am_midi_translate(&sr,AM_MIDI_IN,AM_MIDI_USB,&a,&b)&&b.data[1]==41);
  assert(am_midi_tune_take(41,&tune,&chop)&&tune==7&&!chop);
  am_midi_selected_pad(8);md.mode=AM_MIDI_MODE_DRUM;assert(am_midi_mode_set(&md));
  a.data[0]=0x81;assert(am_midi_translate(&sr,AM_MIDI_IN,AM_MIDI_USB,&a,&b)&&b.data[1]==41);
  md.mode=AM_MIDI_MODE_SELECTED;md.channel=0;md.root=48;assert(am_midi_mode_set(&md));
  a=(struct am_midi_message){3,{0x9f,60,90}};
  assert(am_midi_translate(&sr,AM_MIDI_IN,AM_MIDI_USB,&a,&b)&&b.data[1]==8);
  assert(am_midi_tune_take(8,&tune,&chop)&&tune==12&&!chop);
  assert(am_midi_encode(saved,sizeof saved)>0);am_midi_reset();assert(am_midi_restore(saved));
  am_midi_mode_get(&md);assert(md.mode==AM_MIDI_MODE_SELECTED&&!md.channel&&md.root==48);
  am_midi_selected_pad(-1);memset(&sr,0,sizeof sr);
  assert(!am_midi_translate(&sr,AM_MIDI_IN,AM_MIDI_USB,&a,&b));
 }
 /* Controller sources for native buttons (transport) */
 am_midi_reset();
 {struct am_midi_message cc={3,{0xb0,20,127}},ccoff={3,{0xb0,20,0}},note={3,{0x90,40,100}},noteoff={3,{0x80,40,0}};unsigned code=0,down=9;char saved[AM_MIDI_PAYLOAD];
  am_midi_button_learn(0x52,1);assert(am_midi_button_learning()==0x53);
  am_midi_observe(AM_MIDI_IN,AM_MIDI_USB,&cc);am_midi_learn_tick();assert(!am_midi_button_learning());
  struct am_midi_router br={0};
  assert(!am_midi_translate(&br,AM_MIDI_IN,AM_MIDI_USB,&cc,&b)&&am_midi_button_take(&code,&down)&&code==0x52&&down==1);
  assert(!am_midi_translate(&br,AM_MIDI_IN,AM_MIDI_USB,&ccoff,&b)&&am_midi_button_take(&code,&down)&&code==0x52&&down==0&&!am_midi_button_take(&code,&down));
  am_midi_button_learn(0x51,1);am_midi_observe(AM_MIDI_IN,AM_MIDI_EXTERNAL,&note);am_midi_learn_tick();
  assert(!am_midi_translate(&br,AM_MIDI_IN,AM_MIDI_EXTERNAL,&note,&b)&&am_midi_button_take(&code,&down)&&code==0x51&&down==1);
  assert(!am_midi_translate(&br,AM_MIDI_IN,AM_MIDI_EXTERNAL,&noteoff,&b)&&am_midi_button_take(&code,&down)&&code==0x51&&down==0);
  am_midi_button_learn(0x50,1);am_midi_observe(AM_MIDI_IN,AM_MIDI_USB,&cc);am_midi_learn_tick(); /* same source: moves to Rec, Play loses it */
  assert(!am_midi_translate(&br,AM_MIDI_IN,AM_MIDI_USB,&cc,&b)&&am_midi_button_take(&code,&down)&&code==0x50&&!am_midi_button_take(&code,&down));
  int n=am_midi_encode(saved,sizeof saved);assert(n>0&&n<=(int)AM_MIDI_PAYLOAD&&!memcmp(saved,"AMM5 ",5));
  am_midi_reset();assert(am_midi_encode(saved+0,sizeof saved)==2063);
  am_midi_button_learn(0x52,1);am_midi_observe(AM_MIDI_IN,AM_MIDI_USB,&cc);am_midi_learn_tick();assert(am_midi_encode(saved,sizeof saved)==4599);
  am_midi_reset();assert(am_midi_restore(saved));
  assert(!am_midi_translate(&br,AM_MIDI_IN,AM_MIDI_USB,&cc,&b)&&am_midi_button_take(&code,&down)&&code==0x52);
  saved[5+2216*2]='f';assert(!am_midi_restore(saved)); /* corrupt CRC rejected */
  am_midi_button_clear();assert(!am_midi_translate(&br,AM_MIDI_IN,AM_MIDI_USB,&cc,&b)||b.data[0]==0xb0);
 }
 /* knob / encoder sources */
 am_midi_reset();
 {struct am_midi_message k2={3,{0xb0,21,100}},enc_up={3,{0xb0,22,65}},enc_down={3,{0xb0,22,63}};unsigned code=0,value=0;struct am_midi_router kr={0};
  am_midi_button_learn(0x80|0x11,1);am_midi_observe(AM_MIDI_IN,AM_MIDI_USB,&k2);am_midi_learn_tick();
  assert(!am_midi_translate(&kr,AM_MIDI_IN,AM_MIDI_USB,&k2,&b)&&am_midi_button_take(&code,&value)&&code==0x91&&value==100);
  am_midi_button_learn(0x80|0x64,1);am_midi_observe(AM_MIDI_IN,AM_MIDI_USB,&enc_up);am_midi_learn_tick();
  assert(!am_midi_translate(&kr,AM_MIDI_IN,AM_MIDI_USB,&enc_up,&b)&&am_midi_button_take(&code,&value)&&code==0xe4&&value==1);
  assert(!am_midi_translate(&kr,AM_MIDI_IN,AM_MIDI_USB,&enc_down,&b)&&am_midi_button_take(&code,&value)&&code==0xe4&&value==127);
  char saved[AM_MIDI_PAYLOAD];assert(am_midi_encode(saved,sizeof saved)==4599);am_midi_reset();assert(am_midi_restore(saved));
  assert(!am_midi_translate(&kr,AM_MIDI_IN,AM_MIDI_USB,&k2,&b)&&am_midi_button_take(&code,&value)&&code==0x91&&value==100);
 }
 am_midi_reset();
 am_midi_log_clear();assert(am_midi_snapshot(log,AM_MIDI_LOG,&latest)==0);puts("PASS MIDI parser, maps, captured releases, transactional codec, bounded monitor and Learn");return 0;
}
