/* Native Seq export loop and Seq input execute; DSP/writer/services are fixtures.
 * This validates routing, durations and isolation, not device audio output. */
#define main unused_fx_main
#include "check_fx_native.c"
#undef main
#include "src/resample_fast_native.h"
#include "build/phone_hooks.h"
#include <pthread.h>
#define F(p,o,T) (*(T*)((char*)(p)+(o)))
static unsigned char engine[0x800],things[0x700],track[0x2200],wrong_track[0x2200],program[0x2900],policy[0x210],routes[0x90],clock_data[0xd0],note[56];
static unsigned char context[0xa50],settings[0x900],thread_owner[0x200],audio_info[0xb0],target[0x108];
static uintptr_t executor[5];
static uintptr_t property_vtable[3];

static void patch(uintptr_t at,void*fn){uintptr_t page=(base+at)&~(uintptr_t)4095;assert(!mprotect((void*)page,4096,PROT_READ|PROT_WRITE));uint32_t code[]={0x58000050,0xd61f0200};memcpy((void*)(base+at),code,8);memcpy((void*)(base+at+8),&fn,8);__builtin___clear_cache((char*)(base+at),(char*)(base+at+16));assert(!mprotect((void*)page,4096,PROT_READ|PROT_EXEC));}
static int disabled(void*p){(void)p;return 0;}
static void ignored(void*p){(void)p;}
static void out_event(void*p,int n,int count,void*a,void*b,void*c){(void)p;(void)n;(void)count;(void)a;(void)b;(void)c;}
static void toggle(void*p,int on){assert(p==thread_owner+0x30&&!on);}
static void*no_tls(unsigned key){assert(!key);return NULL;}
static unsigned clock_ms(void){return 0;}
static void zero(float*p,unsigned n){memset(p,0,n*sizeof(float));}

static unsigned char instrument[0x2400],sample[0x138],metadata[0x400],mixer[0x200],mixer_owner[0x200],layers[8*0xb30],sample_ref[24];
static unsigned char linked_instrument[0x2400],linked_sample[0x138],linked_metadata[0x400],linked_layers[8*0xb30];
static unsigned linked_mode,checking_render,physical_mode;
static uintptr_t render_vtable[3];
static unsigned render_on,render_off,notes,blocks,writes,foreign_notes,closes;
static unsigned outside_calls,fallback_calls;
static _Thread_local int outside;
static uint64_t source_frames=8000,written;
static unsigned scenario;
static unsigned tap_mode,both_outputs,tap_writes,missing_route;
static unsigned char tap_root[0x138],tap_container[0x180];
static unsigned char physical_audio[0x130];static float physical_left[128],physical_right[128];static float*physical_channels[]={physical_left,physical_right};
static float tap_left[128],tap_right[128];
static float*tap_channels[]={tap_left,tap_right};
static unsigned real_writer;
static unsigned char primary_audio[0x130];static float primary_l[128],primary_r[128];
static float*primary_channels[]={primary_l,primary_r};static uintptr_t primary_vtable[4],primary_sink[1],primary_sinks[1];static float primary_peak;
static int pcm_sink(void*self,const float*l,const float*r,unsigned n){assert(self==primary_sink&&n<=128);written+=n;++writes;for(unsigned k=0;k<n;++k){float p=fmaxf(fabsf(l[k]),fabsf(r[k]));if(p>primary_peak)primary_peak=p;}return 1;}
static void init_primary(void){
 memset(target,0,sizeof(target));memset(primary_audio,0,sizeof(primary_audio));memset(primary_l,0,sizeof(primary_l));memset(primary_r,0,sizeof(primary_r));primary_peak=0;
 primary_vtable[3]=(uintptr_t)pcm_sink;primary_sink[0]=(uintptr_t)primary_vtable;primary_sinks[0]=(uintptr_t)primary_sink;
 F(primary_audio,0,int)=2;F(primary_audio,4,int)=128;F(primary_audio,16,void*)=primary_channels;F(primary_audio,0x128,unsigned char)=1;
 F(target,0x18,void*)=primary_sinks;F(target,0x20,void*)=primary_sinks+1;F(target,0x28,void*)=primary_sinks+1;
 F(target,0x30,void*)=primary_audio;F(target,0x38,void*)=primary_audio+sizeof(primary_audio);F(target,0x40,void*)=primary_audio+sizeof(primary_audio);
 F(target,0x60,void*)=target+0x50;F(target,0x90,void*)=target+0x80;F(target,0xc0,void*)=target+0xb0;
}

