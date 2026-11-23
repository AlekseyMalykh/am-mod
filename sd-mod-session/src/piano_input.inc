/* Musical input for Piano Roll. Native Engine/recorder owns timing, duration,
 * quantization and history. UI never edits a recording clip directly. */
extern "C" void poly_record_release_event(void*,void*,unsigned,unsigned,int) __attribute__((weak));
extern "C" void am_midi_native_key_begin(void*,unsigned,unsigned) __attribute__((weak));
extern "C" void am_midi_native_key_end(void) __attribute__((weak));
extern "C" void am_midi_native_key_release(void*,unsigned,void*) __attribute__((weak));
#ifdef MPC_PAD_MIDI_RECORDING_TRIAL
extern "C" {
#include "pad_midi_note.h"
#include "synth.h"
}
#endif
struct PianoInputSlot {
 std::atomic<unsigned> used{0};void *context,*input,*program;
 EventPreviewNote note;unsigned key,down;uint64_t epoch;
#ifdef MPC_PAD_MIDI_RECORDING_TRIAL
 unsigned synth;uint64_t occurrence;
#endif
};
struct PianoInputVoice {void *context,*program,*engine,*track;unsigned source,choice;
#ifdef MPC_PAD_MIDI_RECORDING_TRIAL
 unsigned synth,pitch;uint64_t occurrence;
#endif
};
static PianoInputSlot piano_input_slots[64];
static PianoInputVoice piano_input_voices[16]{}; /* Native worker only. */
static std::atomic<uint64_t> piano_input_epoch{1};
#ifdef MPC_PAD_MIDI_RECORDING_TRIAL
/* Music worker only. The existing Synth API releases all voices of a pitch;
 * shared pitch gates therefore sound once and release on the final owner.
 * The trial Synth queue also accepts UI/MIDI producers; these gate counts
 * cover only the recording worker's keys (the Synth still releases by pitch). */
