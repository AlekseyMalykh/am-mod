/* Production input handlers, without a fabricated native screen/GUI owner. */
#include "src/midi_ui.c"
#include <assert.h>
void am_midi_io_ready(void){}
static unsigned char context[0xc0],program_model[0x2000];
static void*test_context;
void*note_native_context(void){return test_context;}
int music_ui_shift_active(void){return 0;}
static void button(unsigned code,unsigned down){unsigned char e[16]={0};e[8]=(uint8_t)code;e[9]=(uint8_t)down;assert(am_midi_ui_button(e));}
static void tap(unsigned code){button(code,1);button(code,0);}
static void knob_input(unsigned code,unsigned value,unsigned shift){unsigned char e[16]={0};e[8]=(uint8_t)code;e[9]=(uint8_t)value;assert(am_midi_ui_control(e,shift));}
int main(void){
 /* Use a permuted native note map and poisoned legacy key fields. */
 test_context=context;F(context,0xb8,void*)=program_model;
 for(unsigned i=0;i<128;++i){int n=(int)((i+36)%128);F(program_model,0x12a8+i*4,int)=n;F(program_model,0x14a8+n*4,int)=(int)i;}
 am_midi_ui_tick(NULL);
 struct am_midi_map mapped={0};mapped.in_kind=1;mapped.in_number=60;assert(am_midi_map_set(5,&mapped));
 struct am_midi_router router={0};struct am_midi_message input={3,{0x90,60,90}},output;
 assert(am_midi_translate(&router,AM_MIDI_IN,AM_MIDI_USB,&input,&output)&&output.data[1]==41);
 am_midi_reset();test_context=NULL;

 atomic_store(&visible,1);assert(am_midi_ui_owns(0x0d,1));tap(0x0d);assert(atomic_load(&page)==1);tap(0x0e);assert(atomic_load(&page)==2);
 knob_input(0x10,127,0);knob_input(0x11,127,0);knob_input(0x12,127,0);assert(direction==2&&port==2&&type==7);knob_input(0x12,127,1);assert(channel==16);
 am_midi_log_pause(0);struct am_midi_message msg={3,{0x90,60,100}};am_midi_observe(0,1,&msg);tap(0x0d);assert(am_midi_log_paused());
 struct am_midi_record log[8];uint64_t latest;assert(am_midi_snapshot(log,8,&latest)==1);tap(0x0e);assert(!am_midi_snapshot(log,8,&latest));assert(am_midi_log_paused());tap(0x0d);assert(!am_midi_log_paused());
 tap(0x0c);assert(atomic_load(&page)==1);knob_input(0x64,1,0);tap(0x6f);assert(atomic_load(&page)==3);
 knob_input(0x10,127,0);knob_input(0x11,0,0);knob_input(0x12,64,0);struct am_midi_map m;am_midi_map_get(127,&m);assert(m.in_kind==1);
 tap(0x0d);assert(am_midi_learn_active());tap(0x0c);assert(atomic_load(&page)==1&&!am_midi_learn_active());
 knob_input(0x64,1,0);tap(0x0e);assert(atomic_load(&page)==4);
 knob_input(0x10,5,0);knob_input(0x10,127,1);assert(pad==5&&map_layer==AM_MIDI_LAYER_LEVELS);
 knob_input(0x11,0,0);knob_input(0x12,64,0);unsigned enabled;am_midi_layer_get(5,&m,&enabled);assert(enabled&&m.out_kind==AM_MIDI_NOTE);
 struct am_midi_map base_map;am_midi_map_get(5,&base_map);assert(base_map.out_kind==AM_MIDI_NATIVE);
 knob_input(0x11,96,0);knob_input(0x12,72,0);am_midi_layer_get(5,&m,&enabled);assert(enabled&&m.out_number==72);
 tap(0x0d);am_midi_layer_get(5,&m,&enabled);assert(!enabled); /* B2 restores inheritance. */
 knob_input(0x11,64,0);knob_input(0x12,127,0);am_midi_layer_get(5,&m,&enabled);assert(enabled&&m.out_channel==16);
 knob_input(0x11,0,0);knob_input(0x12,0,0);am_midi_layer_get(5,&m,&enabled);assert(!enabled);
 knob_input(0x10,0,1);assert(!map_layer&&pad==5);tap(0x0c);assert(atomic_load(&page)==1);
 /* Release ownership survives a native mode change after a consumed down. */
 button(0x0c,1);assert(atomic_load(&page)==0);atomic_store(&visible,0);assert(am_midi_ui_owns(0x0c,1));button(0x0c,0);assert(!am_midi_ui_owns(0x0c,1));
 direction=port=type=channel=0;am_midi_log_clear();
 for(unsigned i=0;i<20;++i)am_midi_observe(0,1,&msg);
 scroll_by(5);uint64_t anchor=scroll_anchor;assert(anchor);
 for(unsigned i=0;i<10;++i)am_midi_observe(0,1,&msg);
 assert(scroll_anchor==anchor);
 for(unsigned i=0;i<AM_MIDI_LOG;++i)am_midi_observe(0,1,&msg);
 struct am_midi_record wrapped[AM_MIDI_LOG];unsigned indices[AM_MIDI_LOG];
 unsigned count=am_midi_snapshot(wrapped,AM_MIDI_LOG,&latest);for(unsigned i=0;i<count;++i)indices[i]=i;
 assert(log_finish(wrapped,indices,count)==7);scroll_by(-1000);assert(!scroll_anchor);
 puts("PASS production MIDI UI handlers: menu, K filters, Shift channel, B1 Back, B2 Pause, B3 Clear, maps, Learn and paired releases");return 0;
}
