/* Real private SampleFileAudition worker and start/stop implementation.
 * Program lookup, properties, UI dispatch and final audio are fixtures. */
#define main original_workflow_main
#include "check_workflow.c"
#undef main
#include <pthread.h>
static unsigned char private_program[0x1400],private_instrument[0x2400],private_layer[0xb00],source_sample[0x138],source_backing[0x400],shared_sample[24],ui_queue[0x200];
static uintptr_t ui_vtable[3];static unsigned replacements,voices,cleanups;static unsigned starts,stops,playing_state,oneshot_mode,loop_enabled;
static void*private_lookup(void*p,int pad){assert(p==private_program&&pad==20);return private_instrument;}
static void property_bool(void*p,int v){if(p==volume_object+0x298){playing_state=v;FIELD(volume_object,0x2e8,unsigned char)=v;}else if(p==private_instrument+0x1fc8)loop_enabled=v;else assert(p==private_instrument+0x2020&&v==1);}
static void property_mode(void*p,int v){assert(p==private_instrument+0x15e8);oneshot_mode=v;}
static void private_gain(void*p,float v){assert(p==private_instrument&&v==.25f);}
static float convert_rate(void*p,const float*v){assert(p==private_instrument+0x20d0&&*v==44100.f);return *v;}
static int no_ui_enqueue(void*p,void*f,uintptr_t key){(void)f;(void)key;assert(p==ui_queue+0x30);return 0;}
static void private_start(void*p,unsigned port,unsigned mask,void*x,const unsigned char*note){assert(p==private_program+0xdf0&&!port&&mask==0x7ffffff5&&!x&&note[0]==72&&!voices);assert(FIELD(note,8,float)==1.f);voices=1;++starts;}
static void private_stop(void*p,unsigned port,unsigned note,unsigned force,float fade){assert(p==private_program&&!port&&note==72&&force==1 );voices=0;if(fade==0.f)++replacements;else{assert(fade==5.f);++stops;}}
static int has_private_voice(void*p,void*i,unsigned note){assert(p==private_program&&i==private_instrument&&note==72);return voices!=0;}
static void cleanup_private(void*p,void*i){assert(p==private_program&&i==private_instrument);++cleanups;}
int main(int argc,char**argv){assert(argc==2);struct sigaction sa={0};sa.sa_sigaction=fault_signal;sa.sa_flags=SA_SIGINFO;sigaction(SIGSEGV,&sa,NULL);sigaction(SIGBUS,&sa,NULL);load_elf(argv[1],0x1800000000ULL);
 stub(0x286bbe0,private_lookup);stub(0x13904f0,property_bool);stub(0x150b380,property_mode);stub(0x276c590,private_gain);stub(0x1e1e2b0,private_start);stub(0x286d8a0,private_stop);
 stub(0x286c6d0,has_private_voice);stub(0x286c5f0,cleanup_private);
 __builtin___clear_cache((char*)base,(char*)(base+0x7500000));
 FIELD(volume_object,0x358,void*)=private_program;FIELD(private_program,0x12f8,unsigned)=72;FIELD(private_instrument,0x22e8,void*)=private_layer;
 FIELD(private_layer,0x4a8,void*)=source_sample;FIELD(private_layer,0x4b0,void*)=shared_sample;FIELD(shared_sample,8,unsigned)=100;FIELD(shared_sample,12,unsigned)=100;
 FIELD(source_sample,0x130,void*)=source_backing;FIELD(source_backing,4,unsigned)=10000;FIELD(source_backing,0x3d0,float)=44100.f;
 FIELD(private_instrument,0x20e0,void*)=(void*)1;FIELD(private_instrument,0x20e8,void*)=convert_rate;
 FIELD(volume_object,0x110,float)=.25f;FIELD(volume_object,0x3e0,void*)=source_sample;FIELD(volume_object,0x10,void*)=ui_queue;
 ui_vtable[2]=(uintptr_t)no_ui_enqueue;FIELD(ui_queue,0x30,void*)=ui_vtable;pthread_key_t ui_key;assert(!pthread_key_create(&ui_key,NULL));FIELD(ui_queue,0x1b0,unsigned)=ui_key;
 FN(0x2949b30,void(*)(void*))(private_layer+0x740);
 FN(0x236f0d0,void(*)(void*))(private_instrument+0x2078);
 atomic_store(&preview_owner,(uintptr_t)volume_object);atomic_store(&preview_sample,(uintptr_t)source_sample);atomic_store(&preview_first,1000);atomic_store(&preview_end,2500);atomic_store(&preview_range,1);atomic_store(&preview_allowed,1);
 unsigned char reference[16]={0},payload[80]={0};FIELD(reference,8,unsigned)=1;FIELD(payload,0,void*)=reference;FIELD(payload,8,void*)=volume_object;FIELD(payload,0x10,unsigned)=1;
 assert(workflow_preview_worker(payload));FN(0x20bd5d0,void(*)(void*))(payload);workflow_preview_played(payload);preview_private=1;preview_source=3;volume_service=volume_object;preview_refresh();assert(preview_source==3);
 assert(replacements==1&&starts==1&&playing_state==1&&oneshot_mode==0&&!loop_enabled&&!FIELD(reference,8,unsigned));
 assert(FIELD(private_layer,0x740+0x78,int64_t)==1000&&FIELD(private_layer,0x740+0xd0,int64_t)==2500&&FIELD(shared_sample,8,unsigned)==100&&!FIELD(volume_object,0x18,unsigned));
 FIELD(volume_object,0x3d8,int64_t)=-1;FIELD(reference,8,unsigned)=1;FN(0x20bc9c0,void(*)(void*))(payload);preview_refresh();assert(playing_state&&preview_source==3&&!cleanups&&!FIELD(reference,8,unsigned));
 voices=0;FIELD(reference,8,unsigned)=1;FN(0x20bc9c0,void(*)(void*))(payload);preview_refresh();assert(!playing_state&&preview_source==128&&cleanups==1&&!FIELD(reference,8,unsigned));
 /* Consecutive requests share one private voice, never overlap. */
 for(unsigned n=0;n<3;++n){FIELD(reference,8,unsigned)=1;assert(workflow_preview_worker(payload));FN(0x20bd5d0,void(*)(void*))(payload);assert(voices==1&&starts==n+2&&replacements==n+2);}
 FIELD(reference,8,unsigned)=1;FN(0x20bc8b0,void(*)(void*))(payload);assert(stops==1&&!playing_state&&!FIELD(reference,8,unsigned));
 atomic_store(&preview_allowed,0);assert(!workflow_preview_worker(payload));
 puts("PASS actual native SampleFileAudition20bd5d0 ->20bd210 -> private instrument20: one-shot/Loop Off, exact selected slice copied to private layer, natural completion20bc9c0 resets replay, single-voice replacement, native force-stop20bc8b0 and refcounts; source retained; UI/property/final audio receivers remain fixtures");
}
