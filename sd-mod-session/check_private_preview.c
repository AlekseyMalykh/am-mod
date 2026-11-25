/* Real private SampleFileAudition worker and start/stop implementation.
 * Program lookup, properties, UI dispatch and final audio are fixtures. */
#define main original_workflow_main
#include "check_workflow.c"
#undef main
#include <pthread.h>
static int swap_on;static unsigned transport_bits,preview_notifications,knob_calls;
int seq_audition_active(void){return swap_on;}
unsigned step_native_transport_state(void){return transport_bits;}
void seq_audition_preview(int on){if(on)++preview_notifications;}
int seq_audition_knob(unsigned value){assert(value==80);++knob_calls;return 1;}
int seq_audition_view(struct seq_audition_view*out){*out=(struct seq_audition_view){2,2,0,-2,5};return swap_on;}
static unsigned large_note,status_badge,load_restores;
void seq_audition_off(const char*why){assert(!strcmp(why,"load selected file")||!strcmp(why,"load"));swap_on=0;++load_restores;}
static void audition_text(void*g,const uint64_t*s,int x,int y,int w,int h,int flags,int lines,float scale){
 const char*t=(char*)(uintptr_t)*s;
 if(!strcmp(t,"D")){assert(x==107&&y==212&&height>=25&&w==105&&h==26);++large_note;}
 if(strstr(t,"SWAPPED"))++status_badge;
 output_text(g,s,x,y,w,h,flags,lines,scale);
}
static void set_bounds(void*p,uint64_t xy,uint64_t wh){FIELD(p,0x20,int)=(int)xy;FIELD(p,0x24,int)=(int)(xy>>32);FIELD(p,0x28,int)=(int)wh;FIELD(p,0x2c,int)=(int)(wh>>32);}
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
 /* Swap must not veto the native load. Suppress only its private voice. */
 swap_on=1;transport_bits=3;FIELD(reference,8,unsigned)=1;
 unsigned old_starts=starts;assert(workflow_preview_worker(payload));FN(0x20bd5d0,void(*)(void*))(payload);workflow_preview_played(payload);
 assert(starts==old_starts+1&&!voices&&FIELD(private_layer,0x4a8,void*)==source_sample&&preview_notifications);
 transport_bits=0;FIELD(reference,8,unsigned)=1;assert(workflow_preview_worker(payload));FN(0x20bd5d0,void(*)(void*))(payload);workflow_preview_played(payload);assert(voices==1);
 stub(0x112fd50,component_visible);stub(0x112f6b0,unblocked);stub(0x1149880,invalidate);stub(0x102d5e0,set_colour);stub(0x102e000,fill);stub(0x1082810,font_make);stub(0x102dca0,font_use);stub(0x1070d60,font_end);stub(0x10817b0,audition_text);stub(0x1160fc0,set_bounds);__builtin___clear_cache((char*)base,(char*)base+0x7500000);
 files=file_root;is_visible=file_root;FIELD(file_root,0x1388,int)=FILE_SAMPLES;
 unsigned char knob[16]={0};knob[8]=0x11;knob[9]=80;assert(workflow_control(knob,workflow_ticket())&&knob_calls==1);
 audition_panels((void*)1);assert(large_note==1&&status_badge==1);
 for(unsigned n=0;n<2;++n){void*p=file_root+(n?0x11d0:0xff0);FIELD(p,0x18,void*)=file_root;FIELD(p,0x24,int)=n?198:38;FIELD(p,0x2c,int)=n?38:160;}
 browser_path_layout(file_root);assert(FIELD(file_root,0xff0+0x24,int)==58&&FIELD(file_root,0xff0+0x2c,int)==140&&FIELD(file_root,0x11d0+0x24,int)==38&&FIELD(file_root,0x11d0+0x2c,int)==20);
 /* Playing a different pad (including another bank) changes native selection,
  * but the replacement must load into the captured absolute pad 5. */
 stub(0x28f9d60,select_pad);__builtin___clear_cache((char*)base+0x28f9d60,(char*)base+0x28f9d80);
 browser_context_live=1;selected_pad=79;result_pad=79;result_pad_ready=1;
 search=results=1;swap_on=0;assert(replacement_destination()==79);
 swap_on=1;assert(replacement_destination()==5);search=results=0;
 FIELD(file_root,0x100,void*)=surface_controls;atomic_store(&selected_audio,1);unsigned char load_event[16]={0};load_event[8]=0x6f;load_event[9]=1;
 assert(!workflow_button(load_event,workflow_ticket())&&!swap_on&&load_restores==1);
 assert(selected_pad==5&&result_pad==5&&result_pad_ready);
 swap_on=1;selected_pad=127;result_pad=127;finish_replacement("load");
 assert(!swap_on&&selected_pad==5&&result_pad==5&&load_restores==2);
 /* No active replacement preserves the user's ordinary destination. */
 selected_pad=79;result_pad=79;finish_replacement("load");assert(selected_pad==79&&result_pad==79);
 puts("PASS Swap/Preview playing and stopped: native candidate load always runs; only private voice is silenced during Play; K2 owns Orig Note, 25px note/status panels and upper folder path resize the native list");
 atomic_store(&preview_allowed,0);assert(!workflow_preview_worker(payload));
 puts("PASS actual native SampleFileAudition20bd5d0 ->20bd210 -> private instrument20: one-shot/Loop Off, exact selected slice copied to private layer, natural completion20bc9c0 resets replay, single-voice replacement, native force-stop20bc8b0 and refcounts; source retained; UI/property/final audio receivers remain fixtures");
}