static void*lookup(void*p,int pad){assert(p==program&&(pad==18||(linked_mode&&pad==21)));return pad==18?instrument:linked_instrument;}
static void prepare_stub(void*c,void*e){assert(c==context&&e==engine);F(c,0x9f8,uint64_t)=4*8000;}
static void play(void*e,int64_t time,unsigned on){assert(e==engine&&!time);if(on)++render_on;else ++render_off;}
static void*other_thread(void*p){assert(!p);outside=1;
 FN(0x211a230,void(*)(void*,int64_t,uintptr_t,void*,void*))(engine,0,0,track,note);
 FN(0x2119d00,void(*)(void*,int64_t,void*,void*,unsigned,unsigned))(engine,0,track,note,2,0x7ffffffe);return NULL;
}
static void clock_tick(void*e,void*c){assert(e==engine&&c==context+8);++blocks;
 if(blocks==1){pthread_t t;assert(!pthread_create(&t,NULL,other_thread,NULL)&&!pthread_join(t,NULL));}
 /* User sequence notes pass neither gate during private export. */
 FN(0x211a230,void(*)(void*,int64_t,uintptr_t,void*,void*))(engine,0,0,track,note);
 FN(0x2119d00,void(*)(void*,int64_t,void*,void*,unsigned,unsigned))(engine,0,track,note,2,0x7ffffffe);
}
static void out_seq(void*p,void*t,int64_t time,void*event){(void)p;(void)t;(void)time;(void)event;}
static void transformed_fast(void*t,int64_t tick,void*e){assert(t==track&&!tick&&e&&F(e,48,unsigned char)==60&&F(e,16,float)==1.f);assert(F(instrument,0x2018,unsigned char)==1);for(unsigned k=0;k<8;++k){assert(!F(layers+k*0xb30,0x918,unsigned));if(linked_mode&&checking_render)assert(!F(linked_layers+k*0xb30,0x918,unsigned));}if(outside){++outside_calls;return;}++notes;if(F(e,40,int64_t)==F(note,40,int64_t))++foreign_notes;}
static void process(void*s,int frames){assert(s==settings&&frames==128);}
static void mix(void*m,void*audio,int frames){assert(m==mixer&&frames==128&&F(audio,0,int)==4);float**ch=F(audio,16,float**);uint64_t first=F(context,0x9f0,uint64_t);
 /* Native2226fb0 ignores its x1 local buffer. Device Sample renders use
  * the master tap, with output+8 null. The direct path is tested separately. */
 F(target,8,void*)=tap_mode&&!both_outputs?NULL:audio;
 if(tap_mode){F(tap_container+0x10,0,int)=2;F(tap_container+0x10,4,int)=128;F(tap_container+0x10,16,float**)=tap_channels;audio=tap_container+0x10;ch=tap_channels;}
 F(audio,0x128,unsigned char)=0;
 uint64_t sounding=linked_mode?source_frames*3:scenario==4?source_frames*2:source_frames;
 for(int n=0;n<frames;++n){ch[0][n]=notes&&first+(unsigned)n<sounding?.25f:0;ch[1][n]=scenario==1?.1f:ch[0][n];if(!tap_mode)ch[2][n]=ch[3][n]=0;}
 if(physical_mode){
  F(physical_audio,0,int)=2;F(physical_audio,4,int)=128;F(physical_audio,16,float**)=physical_channels;F(physical_audio,0x128,unsigned char)=0;
  for(int n=0;n<frames;++n){physical_left[n]=ch[0][n];physical_right[n]=ch[1][n];ch[0][n]=ch[1][n]=0;}
 }
 if(scenario==2)ch[0][0]=NAN;
 if(scenario==3&&blocks==2)F(context,0xa18,unsigned char)=1;
 if(scenario==5){ch[0][0]=ch[1][0]=NAN;F(audio,0x128,unsigned char)=1;}
}
static void write_fixture(void*w,void*a,void*g,unsigned n){assert(w==target&&a&&g&&n<=128);if(tap_mode&&!both_outputs){assert(a==(physical_mode?physical_audio:tap_container+0x10));assert(F(g,0,float)==1.f&&F(g,4,float)==1.f&&F(g,8,float)==0.f);}if(!tap_mode)written+=n;++writes;}
static void tap_fixture(void*w,void*a,void*key,float gain){assert(tap_mode&&w==target&&a==tap_container+0x10&&!key&&gain==1.f);++tap_writes;}
static int writer_good(void*w,unsigned n){assert(w==target&&n<=128);if(tap_mode)written+=n;return 1;}
static void sync_audio(void*p){assert(p==audio_info);}
static void no_params(void*p){assert(p==audio_info);}
static void mixer_ready(void*p,unsigned n){assert(p==mixer&&n==128);}
static void close_writer(void*p){assert(p==target);++closes;}
static void unowned(void*c,void*s,void*out,void*event,void*t){assert(c==context&&s==settings&&out==target&&event==note&&t==wrong_track);++fallback_calls;}
static void invoke_render(void){uint32_t insn=F((void*)base,0x18b9df0,uint32_t);int32_t delta=(int32_t)(insn<<6)>>6;
 FN(0x18b9df0+(int64_t)delta*4,void(*)(void*,void*,void*,void*,void*))(context,settings,target,note,wrong_track);
}
static void run_one(unsigned which,uint64_t length){
 if(real_writer)init_primary();
 missing_route=which>=15;tap_mode=which>=7;both_outputs=which==14;which=missing_route?which-15:which%7;scenario=which;tap_writes=0;source_frames=length;F(metadata,4,int)=(int)length;F(metadata,8,unsigned)=8000;F(metadata,0x3d0,float)=8000.f;
 notes=blocks=writes=foreign_notes=render_on=render_off=outside_calls=closes=0;unsigned loop=which==6?0:1;F(instrument,0x2018,unsigned char)=1;for(unsigned k=0;k<8;++k){F(layers+k*0xb30,0x918,unsigned)=loop?k%3+1:0;F(layers+k*0xb30,0x7b8,int64_t)=5;F(layers+k*0xb30,0x810,int64_t)=1000;F(layers+k*0xb30,0x868,int64_t)=20;F(layers+k*0xb30,0x8c0,int64_t)=900;}written=0;memset(context,0,sizeof(context));
 if(linked_mode){F(linked_metadata,4,int)=(int)(length*3);for(unsigned k=0;k<8;++k)F(linked_layers+k*0xb30,0x918,unsigned)=k%3+1;}
 F(context,0x398,unsigned)=8000;F(context,0x70,void*)=thread_owner;F(context,0x88,void*)=audio_info;F(context,0x9f8,uint64_t)=4*8000;F(context,0xa38,void*)=target;
 F(context,0x3f0,unsigned)=tap_mode?4:0;F(context,0x4a0,unsigned char)=missing_route?0xa0:tap_mode;F(target,8,void*)=tap_mode&&!both_outputs?NULL:target;F(context,0xa0,void*)=tap_root;F(tap_root,0x130,void*)=tap_container;
 /* Actual r20a device crash: service+6e8 held0x70, not a char*.
  * Poison both fields that the submission-settings fixture used to fake. */
 F(settings,0x6e8,uintptr_t)=0x70;F(settings,0x218,unsigned)=0xdeadbeef;assert(resample_fast_native_arm(settings,18,"/tmp/mpc-private-export-not-written.wav"));
 unsigned char before[56];memcpy(before,note,sizeof(before));checking_render=1;invoke_render();checking_render=0;assert(!memcmp(before,note,sizeof(before)));assert(F(context,0x4a0,unsigned char)==(missing_route?0xa0:tap_mode));
 fprintf(stderr,"FIXTURE blocks=%u writes=%u written=%llu err=%s\n",blocks,writes,(unsigned long long)written,resample_fast_native_error());
 if(!length){assert(!notes&&!render_on&&!render_off&&closes==1&&resample_fast_native_error()&&F(instrument,0x2018,unsigned char)==1);return;}
 assert(notes==1&&!foreign_notes&&render_on==1&&render_off==1&&outside_calls==2&&closes==1);
 if(which!=2){if(tap_mode){assert(real_writer?writes:(both_outputs?(tap_writes&&writes):(!tap_writes&&writes)));}else assert(!tap_writes&&writes);}
 if(which==0||which==4||which==5||which==6){uint64_t body=linked_mode?length*3:which==4?length*2:length;assert(!resample_fast_native_error()&&written>=body+4000&&written<body+4128);}
 else assert(resample_fast_native_error()&&F(context,0xa18,unsigned char));
 assert(F(instrument,0x2018,unsigned char)==1&&F(sample_ref,8,unsigned)==100);
 for(unsigned k=0;k<8;++k){assert(F(layers+k*0xb30,0x918,unsigned)==(loop?k%3+1:0));assert(F(layers+k*0xb30,0x7b8,int64_t)==5&&F(layers+k*0xb30,0x810,int64_t)==1000&&F(layers+k*0xb30,0x868,int64_t)==20&&F(layers+k*0xb30,0x8c0,int64_t)==900);F(layers+k*0xb30,0x918,unsigned)=0;}
 if(linked_mode)for(unsigned k=0;k<8;++k)assert(F(linked_layers+k*0xb30,0x918,unsigned)==k%3+1);
 if(real_writer){fprintf(stderr,"NATIVE_PRIMARY peak=%.6g frames=%llu\n",(double)primary_peak,(unsigned long long)written);assert(primary_peak==.25f&&!tap_writes);}
 /* Unscoped native calls still forward unchanged through both trampolines. */
 FN(0x211a230,void(*)(void*,int64_t,uintptr_t,void*,void*))(engine,0,0,track,note);fprintf(stderr,"FORWARD notes=%u foreign=%u\n",notes,foreign_notes);assert(foreign_notes==1);
}
static void previous_bridge(uintptr_t entry,uintptr_t bridge){
 uint32_t original=F((void*)base,entry+4,uint32_t);intptr_t d=(intptr_t)bridge-(intptr_t)(entry+4);
 F((void*)base,entry+4,uint32_t)=0x14000000u|((uint32_t)(d/4)&0x3ffffffu);
 uint32_t jump[]={original,0x58000070,0xd61f0200,0xd503201f};memcpy((void*)(base+bridge),jump,16);F((void*)base,bridge+16,uintptr_t)=base+entry+8;
 __builtin___clear_cache((char*)(base+entry),(char*)(base+entry+8));__builtin___clear_cache((char*)(base+bridge),(char*)(base+bridge+24));
}
static int no_preview_gate(void*t,const void*e,unsigned mask){assert(t==track&&e&&mask==0x7ffffffe);return 0;}
static void frozen_bridges(void){
 FILE*f=fopen("build/r20/autorun/phone-code.bin","rb");assert(f);assert(fread((void*)(base+0x6d3d000),1,0x5000,f)>4096);fclose(f);
 f=fopen("build/r20/autorun/phone-table.bin","rb");assert(f&&fread((void*)(base+PHONE_TABLE_VA),1,4096,f)==4096);fclose(f);
 assert(phone_hooks[H_PREVIEW_EVENT_GATE].entry==0);
 F((void*)base,PHONE_TABLE_VA+64+H_PREVIEW_EVENT_GATE*16+8,uintptr_t)=(uintptr_t)no_preview_gate;
 const char*names[]={"phone-muted-sequence_input-gate.bin","phone-muted-sequence-gate.bin"};uintptr_t entries[]={0x211a234,0x2119d04};
 for(unsigned i=0;i<2;++i){char file[128];snprintf(file,sizeof(file),"build/r20/autorun/%s",names[i]);f=fopen(file,"rb");assert(f&&fread((void*)(base+entries[i]),1,4,f)==4);fclose(f);}
 __builtin___clear_cache((char*)base,(char*)(base+0x7500000));
}
int main(int argc,char**argv){
 assert(argc==2||argc==3);struct sigaction sa={.sa_sigaction=fault,.sa_flags=SA_SIGINFO};sigaction(SIGSEGV,&sa,NULL);sigaction(SIGILL,&sa,NULL);sigaction(SIGABRT,&sa,NULL);load(argv[1]);setbuf(stdout,NULL);phase="fast native loop";physical_mode=argc==3&&!strcmp(argv[2],"physical-output");real_writer=physical_mode||(argc==3&&!strcmp(argv[2],"primary-writer"));
 patch(0x21152d0,ignored);patch(0x2133800,disabled);patch(0x29cb250,transformed_fast);patch(0x212cba0,out_event);patch(0x3b89890,disabled);patch(0x21560f0,out_seq);patch(0x2142f00,clock_ms);
 patch(0xb53508,no_tls);patch(0x2142bd0,no_params);patch(0xdd20b0,zero);patch(0xf2f240,clock_ms);patch(0x18b4500,close_writer);patch(0x2227150,ignored);
 patch(0x18b5bc0,prepare_stub);patch(0x2114c00,clock_tick);patch(0x2122020,play);patch(0x2143040,sync_audio);patch(0x2142c00,sync_audio);patch(0x2226ab0,mixer_ready);patch(0x2226fb0,mix);if(!real_writer){patch(0x18b8dd0,write_fixture);patch(0x18b3b10,writer_good);}patch(0x286bbe0,lookup);
 F(engine,0x3c0,void*)=things;F(engine,0x5d8,void*)=policy;F(engine,0x7a8,void*)=routes;F(engine,0x788,void*)=clock_data;F(engine,0x770,void*)=mixer;F(mixer_owner,0xc8,void*)=mixer;F(mixer,0x120,void*)=mixer_owner;F(engine,0x788,void*)=mixer_owner;
 F(things,0xb8,void*)=program;F(things,0x140,void*)=track;F(track,0xbb8,void*)=program;F(track,0x750+40,unsigned)=1;
 property_vtable[2]=(uintptr_t)disabled;F(track,0x890,void*)=property_vtable;
 F(program,0x12a8+18*4,int)=60;F(program,0x14a8+60*4,int)=18;F(instrument,0x2310,void*)=program;F(sample,0x130,void*)=metadata;F(instrument,0x22e8,void*)=layers;F(instrument,0x22f0,void*)=layers+sizeof(layers);F(layers,0x4a8,void*)=sample;F(layers,0x4b0,void*)=sample_ref;F(sample_ref,8,unsigned)=100;F(sample_ref,12,unsigned)=100;
 for(unsigned k=0;k<8;++k)FN(0x2949b30,void(*)(void*))(layers+k*0xb30+0x740);
 linked_mode=argc==3&&!strcmp(argv[2],"pad-link");
 if(linked_mode){
  F(instrument,0x234d,unsigned char)=1;F(instrument,0xea8,int)=22;
  /* Repeat/self targets affect native playback but must not duplicate the
   * duration/Loop guard. A link on the linked pad must not be traversed. */
  F(instrument,0xf00,int)=22;F(instrument,0xf58,int)=19;
  F(linked_instrument,0x234d,unsigned char)=1;F(linked_instrument,0xea8,int)=100;
  F(linked_instrument,0x2310,void*)=program;F(linked_instrument,0x22e8,void*)=linked_layers;F(linked_instrument,0x22f0,void*)=linked_layers+sizeof(linked_layers);
  F(linked_layers,0x4a8,void*)=linked_sample;F(linked_layers,0x4b0,void*)=sample_ref;F(linked_sample,0x130,void*)=linked_metadata;F(linked_metadata,8,unsigned)=8000;F(linked_metadata,0x3d0,float)=8000.f;
  for(unsigned k=0;k<8;++k)FN(0x2949b30,void(*)(void*))(linked_layers+k*0xb30+0x740);
 }
 typedef struct {uint64_t q[7];} Event;*(Event*)note=FN(0x27e54d0,Event(*)(unsigned,int64_t,uint64_t,unsigned,unsigned,float))(60,0,960,100,0,1.f);
 executor[4]=(uintptr_t)toggle;F(thread_owner,0x30,void*)=executor;F(audio_info,0xa8,unsigned)=128;F(settings,0x588,void*)=audio_info;F(settings,0x5a0,void*)=mixer;F(settings,0x5c0,void*)=engine;F(settings,0x218,unsigned)=0xdeadbeef;render_vtable[2]=(uintptr_t)process;F(settings,0,void*)=render_vtable;F(target,8,void*)=target;
 F(settings,0x494,float)=1.f;
 if(physical_mode){F(tap_container,8,void*)=physical_audio;F(tap_container,0x140,unsigned)=1;}
 if(argc==3&&strcmp(argv[2],"tap-only")&&strcmp(argv[2],"missing-output")&&!real_writer){if(!strcmp(argv[2],"frozen-bridge"))frozen_bridges();else{previous_bridge(0x211a230,0x6d3d000);previous_bridge(0x2119d00,0x6d3d040);}}
 uint32_t expected=F((void*)base,0x18b9f7c,uint32_t);F((void*)base,0x18b9f7c,uint32_t)=0;
 assert(!resample_fast_native_install(base)&&F((void*)base,0x18b9df0,uint32_t)==0x97ffff18);F((void*)base,0x18b9f7c,uint32_t)=expected;
 assert(!resample_fast_native_ready(base));assert(resample_fast_native_install(base)&&resample_fast_native_ready(base));assert(resample_fast_native_install(base));assert(!resample_fast_native_install(base+4096));
 patch(0x18b94f0,tap_fixture);patch(0x18b9a50,unowned);F(settings,0x6e8,uintptr_t)=0x70;
 assert(resample_fast_native_arm(wrong_track,18,"/tmp/owned.wav"));invoke_render();assert(fallback_calls==1&&resample_fast_native_error());
 if(argc==3&&!strcmp(argv[2],"tap-only")){run_one(7,8000);puts("PASS native master tap is monitored without the optional direct output.");return 0;}
 if(argc==3&&!strcmp(argv[2],"missing-output")){run_one(15,8000);puts("PASS missing Source4 output enabled and restored.");return 0;}
 if(argc==3&&!strcmp(argv[2],"seq-trace")){
  run_one(0,8000);notes=blocks=writes=render_on=render_off=outside_calls=closes=0;written=0;tap_mode=0;F(context,0x9f0,uint64_t)=0;F(context,0x9f8,uint64_t)=32000;F(context,0xa18,unsigned char)=0;F(context,0x3f0,unsigned)=0;F(context,0x4a0,unsigned char)=0;
  FN(0x18b9ee0,void(*)(void*,void*,void*))(context,settings,target);
  assert(written==32000&&blocks==250&&render_on==1&&render_off==1&&closes==1);puts("PASS unowned native Seq duration/transport/output preserved with bounded writer diagnostics.");return 0;
 }
 if(real_writer){run_one(15,8000);puts("PASS production adapter + native accumulator/flush: destination PCM nonzero; DSP and final encoder are fixtures.");return 0;}
 if(linked_mode){run_one(0,8000);run_one(2,8000);run_one(3,8000);run_one(15,8000);puts("PASS production Pad Link render guard: longer linked sample determines duration, source/link Loop restored after success/failure/cancel, duplicate/self links guarded once, native source dispatched once and foreign sequence events excluded. DSP remains a fixture.");return 0;}
 run_one(0,45*8000);run_one(0,8000);run_one(1,8000);run_one(2,8000);run_one(3,8000);run_one(4,8000);run_one(5,8000);run_one(6,8000);run_one(0,0);
 for(unsigned n=7;n<=21;++n)run_one(n,8000);
 puts("PASS actual native Seq rendering loop, four-channel buffer, Seq note factory/input/filter/dispatch, scoped gates, long-source frame budget, quiet tail and failures; services/DSP/writer are fixtures, service+6e8=0x70 and +218=deadbeef never dereferenced as submission settings; native render-context rate used; hardware audio unproven.");return 0;
}
