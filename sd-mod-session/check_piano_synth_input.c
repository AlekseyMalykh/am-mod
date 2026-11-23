/* Actual ThreadQueues, copied native events, real Synth DSP. Engine On and
 * recorder Off receivers are fixtures, explicitly inherited from the normal
 * Piano Roll queue test. No native Project is modified or serialized. */
#define PIANO_INPUT_TEST_MAIN piano_legacy_input_proof
#define PIANO_INPUT_FIXTURE_CAPACITY 128
#include "check_piano_input.c"
#include "src/pad_midi_note.h"
#include "src/synth.h"

static unsigned occupied;
static unsigned char empty_carrier[0x2468],trial_slots[64][0xc0];
static void *carrier(void *p,int source){assert(worker_active&&p==program&&source==2);return empty_carrier;}
static int has_sample(void*p){assert(worker_active&&p==empty_carrier);return (int)occupied;}
static void render(void){float l[256]={0},r[256]={0};am_synth_render_add(l,r,256);for(unsigned i=0;i<256;++i)assert(isfinite(l[i])&&isfinite(r[i]));}
static void quiet(void){for(unsigned i=0;i<40;++i)render();assert(!am_synth_active());}
static unsigned voices(void){render();struct am_synth_state live;am_synth_peek(&live);return live.voices;}
static struct step_event note(unsigned pitch,unsigned velocity){
 struct step_event e;assert(am_pad_midi_note_make(38,pitch,velocity,0,
  FN(0x27e54d0,am_pad_note_factory),FN(0x27e4400,am_pad_note_choice_set),
  FN(0x27e4230,am_midi_choice_get),&e));return e;
}
static void reset_queue(void*q,uint64_t*reader){
 assert(!FIELD(context,0x50,unsigned));*reader=0;memset(q,0,0x48);FIELD(q,0,uint64_t)=64;FIELD(q,0x30,void*)=trial_slots;
 for(unsigned i=0;i<64;++i){FIELD(trial_slots[i],0,uintptr_t)=base+0xb57a7c;FIELD(trial_slots[i],8,uintptr_t)=base+0x15265b0;}
}
int main(int argc,char **argv){
 assert(!piano_legacy_input_proof(argc,argv));phase="trial Synth on existing native Piano input queue";
 stub(0x286bbe0,carrier);stub(0x2804760,has_sample);__builtin___clear_cache((char*)base,(char*)base+0x7500000);
 void*q=queues+0x1d8;uint64_t reader;reset_queue(q,&reader);piano_on=piano_off=piano_last=0;
 am_synth_param_set(2,0);am_synth_param_set(5,0);
 struct step_event low=note(48,31),high=note(67,119);
 assert(!batch_piano_synth_input(base,context,program,&low,0,0));
 assert(batch_piano_synth_input(base,context,program,&low,0,1));
 assert(batch_piano_synth_input(base,context,program,&high,7,2));
 assert(!am_synth_active()&&!piano_on);piano_drain(q,&reader);assert(piano_on==2&&voices()==2);
 assert(FIELD(&piano_notes[0],0x10,float)==31.f/127&&FIELD(&piano_notes[1],0x10,float)==119.f/127);
 assert(batch_piano_synth_input(base,context,NULL,NULL,7,2));piano_drain(q,&reader);assert(piano_off==1&&!piano_last);
 assert(batch_piano_synth_input(base,context,NULL,NULL,0,1));piano_drain(q,&reader);assert(piano_off==2&&piano_last==1);quiet();

 /* Retrigger on one hardware key: the old occurrence cannot release the new. */
 assert(batch_piano_synth_input(base,context,program,&low,0,3));piano_drain(q,&reader);render();
 assert(batch_piano_synth_input(base,context,program,&high,0,4));piano_drain(q,&reader);render();
 unsigned off=piano_off;
 assert(batch_piano_input(base,context,NULL,NULL,0));piano_drain(q,&reader);assert(piano_off==off&&batch_piano_synth_held()==1); /* Late ordinary Piano key-up has no Synth ownership. */
 assert(batch_piano_synth_input(base,context,NULL,NULL,0,3));piano_drain(q,&reader);assert(piano_off==off&&am_synth_active());
 unsigned on=piano_on,faults=batch_piano_synth_faults();
 assert(batch_piano_synth_input(base,context,program,&low,0,3));piano_drain(q,&reader);assert(piano_on==on&&piano_off==off&&batch_piano_synth_faults()==faults+1);
 assert(batch_piano_synth_input(base,context,NULL,NULL,0,4));piano_drain(q,&reader);quiet();

 /* Two keys holding one pitch sound once; releasing either preserves the gate. */
 assert(batch_piano_synth_input(base,context,program,&high,1,5));
 assert(batch_piano_synth_input(base,context,program,&high,2,6));piano_drain(q,&reader);assert(voices()==1);
 assert(batch_piano_synth_input(base,context,NULL,NULL,2,6));piano_drain(q,&reader);render();assert(am_synth_active());
 assert(batch_piano_synth_input(base,context,NULL,NULL,1,5));piano_drain(q,&reader);quiet();

 /* Stop invalidates pending On before the music worker consumes it. */
 on=piano_on;assert(batch_piano_synth_input(base,context,program,&low,0,7));assert(batch_piano_input_stop(base,context));piano_drain(q,&reader);assert(piano_on==on);quiet();
 /* Project switch: queued On rejected, old audio gate released value-only. */
 faults=batch_piano_synth_faults();assert(batch_piano_synth_input(base,context,program,&low,0,8));FIELD(things,0xb8,void*)=NULL;piano_drain(q,&reader);assert(batch_piano_synth_faults()==faults+1&&piano_on==on);
 FIELD(things,0xb8,void*)=program;assert(batch_piano_synth_input(base,context,program,&low,0,9));piano_drain(q,&reader);render();off=piano_off;
 FIELD(things,0xb8,void*)=NULL;assert(batch_piano_input_stop(base,context));piano_drain(q,&reader);assert(piano_off==off);quiet();FIELD(things,0xb8,void*)=program;
 /* Occupancy is revalidated by the worker, after submission. */
 faults=batch_piano_synth_faults();on=piano_on;assert(batch_piano_synth_input(base,context,program,&low,0,10));occupied=1;piano_drain(q,&reader);assert(piano_on==on&&batch_piano_synth_faults()==faults+1);occupied=0;

 /* A pre-timed playback event is not a fresh pad gesture. */
 struct step_event timed=low;timed.time=960;assert(!batch_piano_synth_input(base,context,program,&timed,0,11));

 /* A full Synth queue can also come from legacy CC traffic. An unaccepted
  * Off stays owned; retrying after the consumer drains must close that gate. */
 assert(batch_piano_synth_input(base,context,program,&high,0,12));piano_drain(q,&reader);render();
 for(unsigned i=0;i<64;++i)am_synth_cc(1,i);
 off=piano_off;faults=batch_piano_synth_faults();
 assert(batch_piano_synth_input(base,context,NULL,NULL,0,12));piano_drain(q,&reader);
 assert(piano_off==off&&batch_piano_synth_faults()==faults+1&&batch_piano_synth_held()==1);render();
 assert(batch_piano_synth_input(base,context,NULL,NULL,0,12));piano_drain(q,&reader);assert(piano_off==off+1&&!batch_piano_synth_held());quiet();

 /* Fill the native input queue with retriggers, without draining Synth.
  * Synth reserves Off space and refuses excess Ons BEFORE the recorder handoff. */
 faults=batch_piano_synth_faults();on=piano_on;
 for(unsigned i=0;i<48;++i)assert(batch_piano_synth_input(base,context,program,&high,0,100+i));
 assert(!batch_piano_synth_input(base,context,program,&high,0,200)); /* Native slots reserve 16 releases. */
 piano_drain(q,&reader);assert(batch_piano_input_stop(base,context));piano_drain(q,&reader);
 assert(piano_on>on&&piano_on<on+48&&batch_piano_synth_faults()>faults);quiet();

 /* Native queue stopped: no lifetime leak and no Synth command is emitted. */
 FIELD(q,0x2d,unsigned char)=1;assert(!batch_piano_synth_input(base,context,program,&low,0,201));assert(!FIELD(context,0x50,unsigned)&&!am_synth_active());FIELD(q,0x2d,unsigned char)=0;
 puts("PASS trial Synth/native Piano queue: worker-only note handoff, captured pitch/velocity, reverse releases, occurrence-safe retrigger, shared gates, Stop epochs, project-change release, occupied-carrier rejection, queue backpressure and retained context lifetime; Engine/recorder receivers are fixtures");return 0;
}
