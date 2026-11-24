#define main unused_fx_probe
#include "check_fx_native.c"
#undef main
#include "src/midi_native.c"
/* Project metadata and engine services are fixtures. Track command building,
 * both stock filters, accepted virtual call and native due heap are real ELF. */
static uint64_t project_generation=1;
static void*no_optional_program_extension(void*p,const void*from,const void*to,int64_t hint){(void)p;(void)from;(void)to;(void)hint;return NULL;}
void*check_native_symbol(const char*name,void*fn){return !strcmp(name,"__dynamic_cast")?(void*)no_optional_program_extension:fn;}
uint64_t am_synth_archive_generation(void){return project_generation;}
int am_synth_archive_loading(void){return 0;}
static _Alignas(8) unsigned char track_data[0x3000],program_data[0x3000],instrument_data[16];
static uintptr_t backend_vtable[4];static unsigned backend_count,occupied,change_on_notification;
static uint64_t backend_offset;static unsigned drain_tail_count,drain_filter_count,position_count,drain_gate_open;
static unsigned midi_layer,midi_count;static uintptr_t midi_backend[4],midi_vtable[4];
static unsigned char midi_clock_engine[96],midi_clock[32];
static struct {struct am_midi_wire wire;struct am_midi_message message;} midi_log[8];
static void midi_queue_sink(void*manager,int route,const void*record){
 assert(manager==(void*)0x12345000&&route==321&&midi_count<8);
 assert(am_midi_wire_read(record,&midi_log[midi_count].wire,&midi_log[midi_count].message));++midi_count;
}
static void*drain_nested_engine;static am_synth_drain_fn real_drain;
static void nested_drain(void*engine,uint64_t offset,unsigned flags){
 struct am_synth_drain_scope*outer=synth_drain;assert(outer&&outer->engine==engine);
 native_synth_drain=real_drain;synth_native_drain(drain_nested_engine,offset,flags);assert(synth_drain==outer);
 real_drain(engine,offset,flags);native_synth_drain=nested_drain;
}
void check_synth_mark_partial_drain(void*engine){assert(synth_drain&&synth_drain->engine==engine);synth_drain_filter(track_data+0x750,3,0);}
extern void check_synth_throw_drain(void*,uint64_t,unsigned);
extern void check_synth_catch_drain(am_synth_drain_fn,void*);
static void synth_fault(int signal,siginfo_t*info,void*context){
 ucontext_t*u=context;fprintf(stderr,"TRACE lr=%lx x0=%lx x1=%lx x2=%lx\n",(unsigned long)(u->uc_mcontext.regs[30]-base),(unsigned long)u->uc_mcontext.regs[0],(unsigned long)u->uc_mcontext.regs[1],(unsigned long)u->uc_mcontext.regs[2]);fault(signal,info,context);
}
static void backend_sink(void*owner,uint64_t offset,int route,void*out,const void*command){
 assert(owner==program_data+0xdf0&&route==321&&!out);assert(((const unsigned char*)command)[0]==50);
 backend_offset=offset;++backend_count;
 if(midi_layer)FN(0x28a8d00,void(*)(void*,uint64_t,int,const void*))(midi_backend,offset,route,command);
}
static void backend_off_sink(void*owner,uint64_t offset,int route,unsigned channel,unsigned note){
 assert(owner==program_data+0xdf0&&midi_layer);
 FN(0x28a8eb0,void(*)(void*,uint64_t,int,unsigned,unsigned))(midi_backend,offset,route,channel,note);
}
static void notification(void*owner,int changed){
 assert(owner==track_data+0x228&&changed==1);
 unsigned action=change_on_notification;change_on_notification=0;
 if(action==1)am_synth_route_stop();
 if(action==2){struct am_synth_binding b;assert(am_synth_binding_capture("native-synth-program",15,&b));++project_generation;assert(am_synth_route_publish(&b,project_generation));}
}
static void *instrument_get(void*p,int pad){assert(p==program_data&&pad==15);return instrument_data;}
static int sample_exists(void*p){assert(p==instrument_data);return occupied;}
static unsigned drain_gate(void*filter,unsigned kind,unsigned flags){(void)filter;(void)flags;assert(kind==3);++drain_filter_count;return drain_gate_open;}
static void drain_tail(void*owner,unsigned offset,unsigned flags){(void)owner;(void)offset;(void)flags;++drain_tail_count;}
static void position_notice(void*owner,uint64_t code,uint64_t count,const void*position){(void)owner;(void)position;assert(code==16&&count==1);}
static void position_sink(void*owner,unsigned offset,void*out,uint64_t tick){(void)owner;(void)offset;(void)out;(void)tick;++position_count;}
static void optional_release_bookkeeping(void){}
static void release_sink(void*engine,uint64_t offset,void*track,unsigned note,unsigned tag,unsigned channel,unsigned kind,unsigned scheduled){
 (void)engine;(void)offset;(void)track;(void)note;(void)tag;(void)channel;(void)kind;(void)scheduled;
}
static void patch(uintptr_t offset,void*target){
 uint32_t words[]={0x58000050,0xd61f0200};memcpy((void*)(base+offset),words,8);memcpy((void*)(base+offset+8),&target,8);
 __builtin___clear_cache((char*)base+offset,(char*)base+offset+16);
}
static void filter_init(unsigned char*filter){
 filter[0x98]=255;memset(filter+0x130,255,16);float high=1;memcpy(filter+0xe4,&high,4);
}
static unsigned count_synth(void){unsigned count=0;for(unsigned i=0;i<LIVE_COUNT;++i)count+=!!live[i].synth.occurrence;return count;}
static struct live_note *latest(void){struct live_note*n=NULL;for(unsigned i=0;i<LIVE_COUNT;++i)if(live[i].synth_handled&&(!n||live[i].id>n->id))n=live+i;assert(n);return n;}
static void render(void){float left[256]={0},right[256]={0};am_synth_render_add(left,right,256);}
static void push(void*scheduler,const struct step_event*event,uint64_t end,void*track){
 unsigned char entry[96]={0};memcpy(entry,event,sizeof *event);memcpy(entry+56,&end,8);entry[64]=50;
 unsigned channel=2,kind=3;memcpy(entry+72,&kind,4);memcpy(entry+76,&channel,4);memcpy(entry+80,&track,8);entry[88]=1;
 FN(0x212bc10,void(*)(void*,const void*))((char*)scheduler+0x6c8,entry);
}
static void due(void*scheduler,const struct step_event*event,uint64_t end){
 push(scheduler,event,end,track_data);
 FN(0x21185a0,void(*)(void*,uint64_t,uint64_t))(scheduler,end,96);
}
int main(int argc,char**argv){
 assert(argc==2);struct sigaction sa={.sa_sigaction=synth_fault,.sa_flags=SA_SIGINFO};sigaction(SIGSEGV,&sa,NULL);sigaction(SIGILL,&sa,NULL);sigaction(SIGABRT,&sa,NULL);
 load(argv[1]);phase="Synth native accepted command";
 patch(0x2768e10,notification);patch(0x286bbe0,instrument_get);patch(0x2804760,sample_exists);
 patch(0x282fe30,drain_gate);patch(0x20f9140,drain_tail);patch(0x212c440,position_notice);patch(0x2146140,position_sink);
 patch(0x2830410,optional_release_bookkeeping);patch(0x282e950,optional_release_bookkeeping);patch(0x212cba0,optional_release_bookkeeping);
 assert(am_midi_native_install(base));native_release=release_sink;
 void*p=program_data;memcpy(track_data+0xbb8,&p,8);const char*key="native-synth-program";memcpy(program_data+0xdc8,&key,8);
 int sound=15,note=50,route=321;memcpy(program_data+0x12a8+sound*4,&note,4);memcpy(program_data+0x14a8+note*4,&sound,4);memcpy(track_data+0x6f0,&route,4);
 float one=1;memcpy(track_data+0xb00,&one,4);memcpy(track_data+0x2088+note*4,&one,4);
 filter_init(track_data+0x2888);filter_init(program_data+0xdf0);
 backend_vtable[2]=(uintptr_t)backend_sink;backend_vtable[3]=(uintptr_t)backend_off_sink;uintptr_t table=(uintptr_t)backend_vtable;memcpy(program_data+0xdf0,&table,8);
 struct am_synth_binding binding;assert(am_synth_binding_capture(key,15,&binding));assert(am_synth_route_publish(&binding,project_generation));
 struct step_event event=FN(0x27e54d0,struct step_event(*)(unsigned,int64_t,int64_t,unsigned,unsigned,float))(50,960,240,100,2,.8f);
 FN(0x27e4400,void(*)(void*,int,int,int))(event.data+4,0,0,7);
 void(*dispatch)(void*,uint64_t,const struct step_event*,void*)=FN(0x29caa30,void(*)(void*,uint64_t,const struct step_event*,void*));
 unsigned char scheduler[0x800]={0};struct step_event before=event;
 phase="first stock Track dispatch";dispatch(track_data,96,&event,NULL);assert(count_synth()==1&&backend_count==1&&backend_offset==96&&synth_last_offset==96);
 assert(!memcmp(&before,&event,sizeof event));render();struct am_synth_state state;assert(am_synth_peek(&state)&&state.voices==1&&(state.notes[67/32]&(1u<<(67%32))));
 dispatch(track_data,112,&event,NULL);assert(count_synth()==2&&backend_count==2);uint64_t second=latest()->id;
 due(scheduler,&event,1200);assert(count_synth()==1&&latest()->id==second&&latest()->synth.occurrence);
 due(scheduler,&event,1440);assert(!count_synth());render();
 /* Real stock filters reject before the tap, without a second decision. */
 unsigned backend_before=backend_count;track_data[0x4f0]=1;dispatch(track_data,0,&event,NULL);track_data[0x4f0]=0;
 track_data[0x2888+0x98]=3;dispatch(track_data,0,&event,NULL);track_data[0x2888+0x98]=255;
 program_data[0xdf0+0x98]=3;dispatch(track_data,0,&event,NULL);program_data[0xdf0+0x98]=255;
 assert(!count_synth()&&backend_count==backend_before);
 occupied=1;dispatch(track_data,0,&event,NULL);occupied=0;assert(!count_synth()&&backend_count==backend_before+1);
 const char*other="other-program";memcpy(program_data+0xdc8,&other,8);dispatch(track_data,0,&event,NULL);memcpy(program_data+0xdc8,&key,8);assert(!count_synth());
 am_midi_native_key_begin((void*)0x1234,0,1);dispatch(track_data,0,&event,NULL);am_midi_native_key_end();assert(!count_synth());
 change_on_notification=1;dispatch(track_data,0,&event,NULL);assert(!count_synth()); /* Stop after live_start, before accepted tap */
 change_on_notification=2;dispatch(track_data,0,&event,NULL);assert(!count_synth()); /* source scope captured before Load */
 /* Early aggregate Off retains tombstones: their later scheduled Off must
  * never cut the next identical occurrence after reuse of a lower slot. */
 dispatch(track_data,0,&event,NULL);dispatch(track_data,0,&event,NULL);assert(count_synth()==2);
 live_release_note(scheduler,0,track_data,50,0,2,3,0);assert(!count_synth());
 dispatch(track_data,0,&event,NULL);struct live_note*newer=latest();uint64_t newer_id=newer->id;
 due(scheduler,&event,1680);assert(newer->id==newer_id&&newer->synth.occurrence);
 due(scheduler,&event,1920);assert(newer->synth.occurrence);due(scheduler,&event,2160);assert(!count_synth());
 /* Binding loss is not Stop: old heap entries remain ordered. */
 dispatch(track_data,0,&event,NULL);am_synth_route_clear();assert(am_synth_route_publish(&binding,project_generation));
 dispatch(track_data,0,&event,NULL);newer=latest();due(scheduler,&event,2400);assert(newer->synth.occurrence);due(scheduler,&event,2640);assert(!count_synth());
 /* A UI Stop request cannot prove that old native due entries disappeared. */
 phase="Stop request with delayed old due entry";
 dispatch(track_data,0,&event,NULL);push(scheduler,&event,2800,track_data);am_synth_route_stop();
 dispatch(track_data,0,&event,NULL);newer=latest();push(scheduler,&event,2900,track_data);
 FN(0x21185a0,void(*)(void*,uint64_t,uint64_t))(scheduler,2800,96);assert(newer->synth.occurrence);
 FN(0x21185a0,void(*)(void*,uint64_t,uint64_t))(scheduler,2900,96);assert(!count_synth());
 /* Actual native drain confirms heap cancellation, even when its native
  * release gate refuses the old note. A foreign Engine cannot cancel ours. */
 phase="native heap drain and full-pool recovery";
 unsigned char foreign[0x800]={0},foreign_track[0x3000]={0};dispatch(track_data,0,&event,NULL);
 push(foreign,&event,3000,foreign_track);unsigned tails=drain_tail_count;
 FN(0x2119550,void(*)(void*,uint64_t,unsigned))(foreign,96,0);assert(count_synth()==1&&drain_tail_count==tails+1&&!synth_drain);
 push(scheduler,&event,3000,track_data);FN(0x2119550,void(*)(void*,uint64_t,unsigned))(scheduler,96,0);assert(!count_synth()&&!synth_drain);
 for(unsigned i=0;i<LIVE_COUNT;++i){dispatch(track_data,0,&event,NULL);assert(latest()->synth_handled);push(scheduler,&event,4000+i,track_data);if(i%16==15)render();}
 am_synth_route_stop();dispatch(track_data,0,&event,NULL);assert(!count_synth()); /* fail closed until native confirms cancellation */
 FN(0x2119550,void(*)(void*,uint64_t,unsigned))(scheduler,96,0);
 for(unsigned i=0;i<LIVE_COUNT;++i)assert(!live[i].used);
 dispatch(track_data,0,&event,NULL);assert(count_synth()==1);due(scheduler,&event,4500);assert(!count_synth());
 /* Native position/queued callbacks use two OTHER due-release callsites.
  * Each must release one occurrence rather than aggregate every same note. */
 phase="native alternate due release paths";
 for(unsigned which=0;which<2;++which){
  dispatch(track_data,0,&event,NULL);dispatch(track_data,0,&event,NULL);assert(count_synth()==2);newer=latest();
  uint64_t tick=5000+which*1000;push(scheduler,&event,tick,track_data);push(scheduler,&event,tick+100,track_data);
  void(*position)(void*,uint64_t,uint64_t,void*)=FN(which?0x2118ed0:0x2118c20,void(*)(void*,uint64_t,uint64_t,void*));
  void*owner=which?(void*)(scheduler+0x98):(void*)scheduler;
  position(owner,96,tick,NULL);assert(newer->synth.occurrence&&count_synth()==1);
  position(owner,96,tick+100,NULL);assert(!count_synth());
 }
 assert(position_count==4&&drain_filter_count>=LIVE_COUNT+2);
 phase="interrupted and nested native drain scopes";
 dispatch(track_data,0,&event,NULL);push(scheduler,&event,7000,track_data);struct live_note*cancelled=latest();uint64_t cancelled_id=cancelled->id;
 real_drain=native_synth_drain;native_synth_drain=check_synth_throw_drain;
 check_synth_catch_drain(synth_native_drain,scheduler);native_synth_drain=real_drain;
 assert(!synth_drain&&cancelled->used&&cancelled->id==cancelled_id&&cancelled->synth_handled&&!cancelled->synth.occurrence);
 dispatch(track_data,0,&event,NULL);newer=latest();push(scheduler,&event,7100,track_data);
 FN(0x21185a0,void(*)(void*,uint64_t,uint64_t))(scheduler,7000,96);assert(newer->synth.occurrence);
 drain_nested_engine=foreign;native_synth_drain=nested_drain;synth_native_drain(scheduler,96,0);native_synth_drain=real_drain;
 assert(!count_synth()&&!synth_drain);
 phase="Synth plus native MIDI backend on confirmed drain";
 midi_vtable[3]=base+0x28a8020;midi_backend[0]=(uintptr_t)midi_vtable;midi_backend[1]=(uintptr_t)midi_clock_engine;midi_backend[2]=(uintptr_t)midi_clock;midi_backend[3]=0x12345000;
 float rate=48000;memcpy(midi_clock_engine+80,&rate,4);uint64_t origin=1234000;memcpy(midi_clock+16,&origin,8);
 void(*saved_queue)(void*,int,const void*)=native_queue;native_queue=midi_queue_sink;midi_layer=1;
 dispatch(track_data,96,&event,NULL);push(scheduler,&event,8000,track_data);
 dispatch(track_data,112,&event,NULL);push(scheduler,&event,8100,track_data);
 assert(count_synth()==2&&midi_count==2&&midi_log[0].wire.id!=midi_log[1].wire.id);
 drain_gate_open=1;synth_native_drain(scheduler,96,0);drain_gate_open=0;assert(!count_synth()&&midi_count==4);
 for(unsigned i=0;i<2;++i){
  assert((midi_log[i].message.data[0]&0xf0)==0x90&&(midi_log[i+2].message.data[0]&0xf0)==0x80);
  assert(midi_log[i].wire.id==midi_log[i+2].wire.id&&midi_log[i].message.data[1]==midi_log[i+2].message.data[1]);
 }
 midi_layer=0;native_queue=saved_queue;
 ++project_generation;assert(am_synth_route_publish(&binding,project_generation));dispatch(track_data,0,&event,NULL);assert(count_synth()==1);due(scheduler,&event,3120);
 /* Saturate queue after On: Off cannot hang; owned-only panic is independent
  * of native MIDI forwarding and a free command slot. */
 render();dispatch(track_data,0,&event,NULL);struct am_synth_handle h;while(am_synth_owned_on(60,80,&h)){}while(am_synth_try_note(60,0,0)){}
 uint64_t faults=synth_faults;due(scheduler,&event,3360);assert(synth_faults==faults+1&&!count_synth());
 render();for(unsigned i=0;i<300;++i)render();assert(!am_synth_peek(&state)||!state.voices);
 void*storage;memcpy(&storage,scheduler+0x6c8,8);free(storage);memcpy(&storage,foreign+0x6c8,8);free(storage);
 puts("PASS native Synth: stock Track/filters/virtual call, original backend, owned DSP, three exact due paths, delayed Off after Stop request, confirmed native heap drain, full-pool recovery, foreign Engine isolation, exception/nested scope, paired native MIDI backend releases, project generation, queue-full panic");return 0;
}
