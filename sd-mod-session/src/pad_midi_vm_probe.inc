/* Explicit disposable-VM input only. No constructor action, direct event-tree
 * edits or recorder substitution. Normal builds never compile this file. */
#if !defined(MPC_PAD_MIDI_RECORDING_TRIAL)
#error VM probe requires the isolated Synth trial
#endif
#include "pad_midi_note.h"
#include <inttypes.h>
static void pad_midi_vm_probe_tick(void){
 static unsigned ticks;
 if((++ticks%5)!=0)return;
 FILE*f=fopen("/run/am-pad-midi-vm.request","r");if(!f)return;
 char op[16]={0};unsigned key=0,pitch=0,velocity=0;uint64_t occurrence=0;
 int fields=fscanf(f,"%15s %u %u %u %" SCNu64,op,&key,&pitch,&velocity,&occurrence);
 fclose(f);unlink("/run/am-pad-midi-vm.request");
 void*c=(void*)atomic_load(&copy_context),*program=current_program();int accepted=0;
 if(fields==5&&c&&program&&key<16&&occurrence){
  if(!strcmp(op,"down")){
   int source=FIELD(program,0x12a8+15*4,int);struct step_event event;
   if(source>=0&&source<128&&am_pad_midi_note_make((unsigned)source,pitch,velocity,0,
       FN(0x27e54d0,am_pad_note_factory),FN(0x27e4400,am_pad_note_choice_set),
       FN(0x27e4230,am_midi_choice_get),&event))
    accepted=batch_piano_synth_input(base,c,program,&event,key,occurrence);
  }else if(!strcmp(op,"up"))accepted=batch_piano_synth_input(base,c,NULL,NULL,key,occurrence);
 }else if(fields>=1&&!strcmp(op,"stop")&&c)accepted=batch_piano_input_stop(base,c);
 fprintf(stderr,"PAD_MIDI_VM op=%s key=%u pitch=%u velocity=%u id=%" PRIu64 " queued=%d faults=%u held=%u\n",
   op,key,pitch,velocity,occurrence,accepted,batch_piano_synth_faults(),batch_piano_synth_held());
}
