/* UI-owned, explicitly armed session prototype. The native worker validates
 * the empty carrier again before every On; binding has an optional XPJ field. */
#include "pad_midi_note.h"
#include "synth_archive.h"
#ifdef MPC_SYNTH_NATIVE_PLAYBACK_TRIAL
#include "synth_route.h"
#endif
static void*synth_record_program;
static unsigned synth_record_enabled,synth_record_keys,synth_record_stopping;
static uint64_t synth_record_serial,synth_record_ids[16];
static uint64_t synth_record_generation,synth_applied_generation,synth_applied_revision;
/* UI owner only. Native readers/writers publish one coherent metadata state;
 * no archive mutex is acquired by the audio render or event receiver. */
static void synth_binding_tick(void){
 if(am_synth_archive_loading()){
#ifdef MPC_SYNTH_NATIVE_PLAYBACK_TRIAL
  am_synth_route_clear();
#endif
  return;
 }
 struct am_synth_binding binding;uint64_t revision,generation=am_synth_archive_generation();unsigned mode;
 if(!am_synth_archive_snapshot(&binding,&revision,&mode))return;
 if(mode!=AM_SYNTH_ARCHIVE_KNOWN){
#ifdef MPC_SYNTH_NATIVE_PLAYBACK_TRIAL
  am_synth_route_clear();
#endif
  synth_applied_generation=synth_applied_revision=0;return;
 }
 if(am_synth_archive_loading()||generation!=am_synth_archive_generation())return;
 if(generation!=synth_applied_generation||revision!=synth_applied_revision){
  workflow_synth_record_stop();am_synth_owned_reset();
  for(unsigned i=0;i<AM_SYNTH_PARAMS;++i)am_synth_param_set(i,binding.parameters[i]);
  synth_applied_generation=generation;synth_applied_revision=revision;
 }else{
  unsigned changed=0;for(unsigned i=0;i<AM_SYNTH_PARAMS;++i){unsigned v=am_synth_param_get(i);changed|=v!=binding.parameters[i];binding.parameters[i]=(unsigned char)v;}
  if(changed&&am_synth_archive_replace(&binding,revision)==AM_SYNTH_EDIT_OK)synth_applied_revision=revision+1;
 }
#ifdef MPC_SYNTH_NATIVE_PLAYBACK_TRIAL
 if(!am_synth_archive_loading()&&generation==am_synth_archive_generation())am_synth_route_publish(&binding,generation);
#endif
}
void workflow_synth_record_stop(void){
 synth_record_keys=0;synth_record_stopping=1;
 void*c=(void*)atomic_load(&copy_context);
 if(c)batch_piano_input_stop(base,c);
}
void workflow_synth_record_tick(void){
 if(synth_record_enabled&&(current_program()!=synth_record_program||am_synth_archive_loading()||synth_record_generation!=am_synth_archive_generation())){
  workflow_synth_record_stop();synth_record_enabled=0;synth_record_program=NULL;
 }
 synth_binding_tick();
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
 struct am_synth_binding binding;uint64_t revision;unsigned mode;
 if(!am_synth_archive_snapshot(&binding,&revision,&mode)||am_synth_archive_loading())return -3;
 if(mode==AM_SYNTH_ARCHIVE_OPAQUE)return -2;
 const char*key=FIELD(program,0xdc8,const char*);
 if(!am_synth_binding_capture(key,15,&binding)||am_synth_archive_replace(&binding,revision)!=AM_SYNTH_EDIT_OK)return -3;
 synth_record_generation=am_synth_archive_generation();synth_record_program=program;synth_record_enabled=1;return 1;
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
