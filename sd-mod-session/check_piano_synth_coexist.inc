/* Existing native input worker + real Synth DSP, with recorder receivers
 * inherited as fixtures. The owned handle models an already playing note;
 * native playback dispatch and its timing are tested by another suite. */
static void settle_synth(void){for(unsigned i=0;i<40;++i)render();}
static void check_piano_synth_coexist(void *q,uint64_t *reader){
 phase="live Synth input coexisting with an owned playback voice";
 struct step_event same=note(67,103);struct am_synth_handle playback;
 unsigned on=piano_on,off=piano_off,faults=batch_piano_synth_faults();
 assert(am_synth_owned_on(67,71,&playback));assert(voices()==1);
 assert(batch_piano_synth_input(base,context,program,&same,0,300));
 piano_drain(q,reader);assert(voices()==2);
 assert(batch_piano_synth_input(base,context,NULL,NULL,0,300));
 piano_drain(q,reader);settle_synth();assert(voices()==1);
 assert(piano_on==on+1&&piano_off==off+1);

 /* The opposite release order must preserve the live pad gate. */
 assert(batch_piano_synth_input(base,context,program,&same,0,301));
 piano_drain(q,reader);assert(voices()==2);
 assert(am_synth_owned_off(playback));settle_synth();assert(voices()==1);
 assert(batch_piano_synth_input(base,context,NULL,NULL,0,301));
 piano_drain(q,reader);quiet();

 /* Repeated worker handoffs at the same pitch do not release the owned
  * occurrence, including stale key-ups and worker-only Stop cleanup. */
 assert(am_synth_owned_on(67,71,&playback));assert(voices()==1);
 for(unsigned i=0;i<8;++i){
  assert(batch_piano_synth_input(base,context,program,&same,0,400+i));
  piano_drain(q,reader);settle_synth();assert(voices()==2);
  if(i){
   unsigned released=piano_off;
   assert(batch_piano_synth_input(base,context,NULL,NULL,0,399+i));
   piano_drain(q,reader);assert(piano_off==released&&batch_piano_synth_held()==1);
  }
 }
 assert(batch_piano_input_stop(base,context));piano_drain(q,reader);
 settle_synth();assert(!batch_piano_synth_held()&&voices()==1);
 assert(am_synth_owned_off(playback));quiet();
 assert(batch_piano_synth_faults()==faults);

 /* Full eight-voice live chord. Queue pressure and voice stealing are
  * separate tests: this case stays within the Synth's actual voice limit. */
 on=piano_on;off=piano_off;
 for(unsigned i=0;i<8;++i){
  struct step_event chord=note(48+i,40+i*10);
  assert(batch_piano_synth_input(base,context,program,&chord,i,500+i));
 }
 piano_drain(q,reader);assert(voices()==8&&batch_piano_synth_held()==8&&piano_on==on+8);
 for(unsigned k=0;k<8;++k){
  unsigned i=(k*3)%8;
  assert(batch_piano_synth_input(base,context,NULL,NULL,i,500+i));
  piano_drain(q,reader);settle_synth();
  assert(voices()==7-k&&batch_piano_synth_held()==7-k&&piano_off==off+k+1);
 }
 quiet();assert(batch_piano_synth_faults()==faults);
 puts("PASS live/playback coexistence: same-pitch gates survive either release order, eight retriggers/stale Off and worker Stop preserve owned playback; eight-voice chord releases independently through native input queue. Recorder receivers are fixtures.");
}
