/* Playback ratchets, not physical held-pad Note Repeat recording. Native
 * expansion, queued-On delivery and due-Off are executed from the stock ELF.
 * Recorder presence, monitor bookkeeping and its interception gate are fixtures. */
static void*repeat_recorder(void*service,void*track){(void)service;assert(track==track_data);return instrument_data;}
static unsigned repeat_no_intercept(void*owner,unsigned kind,unsigned flags){(void)owner;(void)kind;(void)flags;return 0;}
static size_t repeat_queue_size(void*engine,unsigned offset){
 uintptr_t begin,end;memcpy(&begin,(char*)engine+offset,8);memcpy(&end,(char*)engine+offset+8,8);assert(end>=begin&&(end-begin)%96==0);return (end-begin)/96;
}
static void check_synth_repeat(void*engine,const struct step_event*original,void*position){
 const uintptr_t sites[]={0x20f8da0,0x282fec0,0x21152d0,0x212c9c0};unsigned char saved[4][16];
 for(unsigned i=0;i<4;++i)memcpy(saved[i],(void*)(base+sites[i]),16);
 patch(sites[0],repeat_recorder);patch(sites[1],repeat_no_intercept);patch(sites[2],optional_release_bookkeeping);patch(sites[3],optional_release_bookkeeping);
 phase="native playback ratchets across loop end";
 struct step_event note=*original;note.time=7640;note.data[36]=(note.data[36]&~7u)|2; /* three native strikes */
 uint64_t origin=7640,phase_tick=7640;memcpy(music_data+200,&origin,8);memcpy((char*)position+304,&phase_tick,8);
 unsigned before=backend_count;uint64_t faults=synth_faults;
 drain_gate_open=1;FN(0x2119d00,am_synth_engine_event_fn)(engine,96,track_data,&note,3,0);
 assert(backend_count==before+1&&count_synth()==1&&repeat_queue_size(engine,0x6e8)==2);
 assert(latest()->source.duration==79&&latest()->synth_has_anchor);
 for(unsigned strike=0;strike<3;++strike){
  uint64_t start=7640+strike*80,end=start+79;
  FN(0x21185a0,void(*)(void*,uint64_t,uint64_t))(engine,end,96);assert(!count_synth());
  if(strike<2){
   origin=start+80;phase_tick=origin%7680;memcpy(music_data+200,&origin,8);memcpy((char*)position+304,&phase_tick,8);
   FN(0x2124c80,void(*)(void*,uint64_t))(engine,112);
   assert(backend_count==before+strike+2&&count_synth()==1);
  }
 }
 drain_gate_open=0;assert(repeat_queue_size(engine,0x6e8)==0&&repeat_queue_size(engine,0x6c8)==0&&faults==synth_faults);
 for(unsigned i=0;i<4;++i){memcpy((void*)(base+sites[i]),saved[i],16);__builtin___clear_cache((char*)base+sites[i],(char*)base+sites[i]+16);}
 origin=10000;phase_tick=0;memcpy(music_data+200,&origin,8);memcpy((char*)position+304,&phase_tick,8);
 puts("PASS native ratchets: three strikes/79-tick gates at 7640, 7720, 7800 via stock pending-On and due-Off loops; no extra Synth expansion or faults");
}
