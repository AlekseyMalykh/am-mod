/* Included by midi_native.c. Everything before the sender is owned by the
 * native music executor. Only the copied value envelope crosses threads. */
#define LIVE_COUNT 256u
struct live_route {void *owner;int route;unsigned note,channel,released;};
struct live_note {
 unsigned used,released,sound,layer,route_count;uint64_t id;uintptr_t track,key_owner;unsigned key;
 struct am_midi_event source;uint16_t mask;unsigned char choices[16];
 struct live_route routes[4];
#ifdef MPC_SYNTH_NATIVE_PLAYBACK_TRIAL
 struct am_synth_sequence_voice synth;uint64_t synth_stop,synth_generation;unsigned synth_handled;
#endif
};
static struct live_note live[LIVE_COUNT];
static _Thread_local struct live_note *live_event,*live_release;
static _Thread_local const struct step_event *live_scheduled;
static _Thread_local unsigned live_blocked,release_scope,release_note;
static _Thread_local uintptr_t release_track;
static _Thread_local struct {uintptr_t owner;unsigned key,layer;} live_key,live_key_stack[8];
static _Thread_local unsigned live_key_depth;
#ifdef MPC_SYNTH_NATIVE_PLAYBACK_TRIAL
static void synth_finish(struct live_note*);
static void synth_scope_tick(void);
static void *synth_instrument_on(void*,uint64_t,int,void*,const void*);
static void synth_instrument_end(void*,int);
#endif
void am_midi_native_key_begin(void *owner,unsigned key,unsigned layer){
 if(live_key_depth<8)live_key_stack[live_key_depth]=live_key;
 ++live_key_depth;live_key.owner=live_key_depth<=8&&key<16?(uintptr_t)owner:0;live_key.key=key;live_key.layer=layer;
}
void am_midi_native_key_end(void){
 if(live_key_depth){--live_key_depth;if(live_key_depth<8)live_key=live_key_stack[live_key_depth];}
}
static _Thread_local struct {struct live_note *note;struct am_midi_event accepted;void *owner;int route;unsigned off;} live_command;
static void (*native_on)(void*,uint64_t,int,const void*);
static void (*native_off)(void*,uint64_t,int,unsigned,unsigned);
static void (*native_pressure)(void*,uint64_t,int,unsigned,unsigned,float);
static void (*native_queue)(void*,int,const void*);
static void (*native_release)(void*,uint64_t,void*,unsigned,unsigned,unsigned,unsigned,unsigned);
static void (*native_send)(void*,void*);
static void (*native_filter)(void*,void*,int);
static void (*native_port_queue)(void*,const void*);
static struct live_note *live_start(const struct am_midi_context *context,const struct step_event *source){
#ifdef MPC_SYNTH_NATIVE_PLAYBACK_TRIAL
 synth_scope_tick();
#endif
 for(unsigned i=0;i<LIVE_COUNT;++i)if(!live[i].used||(live[i].released
#ifdef MPC_SYNTH_NATIVE_PLAYBACK_TRIAL
    &&!live[i].synth_handled
#endif
    )){
  struct live_note *n=live+i;
  memset(n,0,sizeof *n);n->used=1;n->track=context->track_id;
  n->source=context->event;n->sound=context->sound;
  memcpy(&n->mask,source->data+4,2);memcpy(n->choices,source->data+6,16);
  n->key_owner=live_key.owner;n->key=live_key.key;
#ifdef MPC_SYNTH_NATIVE_PLAYBACK_TRIAL
  n->synth_stop=am_synth_route_stop_serial();
  n->synth_generation=am_synth_archive_generation();
#endif
  n->layer=!!n->mask||(n->key_owner&&live_key.layer);return n;
 }
 return NULL;
}
static struct live_note *live_repeat(const struct live_note *source){
 for(unsigned i=0;i<LIVE_COUNT;++i)if(!live[i].used||(live[i].released
#ifdef MPC_SYNTH_NATIVE_PLAYBACK_TRIAL
    &&!live[i].synth_handled
#endif
    )){
  struct live_note *n=live+i;if(n==source)continue;
  *n=*source;n->id=0;n->released=0;n->route_count=0;memset(n->routes,0,sizeof n->routes);
#ifdef MPC_SYNTH_NATIVE_PLAYBACK_TRIAL
  memset(&n->synth,0,sizeof n->synth);n->synth_handled=0;
#endif
  return n;
 }
 return NULL;
}
static void live_forget(struct live_note *n){
 if(n)memset(n,0,sizeof *n);
}
static void live_queue(void *manager,int route,const void *record){
 struct live_note *n=live_command.note;
 unsigned char copy[32];memcpy(copy,record,32);
 if(!n){
  struct am_midi_wire wire;unsigned char payload[AM_MIDI_WIRE_BYTES];
  if(am_midi_wire_current(&wire))am_midi_wire_wrap(copy,payload,&wire);
  native_queue(manager,route,copy);return;
 }
 if(!live_command.off){
  if(!n->id)n->id=am_midi_wire_id();
  if(!n->id||n->route_count==4)return; /* Fail closed before any wire On. */
  n->routes[n->route_count++]=(struct live_route){live_command.owner,route,copy[1],copy[0]&15,0};
 }
 if(!n->id)return;
 const struct am_midi_event *e=&live_command.accepted;
 struct am_midi_wire wire={n->id,n->sound,n->layer,e->tune+(e->slice?(int)e->slice-1:0)};
 unsigned char payload[AM_MIDI_WIRE_BYTES];if(!am_midi_wire_wrap(copy,payload,&wire))return;
 native_queue(manager,route,copy);
}
static void live_on(void *owner,uint64_t offset,int route,const void *command){
 struct live_note *n=live_event;
 if(live_blocked)return;
 if(!n){native_on(owner,offset,route,command);return;}
 for(unsigned j=0;j<n->route_count;++j)if(n->routes[j].owner==owner&&n->routes[j].route==route){n=live_repeat(n);if(!n)return;break;}
 struct am_midi_event accepted;
 if(!am_midi_command_copy(command,&n->source,(am_midi_choice_get)(image+0x27e4230),&accepted))return;
 __typeof__(live_command) previous=live_command;
 live_command.note=n;live_command.accepted=accepted;live_command.owner=owner;live_command.route=route;live_command.off=0;
 native_on(owner,offset,route,command);live_command=previous;
}
static void live_off_route(struct live_note *n,struct live_route *r,uint64_t offset){
 if(n->released||r->released)return;
 __typeof__(live_command) previous=live_command;
 live_command.note=n;live_command.accepted=n->source;live_command.off=1;
 native_off(r->owner,offset,r->route,r->channel,r->note);live_command=previous;r->released=1;
}
static void live_finish(struct live_note *n,uint64_t offset){
 if(!n)return;
#ifdef MPC_SYNTH_NATIVE_PLAYBACK_TRIAL
 synth_finish(n);
#endif
 if(n->released)return;
 for(unsigned j=0;j<n->route_count;++j)live_off_route(n,n->routes+j,offset);
 n->released=1;
}
static void live_off(void *owner,uint64_t offset,int route,unsigned channel,unsigned note){
 unsigned owned=0;
 if(release_scope==2&&!live_release){
  for(unsigned i=0;i<LIVE_COUNT;++i)if(live[i].used&&live[i].track==release_track&&live[i].source.source_note==release_note)
   for(unsigned j=0;j<live[i].route_count;++j){struct live_route *r=live[i].routes+j;
    if(r->owner==owner&&r->route==route&&r->note==note&&r->channel==channel)return;
   }
 }
 for(unsigned i=0;i<LIVE_COUNT;++i){struct live_note *n=live+i;
  if(!n->used||(live_release&&n!=live_release)||(release_scope&&(n->track!=release_track||n->source.source_note!=release_note))||(release_scope==2&&!live_release))continue;
  for(unsigned j=0;j<n->route_count;++j){struct live_route *r=n->routes+j;
   if(r->owner==owner&&r->route==route&&r->note==note&&r->channel==channel){
    owned=1;live_off_route(n,r,offset);
    /* One native scheduled release owns one captured occurrence. Live
     * aggregate releases below finish every occurrence of the source. */
   }
  }
  if(n->route_count){
   unsigned pending=0;
   for(unsigned j=0;j<n->route_count;++j)pending|=!n->routes[j].released;
   if(!pending){
#ifdef MPC_SYNTH_NATIVE_PLAYBACK_TRIAL
    synth_finish(n);
#endif
    n->released=1;
   }
  }
 }
 if(!owned)native_off(owner,offset,route,channel,note);
}
static void live_pressure(void *owner,uint64_t offset,int route,unsigned channel,unsigned note,float value){
 unsigned owned=0;
 for(unsigned i=0;i<LIVE_COUNT;++i){struct live_note *n=live+i;if(!n->used||n->released)continue;
  for(unsigned j=0;j<n->route_count;++j){struct live_route *r=n->routes+j;
   if(r->owner==owner&&r->route==route&&r->channel==channel&&r->note==note){
    __typeof__(live_command) previous=live_command;live_command.note=n;live_command.accepted=n->source;live_command.off=1;
    native_pressure(owner,offset,route,channel,note,value);live_command=previous;owned=1;
   }
  }
 }
 if(!owned)native_pressure(owner,offset,route,channel,note,value);
}
static int live_same(const struct am_midi_event *a,const struct am_midi_event *b){
 return a->source_note==b->source_note&&a->tick==b->tick&&a->duration==b->duration&&a->tune==b->tune&&a->slice==b->slice;
}
static void live_release_note(void *engine,uint64_t offset,void *track,unsigned note,unsigned tag,unsigned channel,unsigned kind,unsigned scheduled){
 {static unsigned logged;if(logged<300&&am_midi_mode_active()){++logged;fprintf(stderr,"AM_MIDI RELEASE note=%u tag=%u ch=%u kind=%u sched=%u\n",note,tag,channel,kind,scheduled);}}
 if(engine)emu_engine=engine;
 emu_release(engine,track,note&255);
 struct live_note *previous=live_release,*match=NULL;
 unsigned old_scope=release_scope,old_note=release_note;uintptr_t old_track=release_track;
 release_scope=live_scheduled?2:1;release_track=(uintptr_t)track;release_note=note&255;
 if(live_scheduled){
  const struct step_event *entry=live_scheduled;
  struct am_midi_event value;
  if(entry&&am_midi_event_copy(entry,(am_midi_choice_get)(image+0x27e4230),AM_MIDI_EVENT_NOTE_ON,&value))
   for(unsigned i=0;i<LIVE_COUNT;++i)if(live[i].used&&live[i].track==(uintptr_t)track&&live_same(&live[i].source,&value)){
#ifdef MPC_SYNTH_NATIVE_PLAYBACK_TRIAL
    if(!match||live[i].id<match->id)match=live+i;
#else
    match=live+i;break;
#endif
   }
 }
 live_release=match;
 native_release(engine,offset,track,note,tag,channel,kind,scheduled);
 if(match){live_finish(match,offset);live_forget(match);}
 else if(!live_scheduled){
  for(unsigned i=0;i<LIVE_COUNT;++i)if(live[i].used&&live[i].track==(uintptr_t)track&&live[i].source.source_note==(note&255)){
   live_finish(live+i,offset);
#ifdef MPC_SYNTH_NATIVE_PLAYBACK_TRIAL
   if(!live[i].synth_handled)
#endif
    live_forget(live+i);
  }
 }
 live_release=previous;
 release_scope=old_scope;release_track=old_track;release_note=old_note;
}
/* x7 is NOT a scheduler flag: the native live-release path sets it too.
 * The exact due-heap callsite passes its real x8 entry as a ninth argument. */
