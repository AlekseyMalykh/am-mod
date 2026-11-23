/* UI-owned, explicitly armed session prototype. The native worker validates
 * the empty carrier again before every On; no binding is saved in an XPJ. */
#include "pad_midi_note.h"
static void*synth_record_program;
static unsigned synth_record_enabled,synth_record_keys,synth_record_stopping;
static uint64_t synth_record_serial,synth_record_ids[16];
void workflow_synth_record_stop(void){
 synth_record_keys=0;synth_record_stopping=1;
 void*c=(void*)atomic_load(&copy_context);
 if(c)batch_piano_input_stop(base,c);
}
void workflow_synth_record_tick(void){
 if(synth_record_enabled&&current_program()!=synth_record_program){
  workflow_synth_record_stop();synth_record_enabled=0;synth_record_program=NULL;
 }
 if(synth_record_stopping){
  void*c=(void*)atomic_load(&copy_context);
  if(c&&batch_piano_input_stop(base,c)&&!batch_piano_synth_held())synth_record_stopping=0;
 }
}
int workflow_synth_record_toggle(void){
 workflow_synth_record_stop();
 if(synth_record_enabled){synth_record_enabled=0;synth_record_program=NULL;return 0;}
 void*program=current_program();if(!program)return -1;
 void*instrument=FN(0x286bbe0,void*(*)(void*,int))(program,15);
 if(!instrument||FN(0x2804760,int(*)(void*))(instrument))return -1;
 synth_record_program=program;synth_record_enabled=1;return 1;
}
int workflow_synth_pad(unsigned key,unsigned pitch,unsigned velocity,int down){
 if(key>=16)return 0;
 unsigned bit=1u<<key;void*c=(void*)atomic_load(&copy_context);
 if(!down){
  if(!(synth_record_keys&bit))return 0;
  if(c&&batch_piano_synth_input(base,c,NULL,NULL,key,synth_record_ids[key]))synth_record_keys&=~bit;
  else workflow_synth_record_stop();
  return 1;
 }
 if(!synth_record_enabled)return 0;
 if(!c||current_program()!=synth_record_program||synth_record_stopping)return 1;
 if(synth_record_keys&bit)return 1;
 int source=FIELD(synth_record_program,0x12a8+15*4,int);struct step_event note;
 if(source<0||source>=128||!am_pad_midi_note_make((unsigned)source,pitch,velocity,0,
   FN(0x27e54d0,am_pad_note_factory),FN(0x27e4400,am_pad_note_choice_set),
   FN(0x27e4230,am_midi_choice_get),&note))return 1;
 if(!++synth_record_serial)++synth_record_serial;
 if(batch_piano_synth_input(base,c,synth_record_program,&note,key,synth_record_serial)){
  synth_record_ids[key]=synth_record_serial;synth_record_keys|=bit;
 }
 fprintf(stderr,"SYNTH_PAD_RECORD key=%u pitch=%u velocity=%u id=%llu queued=%u\n",
   key,pitch,velocity,(unsigned long long)synth_record_serial,!!(synth_record_keys&bit));
 return 1;
}
