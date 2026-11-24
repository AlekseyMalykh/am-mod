/* Native music-executor owner, compiled only in the separate playback trial.
 * The stock backend is ALWAYS called. This tap adds an owned Synth voice only
 * after both stock channel/note/velocity filters accepted its command. */
static struct am_synth_sequence synth_player;
static uint64_t synth_generation,synth_transport_stop,synth_faults,synth_last_offset;
static void(*native_synth_drain)(void*,uint64_t,unsigned);
static am_synth_engine_event_fn native_synth_engine_event;
static am_synth_track_cancel_fn native_synth_track_cancel;
static _Thread_local struct am_synth_track_cancel_scope*synth_track_cancel;
static _Thread_local struct am_synth_drain_scope*synth_drain;
_Static_assert(LIVE_COUNT==AM_SYNTH_DRAIN_OCCURRENCES,"Native drain scope must cover the existing live pool");
static void synth_cancel(void){
 ++synth_faults;
 if(am_synth_sequence_stop(&synth_player)!=AM_SYNTH_SEQUENCE_ACCEPTED)(void)am_synth_owned_reset();
}
static void synth_finish(struct live_note*n){
 if(!n->synth.occurrence)return;
 int result=am_synth_sequence_off(&synth_player,&n->synth,n->synth.epoch,n->synth.transport,n->synth.occurrence);
 if(result==AM_SYNTH_SEQUENCE_BUSY)synth_cancel();
 memset(&n->synth,0,sizeof n->synth);
}
static void synth_retire_cancelled(void){
 /* Only after a project generation change. A physical Stop request does NOT
  * prove that the native due heap was drained; retain its tombstones.
  * Never dereference retired native backend/track pointers here. */
 for(unsigned i=0;i<LIVE_COUNT;++i)if(live[i].used&&live[i].synth_handled){
  memset(&live[i].synth,0,sizeof live[i].synth);live[i].synth_handled=live[i].synth_cancel_pending=0;live[i].synth_heap=0;
  if(!live[i].route_count&&live+i!=live_event)live_forget(live+i);
 }
}
static void synth_scope_tick(void){
 uint64_t generation=am_synth_archive_generation(),stop=am_synth_route_stop_serial();
 if(synth_generation==generation&&synth_transport_stop==stop)return;
 if(synth_player.epoch)am_synth_sequence_stop(&synth_player);
 if(synth_generation!=generation)synth_retire_cancelled();
 else for(unsigned i=0;i<LIVE_COUNT;++i)if(live[i].synth_handled)memset(&live[i].synth,0,sizeof live[i].synth);
 synth_generation=generation;synth_transport_stop=stop;
}
static int synth_release_same(const struct live_note*n,void*engine,const struct step_event*entry,const struct am_midi_event*event){
 if(n->synth_heap!=(uintptr_t)engine+0x6c8)return 0;
 unsigned channel;memcpy(&channel,(const char*)entry+76,4);if(channel!=n->synth_source_channel)return 0;
 if(!n->synth_has_anchor)return 2*live_same(&n->source,event);
 int64_t end,anchor;memcpy(&end,(const char*)entry+56,8);
 if(__builtin_sub_overflow(end,event->duration,&anchor))return 0;
 /* Native tail clamping changes due and duration by the same amount. Its
  * absolute gate start remains the identity; no duration timer is invented. */
 if(anchor!=n->synth_anchor||n->source.source_note!=event->source_note||n->source.tick!=event->tick||n->source.tune!=event->tune||n->source.slice!=event->slice)return 0;
 /* Prefer unchanged duration: simultaneous unisons with different lengths
  * share the anchor, but must still end in their original individual order. */
 return n->source.duration==event->duration?2:1;
}
static void synth_native_schedule(void*heap,const void*entry){
 struct am_synth_event_scope*scope=synth_event_scope;uint64_t id=scope?scope->accepted:0;
 struct am_midi_event event;int64_t end,anchor=0;int known=0;
 if(id&&heap==(char*)scope->engine+0x6c8&&am_midi_event_copy(entry,(am_midi_choice_get)(image+0x27e4230),AM_MIDI_EVENT_NOTE_ON,&event)){
  memcpy(&end,(const char*)entry+56,8);known=!__builtin_sub_overflow(end,event.duration,&anchor);
 }
 ((void(*)(void*,const void*))(image+0x212bc10))(heap,entry);
 if(scope)scope->accepted=0;
 if(id)for(unsigned i=0;i<LIVE_COUNT;++i){struct live_note*n=live+i;
  if(n->used&&n->id==id&&n->synth_handled&&n->synth_heap==(uintptr_t)heap&&n->track==(uintptr_t)scope->track){
   if(known&&live_same(&n->source,&event)){n->synth_anchor=anchor;n->synth_has_anchor=1;}
   else{++synth_faults;n->synth_cancel_pending=1;live_finish(n,0);}
   break;
  }
 }
}
static void synth_drain_begin(struct am_synth_drain_scope*scope){scope->previous=synth_drain;synth_drain=scope;}
static void synth_drain_end(struct am_synth_drain_scope*scope,int success){
 uintptr_t begin=0,end=1;
 if(success){memcpy(&begin,(char*)scope->engine+0x6c8,8);memcpy(&end,(char*)scope->engine+0x6d0,8);}
 if(success&&begin==end)for(unsigned i=0;i<LIVE_COUNT;++i){struct live_note*n=live+i;
  if(scope->ids[i]&&n->used&&n->id==scope->ids[i]&&n->synth_handled){
   memset(&n->synth,0,sizeof n->synth);n->synth_handled=n->synth_cancel_pending=0;
   /* Original backend releases have returned, and the native heap is empty.
    * A slot accepted recursively with another occurrence ID is never erased. */
   if(n!=live_event&&(!n->route_count||n->released))live_forget(n);
  }
 }
 synth_drain=scope->previous;
}
static void synth_native_drain(void*engine,uint64_t offset,unsigned flags){
 am_synth_native_drain_scope(engine,offset,flags,native_synth_drain,synth_drain_begin,synth_drain_end);
}
static unsigned synth_drain_filter(void*filter,unsigned kind,unsigned flags){
 struct am_synth_drain_scope*scope=synth_drain;
 if(scope){
  uintptr_t begin,end;memcpy(&begin,(char*)scope->engine+0x6c8,8);memcpy(&end,(char*)scope->engine+0x6d0,8);
  if(begin&&end>begin&&(end-begin)%96==0){
   uintptr_t track;memcpy(&track,(void*)(begin+80),8);struct am_midi_event event;
   if(filter==(void*)(track+0x750)&&am_midi_event_copy((const struct step_event*)begin,(am_midi_choice_get)(image+0x27e4230),AM_MIDI_EVENT_NOTE_ON,&event)){
    struct live_note*match=NULL;unsigned index=0,best_score=0;
    /* Bounded existing pool scan per entry that native code itself drains. */
    for(unsigned i=0;i<LIVE_COUNT;++i){struct live_note*n=live+i;
     if(!n->used||!n->synth_handled||!n->id||scope->ids[i]==n->id||n->track!=track)continue;
     unsigned score=(unsigned)synth_release_same(n,scope->engine,(const struct step_event*)begin,&event);
     if(score&&(score>best_score||(score==best_score&&(!match||n->id<match->id)))){match=n;index=i;best_score=score;}
    }
    if(match){scope->ids[index]=match->id;live_finish(match,scope->offset);}
   }
  }
 }
 return ((unsigned(*)(void*,unsigned,unsigned))(image+0x282fe30))(filter,kind,flags);
}
static void synth_event_begin(struct am_synth_event_scope*scope){scope->previous=synth_event_scope;synth_event_scope=scope;}
static void synth_event_end(struct am_synth_event_scope*scope,int success){
 synth_event_scope=scope->previous;if(success)return;
 ++synth_faults;
 /* Native insertion can fail AFTER an accepted On. There may then be no
  * corresponding due entry: silence and block this owner until cancellation
  * succeeds, rather than letting an orphan consume the next note's Off. */
 for(unsigned i=0;i<LIVE_COUNT;++i){struct live_note*n=live+i;
  if(n->used&&n->synth_handled&&n->synth_heap==(uintptr_t)scope->engine+0x6c8&&n->track==(uintptr_t)scope->track){n->synth_cancel_pending=1;live_finish(n,0);}
 }
}
static void synth_engine_event(void*engine,uint64_t offset,void*track,const struct step_event*event,unsigned kind,unsigned tag){
 am_synth_native_event_scope(engine,offset,track,event,kind,tag,native_synth_engine_event,synth_event_begin,synth_event_end);
}
static int synth_track_blocked(uintptr_t heap,uintptr_t track){
 struct am_synth_track_cancel_scope*scope=synth_track_cancel;
 for(unsigned depth=0;scope&&depth<8;++depth,scope=scope->previous)
  if(heap==(uintptr_t)scope->engine+0x6c8&&track==(uintptr_t)scope->track)return 1;
 if(scope)return 1; /* bounded fail-closed handling of excessive native reentry */
 for(unsigned i=0;i<LIVE_COUNT;++i)if(live[i].used&&live[i].synth_handled&&live[i].synth_cancel_pending&&live[i].synth_heap==heap&&live[i].track==track)return 1;
 return 0;
}
static void synth_track_cancel_begin(struct am_synth_track_cancel_scope*scope){
 scope->previous=synth_track_cancel;synth_track_cancel=scope;
 for(unsigned i=0;i<LIVE_COUNT;++i){struct live_note*n=live+i;
  if(n->used&&n->synth_handled&&n->synth_heap==(uintptr_t)scope->engine+0x6c8&&n->track==(uintptr_t)scope->track){scope->ids[i]=n->id;n->synth_cancel_pending=1;}
 }
}
static void synth_track_cancel_end(struct am_synth_track_cancel_scope*scope,int success){
 /* Restore first: an exception while forwarding a captured MIDI Off must
  * not leave a pointer to this stack frame installed in TLS. */
 if(synth_track_cancel==scope)synth_track_cancel=scope->previous;
 for(unsigned i=0;i<LIVE_COUNT;++i){struct live_note*n=live+i;uint64_t id=scope->ids[i];
  if(!id||!n->used||n->id!=id||!n->synth_handled)continue;
  if(!success){live_finish(n,0);continue;} /* retain tombstone + pending block until retry/New/Load */
  live_finish(n,0);
  if(n->id!=id)continue;
  n->synth_handled=n->synth_cancel_pending=0;
  if(n!=live_event&&(!n->route_count||n->released))live_forget(n);
 }
}
static void synth_native_track_cancel(void*engine,void*track){
 am_synth_native_track_cancel_scope(engine,track,native_synth_track_cancel,synth_track_cancel_begin,synth_track_cancel_end);
}
static void *synth_instrument_on(void*owner,uint64_t offset,int route,void*out,const void*command){
 (void)route;(void)out;
 struct live_note*previous=live_event,*n=previous;
 if(!n||!n->synth_heap||live_blocked||n->key_owner||n->source.duration<=0||am_synth_archive_loading()||synth_track_blocked(n->synth_heap,n->track))return previous;
 uint64_t generation=am_synth_archive_generation();struct am_synth_route_snapshot binding;
 if(n->synth_generation!=generation||!am_synth_route_read(generation,&binding)||n->synth_stop!=binding.stop)return previous;
 const unsigned char*program;memcpy(&program,(char*)n->track+0xbb8,sizeof program);
 if(!program||owner!=program+0xdf0||n->sound!=binding.carrier)return previous;
 const char*key;memcpy(&key,program+0xdc8,sizeof key);
 if(!key||strnlen(key,128)==128||strcmp(key,binding.program_key))return previous;
 void*instrument=((void*(*)(void*,int))(image+0x286bbe0))((void*)program,(int)binding.carrier);
 if(!instrument||((int(*)(void*))(image+0x2804760))(instrument))return previous;
 unsigned char bytes[44];memcpy(bytes,command,sizeof bytes);uint16_t mask;memcpy(&mask,bytes+12,2);
 /* The recording codec uses Tune relative to MIDI60; unsupported source-note
  * remapping/transpose and other packed choices fail closed in this trial. */
 if(bytes[0]!=n->source.source_note||(mask&~1u))return previous;
 struct am_midi_event accepted;
 if(!am_midi_command_copy(command,&n->source,(am_midi_choice_get)(image+0x27e4230),&accepted))return previous;
 if(synth_player.epoch!=binding.serial&&am_synth_sequence_configure(&synth_player,binding.serial)!=AM_SYNTH_SEQUENCE_ACCEPTED)return previous;
 if(n->synth_handled){n=live_repeat(n);if(!n)return previous;live_event=n;}
 if(!n->id)n->id=am_midi_wire_id();
 if(!n->id)return previous;
 n->synth_handled=1;synth_last_offset=offset;
 if(synth_event_scope)synth_event_scope->accepted=n->id;
 (void)am_synth_sequence_on(&synth_player,&n->synth,synth_player.epoch,synth_player.transport,n->id,&accepted,0);
 /* UI Stop/Load/rebind can invalidate this borrowed command while it is
  * being copied. Cancel a late On even if it acquired the new DSP epoch. */
 struct am_synth_route_snapshot after;
 if(am_synth_archive_loading()||am_synth_archive_generation()!=generation||
    !am_synth_route_read(generation,&after)||after.serial!=binding.serial||after.stop!=binding.stop)synth_cancel();
 return previous;
}
static void synth_instrument_end(void*cookie,int failed){
 if(failed)synth_cancel();
 live_event=cookie;
}