static void live_scheduled_release(void *engine,uint64_t offset,void *track,unsigned note,unsigned tag,unsigned channel,unsigned kind,unsigned flags,const struct step_event *entry){
 const struct step_event *previous=live_scheduled;live_scheduled=entry;
 ((void(*)(void*,uint64_t,void*,unsigned,unsigned,unsigned,unsigned,unsigned))(image+0x2118460))(engine,offset,track,note,tag,channel,kind,flags);
 live_scheduled=previous;
}
#ifdef MPC_SYNTH_NATIVE_PLAYBACK_TRIAL
#include "synth_live.inc"
#endif
void am_midi_native_release_choice(void *track,unsigned source,unsigned choice){
 if(!image||!track||source>=128)return;
 const unsigned char *program;memcpy(&program,(char*)track+0xbb8,sizeof program);if(!program)return;
 int note;memcpy(&note,program+0x12a8+source*4,4);if(note<0||note>127)return;
 unsigned kind=(choice>>8)&15,mask=choice&0x1000u?1u<<kind:0;
 for(unsigned i=0;i<LIVE_COUNT;++i){struct live_note *n=live+i;
  if(n->used&&!n->released&&n->track==(uintptr_t)track&&n->source.source_note==note&&n->mask==mask&&(!mask||n->choices[kind]==(choice&255))){live_finish(n,1);return;}
 }
}
void am_midi_native_key_release(void *owner,unsigned key,void *track){
 if(!image||!owner||!track||key>=16)return;
 for(unsigned i=0;i<LIVE_COUNT;++i)if(live[i].used&&live[i].key_owner==(uintptr_t)owner&&live[i].key==key&&live[i].track==(uintptr_t)track)live_finish(live+i,1);
}
static void live_send(void *owner,void *message){
 struct am_midi_wire wire,previous;struct am_midi_message original;
 if(!am_midi_wire_read(message,&wire,&original)){native_send(owner,message);return;}
 _Alignas(8) unsigned char copy[24]={0};memcpy(copy,original.data,3);memcpy(copy+8,(char*)message+8,8);int size=3;memcpy(copy+16,&size,4);
 am_midi_wire_begin(&wire,&previous);native_send(owner,copy);am_midi_wire_end(&previous);
}
static void live_port_enqueue(void *owner,const void *record){
 struct am_midi_wire wire;
 if(!am_midi_wire_current(&wire)){native_port_queue(owner,record);return;}
 _Alignas(8) unsigned char copy[32];unsigned char payload[AM_MIDI_WIRE_BYTES];memcpy(copy,record,32);
 am_midi_wire_wrap(copy,payload,&wire);native_port_queue(owner,copy);
}
static void live_filter(void *owner,void *buffer,int route){
 /* The native filter ignores messages longer than three bytes. Preserve
  * its exact per-kind/channel policies by presenting each envelope as a
  * short one-event buffer, and reattach provenance at the output queue. */
 _Alignas(8) unsigned char iterator[16],message[24];
 ((void(*)(void*,void*))(image+0xddd210))(iterator,buffer);
 ((void(*)(void*))(image+0xddeb70))(message);
 int offset;
 while(((int(*)(void*,void*,int*))(image+0xddd2e0))(iterator,message,&offset)&255){
  struct am_midi_wire wire,previous;struct am_midi_message original;
  int wrapped=am_midi_wire_read(message,&wire,&original),native_size;memcpy(&native_size,message+16,4);
  if(!wrapped){if(native_size<1||native_size>3)continue;original.size=(uint8_t)native_size;memcpy(original.data,message,(unsigned)native_size);}
  unsigned char bytes[9];uint16_t size=original.size;memcpy(bytes,&offset,4);memcpy(bytes+4,&size,2);memcpy(bytes+6,original.data,size);
  struct {void *data;int capacity,used;} one={bytes,9,6+size};
  am_midi_wire_begin(wrapped?&wire:NULL,&previous);native_filter(owner,&one,route);am_midi_wire_end(&previous);
 }
 ((void(*)(void*))(image+0xddf190))(message);
 ((void(*)(void*))(image+0xddd220))(iterator);
}