static unsigned piano_synth_gates[128];
static std::atomic<unsigned> piano_synth_faults{0};
static std::atomic<unsigned> piano_synth_held_count{0};
extern "C" unsigned batch_piano_synth_faults(void){return piano_synth_faults.load(std::memory_order_acquire);}
extern "C" unsigned batch_piano_synth_held(void){return piano_synth_held_count.load(std::memory_order_acquire);}
#endif
static int piano_input_off(unsigned key,void *current_context){
 auto voice=piano_input_voices[key];
#ifdef MPC_PAD_MIDI_RECORDING_TRIAL
 /* Value-only release remains safe even when the old native context has
  * already changed. Never dereference its captured identities for this. */
 if(voice.synth&&voice.pitch<128&&piano_synth_gates[voice.pitch]){
  if(piano_synth_gates[voice.pitch]==1&&!am_synth_try_note(voice.pitch,0,0)){
   piano_synth_faults.fetch_add(1,std::memory_order_release);return 0;
  }
  --piano_synth_gates[voice.pitch];
 }
#endif
 piano_input_voices[key]={};
 /* The queued current context owns its native reference. Old voice pointers
  * are identities only: never dereference a context after it was destroyed. */
 if(!voice.context||voice.context!=current_context||event_context_program(current_context)!=voice.program)return 1;
 void *input=preview_input(current_context),*engine=input?F(input,0xd8,void*):nullptr;
 void *things=engine?F(engine,0x3c0,void*):nullptr;
 if(engine!=voice.engine||!things||F(things,0xb8,void*)!=voice.program||F(things,0x140,void*)!=voice.track)return 1;
 if(am_midi_native_key_release)am_midi_native_key_release(voice.context,key,voice.track);
 int last=1;
 for(const auto &other:piano_input_voices)
  if(other.context&&other.engine==voice.engine&&other.source==voice.source)last=0;
 poly_record_release_event(voice.engine,voice.track,voice.source,voice.choice,last);
 return 1;
}
static void run_piano_input(void *data)noexcept{
 void *reference=F(data,0,void*);auto *s=static_cast<PianoInputSlot*>(F(data,8,void*));
 try{
  if(s->key==16){for(unsigned key=0;key<16;++key)piano_input_off(key,s->context);}
  else if(!s->down){
#ifdef MPC_PAD_MIDI_RECORDING_TRIAL
   if((!s->synth&&!piano_input_voices[s->key].synth)||(s->synth&&piano_input_voices[s->key].synth
      &&piano_input_voices[s->key].context==s->context
      &&piano_input_voices[s->key].occurrence==s->occurrence))
#endif
    piano_input_off(s->key,s->context);
  }
  else if(s->epoch==piano_input_epoch.load(std::memory_order_acquire)&&event_context_program(s->context)==s->program){
#ifdef MPC_PAD_MIDI_RECORDING_TRIAL
   if(s->synth&&piano_input_voices[s->key].synth&&piano_input_voices[s->key].context==s->context
      &&s->occurrence<=piano_input_voices[s->key].occurrence){
    piano_synth_faults.fetch_add(1,std::memory_order_release);goto piano_input_done;
   }
#endif
   if(!piano_input_off(s->key,s->context)){
#ifdef MPC_PAD_MIDI_RECORDING_TRIAL
    piano_synth_faults.fetch_add(1,std::memory_order_release);
#endif
    goto piano_input_done;
   }
   void *engine=F(s->input,0xd8,void*),*things=engine?F(engine,0x3c0,void*):nullptr;
   void *track=things?F(things,0x140,void*):nullptr;
   unsigned midi=s->note.data[0x20];int source=F(s->program,0x14a8+midi*4,int);
   if(things&&track&&F(things,0xb8,void*)==s->program&&source>=0&&source<128&&F(s->program,0x12a8+source*4,int)==(int)midi){
#ifdef MPC_PAD_MIDI_RECORDING_TRIAL
    struct am_midi_event synth_event{};unsigned synth_pitch=0;
    if(s->synth){
     void *instrument=FN(0x286bbe0,void*(*)(void*,int))(s->program,source);
     struct step_event native_note;std::memcpy(&native_note,&s->note,sizeof native_note);
     if(!instrument||FN(0x2804760,int(*)(void*))(instrument)
        ||!am_pad_midi_note_read(&native_note,
             FN(0x27e4230,am_midi_choice_get),&synth_event,&synth_pitch)){
      piano_synth_faults.fetch_add(1,std::memory_order_release);goto piano_input_done;
     }
    }
#endif
    EventPreviewNote raw=s->note;raw.time=0;raw.type&=~0x300u;raw.data[0x20]=(unsigned char)source;
#ifdef MPC_PAD_MIDI_RECORDING_TRIAL
    struct SynthStart {
     unsigned pitch;bool active;
     ~SynthStart(){if(active&&!am_synth_try_note(pitch,0,0))am_synth_force_note_off(pitch);}
    } started{synth_pitch,false};
    if(s->synth&&!piano_synth_gates[synth_pitch]){
     if(!am_synth_try_note(synth_pitch,synth_event.velocity,1)){
      piano_synth_faults.fetch_add(1,std::memory_order_release);goto piano_input_done;
     }
     started.active=true;
    }
#endif
    struct KeyScope {
     ~KeyScope(){if(am_midi_native_key_end)am_midi_native_key_end();}
    } key_scope;
    if(am_midi_native_key_begin)am_midi_native_key_begin(s->context,s->key,!!F(&raw,0x14,uint16_t));
    FN(0x211b090,void(*)(void*,int64_t,const void*,unsigned))(engine,0,&raw,0x7ffffffa);
    unsigned kind=F(&raw,0x14,uint16_t)&0x8000u?15u:0u;
    unsigned choice=raw.data[6+kind]|(kind<<8)|(F(&raw,0x14,uint16_t)?0x1000u:0);
    PianoInputVoice voice{};voice.context=s->context;voice.program=s->program;voice.engine=engine;voice.track=track;voice.source=(unsigned)source;voice.choice=choice;
#ifdef MPC_PAD_MIDI_RECORDING_TRIAL
    if(s->synth){
     voice.synth=1;voice.pitch=synth_pitch;voice.occurrence=s->occurrence;
     ++piano_synth_gates[synth_pitch];started.active=false;
    }
#endif
    piano_input_voices[s->key]=voice;
   }
#ifdef MPC_PAD_MIDI_RECORDING_TRIAL
   else if(s->synth)piano_synth_faults.fetch_add(1,std::memory_order_release);
#endif
  }
#ifdef MPC_PAD_MIDI_RECORDING_TRIAL
  else if(s->synth&&s->epoch==piano_input_epoch.load(std::memory_order_acquire))piano_synth_faults.fetch_add(1,std::memory_order_release);
#endif
 }catch(...){
#ifdef MPC_PAD_MIDI_RECORDING_TRIAL
  if(s->synth)piano_synth_faults.fetch_add(1,std::memory_order_release);
#endif
  std::fprintf(stderr,"PIANO_INPUT native input failed\n");}
piano_input_done:
#ifdef MPC_PAD_MIDI_RECORDING_TRIAL
 unsigned held=0;for(const auto &voice:piano_input_voices)held+=voice.synth!=0;
 piano_synth_held_count.store(held,std::memory_order_release);
#endif
 s->used.store(0,std::memory_order_release);
 __atomic_fetch_sub((unsigned*)((char*)reference+8),1,__ATOMIC_RELEASE);
}
static int queue_piano_input(void*c,void*input,void*program,const EventPreviewNote*note,unsigned key,uint64_t epoch
#ifdef MPC_PAD_MIDI_RECORDING_TRIAL
                            ,unsigned synth=0,uint64_t occurrence=0
#endif
){
 PianoInputSlot *s=nullptr;unsigned free=0;
 for(auto &slot:piano_input_slots)free+=!slot.used.load(std::memory_order_acquire);
 if(note&&free<=16)return 0; /* Reserve room for matching releases / Stop. */
 for(auto &slot:piano_input_slots){unsigned expected=0;if(slot.used.compare_exchange_strong(expected,1,std::memory_order_acq_rel)){s=&slot;break;}}
 if(!s)return 0;
 s->context=c;s->input=input;s->program=program;s->key=key;s->down=note!=nullptr;s->epoch=epoch;if(note)s->note=*note;
#ifdef MPC_PAD_MIDI_RECORDING_TRIAL
 s->synth=synth;s->occurrence=occurrence;
#endif
 void *owner=F(c,0x48,void*),*reference=(char*)c+0x48;
 Action action={(uintptr_t)run_piano_input,base+0x1517b30,reference,s,0};
 __atomic_fetch_add((unsigned*)((char*)reference+8),1,__ATOMIC_ACQUIRE);
 void *mutex=(char*)owner+0x220;FN(0xf40e50,void(*)(void*))(mutex);
 int accepted=FN(0x2b59300,int(*)(void*,void*,uintptr_t))((char*)owner+0x1d8,&action,(uintptr_t)reference&~(uintptr_t)3)&255;
 FN(0xf40e90,void(*)(void*))(mutex);((int(*)(int,void*,void*))action.manage)(1,&action.reference,nullptr);
 if(!accepted){__atomic_fetch_sub((unsigned*)((char*)reference+8),1,__ATOMIC_RELEASE);s->used.store(0,std::memory_order_release);}
 return accepted;
}
extern "C" int batch_piano_input(uintptr_t b,void*c,void*program,const void*event,unsigned key){
 base=b;if(!poly_record_release_event||!c||!F(c,0x48,void*)||key>=16)return 0;
 if(!event)return queue_piano_input(c,nullptr,nullptr,nullptr,key,piano_input_epoch.load(std::memory_order_acquire));
 auto *n=static_cast<const EventPreviewNote*>(event);float velocity=F(n,0x10,float);
 if(!program||event_context_program(c)!=program||(n->type&255)!=3||n->data[0x20]>=128||!std::isfinite(velocity)||velocity<=0||velocity>1.f)return 0;
 void *input=preview_input(c);if(!input)return 0;
 return queue_piano_input(c,input,program,n,key,piano_input_epoch.load(std::memory_order_acquire));
}
extern "C" int batch_piano_input_stop(uintptr_t b,void*c){
 base=b;uint64_t epoch=piano_input_epoch.fetch_add(1,std::memory_order_acq_rel)+1;
 if(!poly_record_release_event||!c||!F(c,0x48,void*))return 0;
 return queue_piano_input(c,nullptr,nullptr,nullptr,16,epoch);
}
#ifdef MPC_PAD_MIDI_RECORDING_TRIAL
extern "C" int batch_piano_synth_input(uintptr_t b,void*c,void*program,const void*event,unsigned key,uint64_t occurrence){
 base=b;if(!base||!poly_record_release_event||!c||!F(c,0x48,void*)||key>=16||!occurrence)return 0;
 if(!event)return queue_piano_input(c,nullptr,nullptr,nullptr,key,piano_input_epoch.load(std::memory_order_acquire),1,occurrence);
 if(!program||event_context_program(c)!=program)return 0;
 struct am_midi_event value{};unsigned pitch;
 if(!am_pad_midi_note_read(static_cast<const step_event*>(event),FN(0x27e4230,am_midi_choice_get),&value,&pitch)
    ||value.tick||value.duration)return 0;
 void *input=preview_input(c);if(!input)return 0;
 return queue_piano_input(c,input,program,static_cast<const EventPreviewNote*>(event),key,piano_input_epoch.load(std::memory_order_acquire),1,occurrence);
}
#endif
