#define main unused_fx_probe
#include "check_fx_native.c"
#undef main
#include "src/midi_native.c"
static unsigned char track[0x3000],program[0x16a8],engine[96],clock_data[32],filter_model[0x400];
static uintptr_t backend[4],vtable[4];
static unsigned char messages[64][32];static unsigned queued,consumed,wire_count;
static struct am_midi_packet wire_log[64];static struct am_midi_voices voices;
static int accept(void *u,const struct am_midi_packet *p){(void)u;assert(wire_count<64);wire_log[wire_count++]=*p;return 1;}
static void send_sink(void *owner,void *message){
 (void)owner;struct am_midi_wire w;assert(am_midi_wire_current(&w));
 for(unsigned i=3;i<8;++i)assert(!((unsigned char*)message)[i]);
 struct am_midi_message m={3,{0}};memcpy(m.data,message,3);
 assert(am_midi_wire_send(&voices,&w,AM_MIDI_USB,&m,accept,NULL)==1);
}
static void queue_sink(void *manager,int route,const void *record){
 assert(manager==(void*)0x12345000&&route==321&&queued<64);
 /* Execute native JUCE copy, including the inline union and timestamp. */
 FN(0xddec50,void(*)(void*,const void*))(messages[queued],record);
 memcpy(messages[queued]+24,(char*)record+24,8);
 struct am_midi_wire w;assert(am_midi_wire_read(messages[queued],&w,NULL));++queued;
}
static void port_sink(void *owner,const void *record){(void)owner;struct am_midi_wire w;assert(am_midi_wire_read(record,&w,NULL));live_send(NULL,(void*)record);}
static void filter_route_sink(void *owner,void *message,int route){
 assert(owner==filter_model&&route==321);struct am_midi_wire w;assert(am_midi_wire_current(&w));
 _Alignas(8) unsigned char record[32]={0};memcpy(record,message,24);
 FN(0x1e39910,void(*)(void*,const void*))((void*)0x12345000,record);
}
static void flush(void){
 while(consumed<queued){
  struct {void *data;int capacity,used;} buffer={0};
  FN(0xddd040,void(*)(void*,const void*,int))(&buffer,messages[consumed],96);
  assert(buffer.used==6+AM_MIDI_WIRE_BYTES);uint16_t size;memcpy(&size,(char*)buffer.data+4,2);assert(size==AM_MIDI_WIRE_BYTES);
  FN(0x1e620b0,void(*)(void*,void*,int))(filter_model,&buffer,321);free(buffer.data);
  FN(0xddf190,void(*)(void*))(messages[consumed++]);
 }
}
static void accepted_event(void *t,uint64_t offset,const struct step_event *source,void *output){
 (void)output;assert(t==track);unsigned char command[44]={0};command[0]=source->data[32];
 unsigned channel=2;memcpy(command+4,&channel,4);memcpy(command+8,source->data,4);memcpy(command+12,source->data+4,18);
 FN(0x28a8d00,void(*)(void*,uint64_t,int,const void*))(backend,offset,321,command);
}
static void release_sink(void *e,uint64_t offset,void *t,unsigned note,unsigned tag,unsigned channel,unsigned kind,unsigned scheduled){
 (void)e;(void)t;(void)tag;(void)kind;(void)scheduled;
 FN(0x28a8eb0,void(*)(void*,uint64_t,int,unsigned,unsigned))(backend,offset,321,channel,note);
}
int main(int argc,char **argv){
 assert(argc==2);struct sigaction sa={.sa_sigaction=fault,.sa_flags=SA_SIGINFO};sigaction(SIGSEGV,&sa,NULL);sigaction(SIGILL,&sa,NULL);sigaction(SIGABRT,&sa,NULL);
 load(argv[1]);phase="native two-layer wire integration";assert(am_midi_native_install(base));
 native_queue=queue_sink;native_send=send_sink;original=accepted_event;native_release=release_sink;native_port_queue=port_sink;
 memset(filter_model,255,sizeof filter_model);uint32_t branch[2]={0x58000050,0xd61f0200};uintptr_t route_callback=(uintptr_t)filter_route_sink;memcpy((void*)(base+0x1e61db0),branch,8);memcpy((void*)(base+0x1e61db8),&route_callback,8);__builtin___clear_cache((char*)base+0x1e61db0,(char*)base+0x1e61dc0);
 vtable[3]=base+0x28a8020;backend[0]=(uintptr_t)vtable;backend[1]=(uintptr_t)engine;backend[2]=(uintptr_t)clock_data;backend[3]=0x12345000;
 float rate=48000;memcpy(engine+80,&rate,4);uint64_t origin=1234000;memcpy(clock_data+16,&origin,8);
 void *p=program;memcpy(track+0xbb8,&p,8);int sound=14,note=50;memcpy(program+0x12a8+sound*4,&note,4);memcpy(program+0x14a8+note*4,&sound,4);
 struct am_midi_map map={.out_kind=AM_MIDI_NOTE,.out_channel=3,.out_number=67};assert(am_midi_map_set(14,&map));
 struct step_event (*make)(unsigned,int64_t,int64_t,unsigned,unsigned,float)=FN(0x27e54d0,struct step_event(*)(unsigned,int64_t,int64_t,unsigned,unsigned,float));
 void (*set)(void*,int,int,int)=FN(0x27e4400,void(*)(void*,int,int,int));
 void (*dispatch)(void*,uint64_t,const struct step_event*,void*)=FN(0x29caa30,void(*)(void*,uint64_t,const struct step_event*,void*));
 struct step_event a=make(50,960,240,100,2,.8f),b=make(50,1024,120,100,2,.6f);
 set(a.data+4,0,0,-12);set(a.data+4,15,0,3);set(b.data+4,0,0,2);
 struct step_event original_a=a;dispatch(track,96,&a,NULL);dispatch(track,112,&b,NULL);assert(!memcmp(&original_a,&a,sizeof a));
 assert(queued==2);unsigned millis,offset;memcpy(&millis,messages[0]+24,4);memcpy(&offset,messages[0]+28,4);assert(millis==1234002&&offset==96);
 flush();assert(wire_count==2&&wire_log[0].message.data[0]==0x92&&wire_log[0].message.data[1]==57&&wire_log[1].message.data[1]==69);
 map.out_number=90;map.out_channel=7;assert(am_midi_map_set(14,&map));
 /* Physical Tune key release precedes the aggregate native source release. */
 unsigned choice=0x1000u|b.data[6];am_midi_native_release_choice(track,14,choice);flush();
 assert(wire_count==3&&wire_log[2].message.data[0]==0x82&&wire_log[2].message.data[1]==69);
 am_midi_native_release_choice(track,14,0x1f00u|a.data[21]);flush();
 /* A contains Tune+Slice, so its typed mask is not a single Slice modifier.
  * It is deliberately left for the scheduler's exact full-event release. */
 assert(wire_count==3);
 unsigned char scheduler[0x800]={0},entry[96]={0};memcpy(entry,&a,sizeof a);
 uint64_t end=1200;memcpy(entry+56,&end,8);entry[64]=50;unsigned ch=2,kind=3;memcpy(entry+72,&kind,4);memcpy(entry+76,&ch,4);void *t=track;memcpy(entry+80,&t,8);entry[88]=1;
 FN(0x212bc10,void(*)(void*,const void*))(scheduler+0x6c8,entry);
 FN(0x21185a0,void(*)(void*,uint64_t,uint64_t))(scheduler,1200,128);flush();
 assert(wire_count==4&&wire_log[3].message.data[0]==0x82&&wire_log[3].message.data[1]==57);
 FN(0x2118460,void(*)(void*,uint64_t,void*,unsigned,unsigned,unsigned,unsigned,unsigned))(scheduler,128,track,50,0,2,3,0);flush();assert(wire_count==4);

 /* Plain pad and Chop 1 have the same pitch offset, but different layers. */
 struct am_midi_map levels={.out_kind=AM_MIDI_NOTE,.out_number=72,.out_channel=6};assert(am_midi_layer_set(14,&levels,1));
 struct step_event plain=make(50,2000,480,100,2,.8f),chop=plain;set(chop.data+4,15,0,1);
 unsigned initial=wire_count;am_midi_native_key_begin((void*)0x4560,0,0);dispatch(track,0,&plain,NULL);am_midi_native_key_end();
 am_midi_native_key_begin((void*)0x4560,1,1);dispatch(track,0,&chop,NULL);am_midi_native_key_end();flush();
 assert(wire_count==initial+2&&wire_log[initial].message.data[1]==90&&wire_log[initial].message.data[0]==0x96&&wire_log[initial+1].message.data[1]==72&&wire_log[initial+1].message.data[0]==0x95);
 am_midi_native_key_release((void*)0x4560,1,track);flush();assert(wire_count==initial+3&&wire_log[initial+2].message.data[1]==72&&wire_log[initial+2].message.data[0]==0x85);
 am_midi_native_key_release((void*)0x4560,0,track);flush();assert(wire_count==initial+4&&wire_log[initial+3].message.data[1]==90&&wire_log[initial+3].message.data[0]==0x86);
 FN(0x2118460,void(*)(void*,uint64_t,void*,unsigned,unsigned,unsigned,unsigned,unsigned))(scheduler,128,track,50,0,2,3,0);flush();assert(wire_count==initial+4);
 /* Stop may call the backend Off directly, without source-release dispatch.
  * Retire the occurrence as soon as all captured backend routes are closed. */
 dispatch(track,0,&plain,NULL);flush();unsigned stop_count=wire_count;
 FN(0x28a8eb0,void(*)(void*,uint64_t,int,unsigned,unsigned))(backend,0,321,2,50);flush();
 assert(wire_count==stop_count+1);
 for(unsigned i=0;i<LIVE_COUNT;++i)assert(!live[i].used||live[i].released);
 unsigned blocked_count=wire_count;filter_model[0x1b]=0;dispatch(track,0,&plain,NULL);flush();assert(wire_count==blocked_count);filter_model[0x1b]=255;
 void *storage;memcpy(&storage,scheduler+0x6c8,8);free(storage);
 puts("PASS installed native live MIDI: source Sound, Tune+Chop, native queue copy/timing, independent physical release, exact scheduled release, captured map/channel and no duplicate aggregate Off");return 0;
}
