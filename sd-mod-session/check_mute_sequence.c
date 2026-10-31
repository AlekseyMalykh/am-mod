/* Real sampler/sequence routes: r14f gate alone must reproduce the bypass. */
#define main original_steps_main
#include "check_steps.c"
#undef main
static unsigned char routed_engine[0x800],routed_track[0xc00],routed_clock[0x100];static uintptr_t route_vtable[3];
static unsigned played,sampler,buffered,scheduled;
static int yes(void*p,int a,int b){(void)p;(void)a;(void)b;return 1;}
static int no(void*p,int a,int b){(void)p;(void)a;(void)b;return 0;}
static int no_transform(void*p){(void)p;return 0;}
static unsigned random_zero(void){return 0;}
static int64_t block_time(void*p){(void)p;return 0;}
static void track_event(void*p,int64_t when,const struct step_event*e){(void)when;assert(p==routed_track&&(e->type&255)<=4);}
static void engine_event(void*p,uintptr_t id,int n,void*t,void*pad,void*value,float vel){
 (void)pad;(void)value;assert(p==routed_engine+0xe8&&id==0x18&&n==1&&*(void**)t==routed_track&&vel>=0);++played;}
static void sampler_event(void*p,int64_t when,void*t,int pad,unsigned mask,unsigned flags,int source,int ratchet){
 (void)pad;(void)mask;(void)flags;(void)source;(void)ratchet;assert(p==routed_engine&&!when&&t==routed_track);++sampler;}
static void buffer_event(void*p,int64_t a,void*t,const struct step_event*e){(void)p;(void)a;(void)t;assert(musical(e));++buffered;}
static void schedule_off(void*p,const void*e){assert(p==routed_engine+0x6c8&&e);++scheduled;}
static void patch_file(uintptr_t off,const char*path){FILE*f=fopen(path,"rb");assert(f);assert(fread((void*)(base+off),1,4,f)==4);fclose(f);}
typedef void(*Route)(void*,int64_t,void*,const struct step_event*,int,unsigned);
typedef void(*Input)(void*,int64_t,int64_t,void*,const struct step_event*);
int main(int argc,char**argv){assert(argc==2);load_elf(argv[1],0x1800000000ULL);
 struct sigaction a={0};a.sa_sigaction=fault;a.sa_flags=SA_SIGINFO;sigaction(SIGSEGV,&a,NULL);
 phase="native sampler mute route bypass";stub(0x282fe30,yes);stub(0x282fec0,no);stub(0x3b89890,random_zero);
 stub(0x2142f00,block_time);
 stub(0x29cb250,track_event);stub(0x212cba0,engine_event);stub(0x2118460,sampler_event);stub(0x21560f0,buffer_event);
 route_vtable[2]=(uintptr_t)no_transform;FIELD(routed_track,0x890,void*)=route_vtable;
 FIELD(routed_engine,0x788,void*)=routed_clock;stub(0x212bc10,schedule_off);
 FILE*f=fopen("build/phone_bridges.bin","rb");assert(f);size_t size=fread((void*)(base+0x6d3d000),1,PHONE_TABLE_VA-0x6d3d000,f);fclose(f);assert(size>4096);
 FIELD((void*)base,PHONE_TABLE_VA+64+H_PREVIEW_EVENT_GATE*16+8,uintptr_t)=(uintptr_t)step_native_preview_suppressed;
 patch_file(0x213ba04,"autorun/phone-muted-note-gate.bin");__builtin___clear_cache((char*)base,(char*)(base+0x7500000));
 struct step_event note=make_note(4,0,.8f),muted=note;muted.type|=0x200;
 Route route=FN(0x2119d00,Route);Input input=FN(0x211a230,Input);
 route(routed_engine,0,routed_track,&muted,3,0x7ffffffe);assert(played==1&&sampler+scheduled>0); /* r14f misses this */
 input(routed_engine,0,960,routed_track,&muted);assert(played==2&&buffered==1);
 unsigned base_played=played,base_sampler=sampler,base_scheduled=scheduled,base_buffered=buffered;
 struct step_event off=muted;off.type|=0x100;
 played=sampler=scheduled=buffered=0;
 route(routed_engine,0,routed_track,&off,3,0x7ffffffe);input(routed_engine,0,960,routed_track,&off);
 unsigned off_played=played,off_sampler=sampler,off_scheduled=scheduled,off_buffered=buffered;
 patch_file(0x2119d04,"autorun/phone-muted-sequence-gate.bin");patch_file(0x211a234,"autorun/phone-muted-sequence_input-gate.bin");
 __builtin___clear_cache((char*)base,(char*)(base+0x7500000));
 played=sampler=scheduled=buffered=0;route(routed_engine,0,routed_track,&muted,3,0x7ffffffe);input(routed_engine,0,960,routed_track,&muted);
 assert(!played&&!sampler&&!scheduled&&!buffered);
 route(routed_engine,0,routed_track,&note,3,0x7ffffffe);input(routed_engine,0,960,routed_track,&note);
 assert(played==base_played&&sampler==base_sampler&&scheduled==base_scheduled&&buffered==base_buffered);
 played=sampler=scheduled=buffered=0;route(routed_engine,0,routed_track,&off,3,0x7ffffffe);input(routed_engine,0,960,routed_track,&off);
 assert(played==off_played&&sampler==off_sampler&&scheduled==off_scheduled&&buffered==off_buffered);assert(muted.type==(note.type|0x200));
 phase="temporary editor suppression at actual sequence boundaries";
 FIELD(routed_engine,0x3c0,void*)=things;FIELD(things,0x140,void*)=routed_track;FIELD(things,0x368,void*)=(void*)1;
 assert(FN(0x2114fc0,int(*)(void*,int,unsigned,void*))(routed_engine,38,0x7ffffffa,NULL)==38);
 preview_gate_set(routed_track,&note);played=sampler=scheduled=buffered=0;
 route(routed_engine,0,routed_track,&note,3,0x7ffffffe);input(routed_engine,0,960,routed_track,&note);
 assert(!played&&!sampler&&!scheduled&&!buffered);
 struct step_event different=note;event_choice_set(&different,0,5);
 route(routed_engine,0,routed_track,&different,3,0x7ffffffe);assert(played==1);
 route(routed_engine,0,routed_track,&note,0,0x7ffffffa);assert(played==2); /* Native pad audition, including original. */
 assert(!step_native_preview_suppressed((void*)1,&note,0x7ffffffe));
 uint64_t gate_serial=atomic_load(&preview_gate_serial);atomic_store(&preview_gate_serial,gate_serial+1);
 assert(!step_native_preview_suppressed(routed_track,&note,0x7ffffffe)); /* bounded fail-open during UI publish */
 atomic_store(&preview_gate_serial,gate_serial+2);
 preview_gate_set(NULL,NULL);route(routed_engine,0,routed_track,&note,3,0x7ffffffe);assert(played==3);
 puts("PASS actual sequence gate suppresses only staged original, other notes and native Sample audition pass; Cancel restores playback; no event list mutation");
 puts("PASS actual sampler2119d00 and sequence-input211a230 reproduce r14f mute bypass; shipped dual gate blocks muted note-on before sampler, event bus and queue while normal notes and NoteOff pass; final sound/buffer receivers are fixtures");
}
