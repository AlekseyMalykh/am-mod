/* Musical input for Piano Roll. Native Engine/recorder owns timing, duration,
 * quantization and history. UI never edits a recording clip directly. */
extern "C" void poly_record_release_event(void*,void*,unsigned,unsigned,int) __attribute__((weak));
extern "C" void am_midi_native_key_begin(void*,unsigned,unsigned) __attribute__((weak));
extern "C" void am_midi_native_key_end(void) __attribute__((weak));
extern "C" void am_midi_native_key_release(void*,unsigned,void*) __attribute__((weak));
struct PianoInputSlot {
 std::atomic<unsigned> used{0};void *context,*input,*program;
 EventPreviewNote note;unsigned key,down;uint64_t epoch;
};
struct PianoInputVoice {void *context,*program,*engine,*track;unsigned source,choice;};
static PianoInputSlot piano_input_slots[64];
static PianoInputVoice piano_input_voices[16]{}; /* Native worker only. */
static std::atomic<uint64_t> piano_input_epoch{1};
static void piano_input_off(unsigned key,void *current_context){
 auto voice=piano_input_voices[key];piano_input_voices[key]={};
 /* The queued current context owns its native reference. Old voice pointers
  * are identities only: never dereference a context after it was destroyed. */
 if(!voice.context||voice.context!=current_context||event_context_program(current_context)!=voice.program)return;
 void *input=preview_input(current_context),*engine=input?F(input,0xd8,void*):nullptr;
 void *things=engine?F(engine,0x3c0,void*):nullptr;
 if(engine!=voice.engine||!things||F(things,0xb8,void*)!=voice.program||F(things,0x140,void*)!=voice.track)return;
 if(am_midi_native_key_release)am_midi_native_key_release(voice.context,key,voice.track);
 int last=1;
 for(const auto &other:piano_input_voices)
  if(other.context&&other.engine==voice.engine&&other.source==voice.source)last=0;
 poly_record_release_event(voice.engine,voice.track,voice.source,voice.choice,last);
}
static void run_piano_input(void *data)noexcept{
 void *reference=F(data,0,void*);auto *s=static_cast<PianoInputSlot*>(F(data,8,void*));
 try{
  if(s->key==16){for(unsigned key=0;key<16;++key)piano_input_off(key,s->context);}
  else if(!s->down)piano_input_off(s->key,s->context);
  else if(s->epoch==piano_input_epoch.load(std::memory_order_acquire)&&event_context_program(s->context)==s->program){
   piano_input_off(s->key,s->context);
   void *engine=F(s->input,0xd8,void*),*things=engine?F(engine,0x3c0,void*):nullptr;
   void *track=things?F(things,0x140,void*):nullptr;
   unsigned midi=s->note.data[0x20];int source=F(s->program,0x14a8+midi*4,int);
   if(things&&track&&F(things,0xb8,void*)==s->program&&source>=0&&source<128&&F(s->program,0x12a8+source*4,int)==(int)midi){
    EventPreviewNote raw=s->note;raw.time=0;raw.type&=~0x300u;raw.data[0x20]=(unsigned char)source;
    if(am_midi_native_key_begin)am_midi_native_key_begin(s->context,s->key,!!F(&raw,0x14,uint16_t));
    FN(0x211b090,void(*)(void*,int64_t,const void*,unsigned))(engine,0,&raw,0x7ffffffa);
    if(am_midi_native_key_end)am_midi_native_key_end();
    unsigned kind=F(&raw,0x14,uint16_t)&0x8000u?15u:0u;
    unsigned choice=raw.data[6+kind]|(kind<<8)|(F(&raw,0x14,uint16_t)?0x1000u:0);
    piano_input_voices[s->key]={s->context,s->program,engine,track,(unsigned)source,choice};
   }
  }
 }catch(...){std::fprintf(stderr,"PIANO_INPUT native input failed\n");}
 s->used.store(0,std::memory_order_release);
 __atomic_fetch_sub((unsigned*)((char*)reference+8),1,__ATOMIC_RELEASE);
}
static int queue_piano_input(void*c,void*input,void*program,const EventPreviewNote*note,unsigned key,uint64_t epoch){
 PianoInputSlot *s=nullptr;unsigned free=0;
 for(auto &slot:piano_input_slots)free+=!slot.used.load(std::memory_order_acquire);
 if(note&&free<=16)return 0; /* Reserve room for matching releases / Stop. */
 for(auto &slot:piano_input_slots){unsigned expected=0;if(slot.used.compare_exchange_strong(expected,1,std::memory_order_acq_rel)){s=&slot;break;}}
 if(!s)return 0;
 s->context=c;s->input=input;s->program=program;s->key=key;s->down=note!=nullptr;s->epoch=epoch;if(note)s->note=*note;
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
