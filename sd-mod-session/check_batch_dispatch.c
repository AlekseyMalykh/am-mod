/* Execute the real CommandManager ctor and dispatch/command state wrapper.
 * History storage and notifications remain isolated adapters, not the entry
 * pointer or command perform path which r14a's old test accidentally hid. */
#define BATCH_PROOF_MAIN batch_proof_main
#include "check_batch_native.c"
#undef BATCH_PROOF_MAIN
static unsigned char environment[0x100],transaction[0x88];
static uintptr_t environment_vtable[8];
static unsigned dispatch_entered,history_added;
static unsigned char queues[0x260],queue_slots[2][0xc0];
static unsigned progress_seen;
static void queue_mutex(void*p){assert(p==queues+0x220);}
static void observe_progress(void){struct batch_copy_progress p;batch_copy_poll(&p);assert(p.total==4&&p.done<=4);progress_seen|=1u<<p.phase;}
static int valid_context(void*p,void*flags,int mode){assert(p==environment&&flags&&mode==3);return 1;}
static void bind_monitor(void*p,void*manager,int enabled){assert(p==environment+8&&manager==copy_factory&&enabled==1);}
static void enter_dispatch(void*p){assert(p==copy_factory&&FIELD(p,0,uintptr_t)==base+0x6d97818);++dispatch_entered;}
static void trim_history(void*p,uint64_t limit){assert(p==copy_factory);(void)limit;}
static void retain_history(void*p,uintptr_t*shared,int index){
 assert(p==transaction&&index==0);++history_added;
 if(kept[1])FN(0x123bca0,void(*)(void*))((void*)kept[1]);
 kept[0]=shared[0];kept[1]=shared[1];++FIELD((void*)kept[1],8,unsigned);
}
static unsigned char exit_mode[0x180],mode_entry[40],sample_mode[0x60],copy_mode_object[0x60],mode_nodes[2][0x30];
static int worker_active,copy_active=1,sample_active,activation_calls,exit_signals;
static void native_property_lock(void*p){assert(p==exit_mode+0xa8);}
static int native_property_try_lock(void*p){native_property_lock(p);return 0;}
static void activate_mode(void*p,int enabled){assert(worker_active);if(p==copy_mode_object+0x20)copy_active=enabled;else{assert(p==sample_mode+0x20);sample_active=enabled;}++activation_calls;}
static void exit_signal(void*p,uint64_t code,uint64_t count,void*a,void*b){assert(p==exit_mode+8&&code&&count==1&&a);(void)b;++exit_signals;}
static void init_copy_mode(void){
 memset(exit_mode,0,sizeof(exit_mode));memset(mode_entry,0,sizeof(mode_entry));memset(mode_nodes,0,sizeof(mode_nodes));
 FIELD(exit_mode,0x80,int)=0x20a7;FIELD(exit_mode,0xd8,int)=0x30ed;
 FIELD(exit_mode,0x130,void*)=mode_entry;FIELD(exit_mode,0x138,void*)=mode_entry+40;FIELD(exit_mode,0x140,void*)=mode_entry+40;
 FIELD(mode_entry,0,int)=0x30ed;
 void*modal_property=exit_mode+0x88;FIELD(modal_property,8,void*)=(char*)modal_property+8;FIELD(modal_property,0x10,void*)=(char*)modal_property+8;
 FIELD(exit_mode,0x148,int)=0x30ed;FIELD(exit_mode,0x14c,int)=0x20a7;FIELD(exit_mode,0x150,void*)=copy_mode_object;
 FIELD(exit_mode,0x168,void*)=mode_nodes[0];FIELD(mode_nodes[0],0x20,int)=0x20a7;FIELD(mode_nodes[0],0x28,void*)=sample_mode;
 FIELD(mode_nodes[0],0x18,void*)=mode_nodes[1];FIELD(mode_nodes[1],0x20,int)=0x30ed;FIELD(mode_nodes[1],0x28,void*)=copy_mode_object;
 FIELD(context,0x98,void*)=exit_mode;copy_active=1;sample_active=activation_calls=exit_signals=0;
}
static unsigned char preview_input_data[0x200],preview_interaction[0x90],preview_playing_mode[0x138],preview_engine[0x800],preview_transport[0x190],preview_record[0xa00];
static unsigned preview_on,preview_off,preview_pad_seen;
static void audio_on(void*p,int port,int pad,void*properties,float velocity){assert(worker_active&&p==preview_input_data&&!port&&pad==65&&!properties&&velocity==1.f);++preview_on;preview_pad_seen=(unsigned)pad;}
static void audio_off(void*p,int port,int pad){assert(worker_active&&p==preview_input_data&&!port&&pad==65);++preview_off;}
#ifndef BATCH_DISPATCH_MAIN
#define BATCH_DISPATCH_MAIN main
#endif
int BATCH_DISPATCH_MAIN(int argc,char**argv){
 assert(batch_proof_main(argc,argv)==0);
 restore_entry(argv[1],0x19d0a00);
 stub(0x19d12e0,bind_monitor);stub(0x19cd820,enter_dispatch);
 stub(0x19cd250,trim_history);stub(0x19cbc30,retain_history);
 __builtin___clear_cache((char*)base,(char*)(base+0x7500000));
 environment_vtable[5]=(uintptr_t)valid_context;FIELD(environment,0,void*)=environment_vtable;
 FN(0x19cf2b0,void(*)(void*,void*,void*,uint64_t))(copy_factory,environment,(void*)1,UINT64_MAX);
 assert(FIELD(copy_factory,0,uintptr_t)==base+0x6d97818);
 /* Existing open transaction isolates history allocation from the actual
  * dispatcher, native command validation, perform and result propagation. */
 FIELD(copy_factory,0x1b0,void*)=transaction;
 phase="real CommandManager dispatch with exact native self";
 unsigned s[4]={0,1,2,3},d[4]={40,41,42,43};
 assert(batch_copy_submit(base,context,s,d,4));
 assert(dispatch_entered==1&&history_added==1);
 for(unsigned i=0;i<4;++i)assert(FIELD(pads[d[i]],0x100,int)==FIELD(pads[s[i]],0x100,int));
 void*cmd=(void*)kept[0];
 assert(((int(*)(void*))FIELD((void*)FIELD(cmd,0,uintptr_t),0x48,uintptr_t))(cmd)==0);
 for(unsigned i=0;i<4;++i)assert(FIELD(pads[d[i]],0x100,int)==(int)(1000+d[i]));
 /* Actual native queue copies and later invokes the owned 40-byte closure;
  * the worker callback invokes the real dispatcher, never UI component code. */
 void*q=queues+0x1d8;FIELD(q,0,uint64_t)=2;FIELD(q,0x30,void*)=queue_slots;
 for(unsigned i=0;i<2;++i){FIELD(queue_slots[i],0,uintptr_t)=base+0xb57a7c;FIELD(queue_slots[i],8,uintptr_t)=base+0x15265b0;}
 FIELD(context,0x48,void*)=queues;stub(0xf40e50,queue_mutex);stub(0xf40e90,queue_mutex);
 __builtin___clear_cache((char*)base,(char*)(base+0x7500000));
 unsigned async_d[4]={44,45,46,47};copy_observer=observe_progress;
 assert(batch_copy_begin(base,context,s,async_d,4));
 struct batch_copy_progress p;batch_copy_poll(&p);assert(p.phase==BATCH_JOB_PREPARE&&p.total==4&&p.done==0);
 assert(FIELD(context,0x50,unsigned)==1&&!batch_copy_release());
 assert(!batch_copy_begin(base,context,s,async_d,4));
 for(unsigned i=0;i<4;++i)assert(FIELD(pads[async_d[i]],0x100,int)==(int)(1000+async_d[i]));
 uint64_t queue_read=0;FN(0x2b59200,void(*)(void*,uint64_t*))(q,&queue_read);assert(queue_read==1);
 batch_copy_poll(&p);assert(p.phase==BATCH_JOB_DONE&&p.done==4&&p.total==4&&FIELD(context,0x50,unsigned)==0);
 assert((progress_seen&(1u<<BATCH_JOB_PREPARE))&&(progress_seen&(1u<<BATCH_JOB_COPY)));
 assert(dispatch_entered==2&&history_added==2&&batch_copy_release());batch_copy_poll(&p);assert(p.phase==BATCH_JOB_IDLE);
 for(unsigned i=0;i<4;++i)assert(FIELD(pads[async_d[i]],0x100,int)==FIELD(pads[s[i]],0x100,int));
 /* Stopped queue: no dispatch, no leaked lifetime reference, no pending job. */
 FIELD(q,0x2d,unsigned char)=1;assert(!batch_copy_begin(base,context,s,async_d,4));
 batch_copy_poll(&p);assert(p.phase==BATCH_JOB_IDLE&&FIELD(context,0x50,unsigned)==0&&dispatch_entered==2);
 /* Failure before the first write propagates through the genuine native
  * dispatcher into a persistent Failed result, without another Undo entry. */
 FIELD(q,0x2d,unsigned char)=0;fail_clone=2;clones=0;
 assert(batch_copy_begin(base,context,s,async_d,4));
 FN(0x2b59200,void(*)(void*,uint64_t*))(q,&queue_read);assert(queue_read==0);
 batch_copy_poll(&p);assert(p.phase==BATCH_JOB_FAILED&&FIELD(context,0x50,unsigned)==0);
 assert(dispatch_entered==3&&history_added==2&&batch_copy_release());
 for(unsigned i=0;i<4;++i)assert(FIELD(pads[async_d[i]],0x100,int)==FIELD(pads[s[i]],0x100,int));
 fail_clone=0;
 copy_observer=NULL;
 /* Copy exit must run the real modal-stack removal and mode handoff on
  * the native worker. UI receiver callbacks are recorded adapters. */
 restore_entry(argv[1],0x1c0d990);restore_entry(argv[1],0x1c0d420);restore_entry(argv[1],0x1c09870);
 stub(0xb51828,native_property_try_lock);stub(0xb51808,native_property_lock);
 stub(0x13ae070,activate_mode);stub(0x1c0edb0,exit_signal);stub(0x1c0e850,exit_signal);stub(0x1c0ea20,exit_signal);stub(0x1c0ebf0,exit_signal);
 __builtin___clear_cache((char*)base,(char*)(base+0x7500000));
 phase="queued native Copy mode exit";init_copy_mode();
 assert(batch_copy_exit_begin(base,context)&&batch_copy_exit_poll()==BATCH_EXIT_PENDING);
 assert(!batch_copy_exit_begin(base,context));assert(copy_active&&!sample_active&&!activation_calls&&FIELD(context,0x50,unsigned)==1);
 worker_active=1;FN(0x2b59200,void(*)(void*,uint64_t*))(q,&queue_read);worker_active=0;
 assert(batch_copy_exit_poll()==BATCH_EXIT_DONE&&!copy_active&&sample_active&&activation_calls==2&&exit_signals);
 assert(FIELD(exit_mode,0x130,uintptr_t)==FIELD(exit_mode,0x138,uintptr_t));
 assert(FIELD(exit_mode,0xd8,int)==0x3080&&FIELD(exit_mode,0x148,int)==0x20a7&&FIELD(context,0x50,unsigned)==0);
 assert(dispatch_entered==3&&history_added==2);batch_copy_exit_reset();
 /* Cancel without any copy uses the same exit, no Undo command is added. */
 init_copy_mode();assert(batch_copy_exit_begin(base,context));worker_active=1;FN(0x2b59200,void(*)(void*,uint64_t*))(q,&queue_read);worker_active=0;
 assert(batch_copy_exit_poll()==BATCH_EXIT_DONE&&sample_active&&!copy_active&&dispatch_entered==3);batch_copy_exit_reset();
 init_copy_mode();FIELD(q,0x2d,unsigned char)=1;assert(!batch_copy_exit_begin(base,context));
 assert(batch_copy_exit_poll()==BATCH_EXIT_FAILED&&copy_active&&!sample_active&&FIELD(context,0x50,unsigned)==0);batch_copy_exit_reset();
 /* B2 entry chord can release native buttons without destroying the
  * modal host; only its two exact native release callsites use this guard. */
 init_copy_mode();batch_copy_hold_native_exit(1);
 assert(batch_copy_native_release_exit(exit_mode,0,0x30ed)==0x30ed&&copy_active&&!sample_active);
 batch_copy_hold_native_exit(0);worker_active=1;assert(batch_copy_native_release_exit(exit_mode,0,0x30ed)==0x30ed);worker_active=0;assert(!copy_active&&sample_active);
 /* Execute both exact Erase-up closures with their two BL sites redirected,
  * including native lifetime-counter release after the guarded call. */
 stub(0x14c1000,batch_copy_native_release_exit);
 const uintptr_t release_entries[2]={0x14c0f60,0x14c0fb0},release_calls[2]={0x14c0f84,0x14c0fd4};
 for(unsigned i=0;i<2;++i)FIELD((void*)base,release_calls[i],uint32_t)=0x94000000u|((0x14c1000-release_calls[i])/4);
 __builtin___clear_cache((char*)(base+0x14c0f60),(char*)(base+0x14c1020));
 for(unsigned i=0;i<2;++i)for(unsigned hold=0;hold<2;++hold){
  unsigned char reference[16]={0},erase_control[0x58]={0};void*closure[2]={reference,erase_control};
  FIELD(reference,8,unsigned)=1;FIELD(erase_control,0x50,void*)=exit_mode;
  init_copy_mode();batch_copy_hold_native_exit((int)hold);worker_active=1;
  FN(release_entries[i],void(*)(void*))(closure);worker_active=0;
  assert(!FIELD(reference,8,unsigned));assert(copy_active==(int)hold&&sample_active==!(int)hold);
 }
 batch_copy_hold_native_exit(0);
 puts("PASS both actual native Erase-release closures14c0f60/14c0fb0: only their BL calls guarded; Copy retained in Batch, normal exit outside Batch, native lifetime counters decremented");
 /* Audition goes to normal pad audio input on the same worker, with the
  * real native record-None predicate; backend audio calls are fixtures. */
 phase="queued encoder audition";init_copy_mode();FIELD(q,0x2d,unsigned char)=0;
 FIELD(mode_nodes[0],0x28,void*)=preview_playing_mode;FIELD(preview_playing_mode,0,uintptr_t)=base+0x6e10488;FIELD(preview_playing_mode,0x48,uintptr_t)=base+0x6e104e0;
 FIELD(preview_playing_mode,0x48+0xb0,void*)=preview_interaction;FIELD(preview_interaction,0,uintptr_t)=base+0x6d8d850;FIELD(preview_interaction,0x60,void*)=preview_input_data;
 FIELD(preview_input_data,0xd8,void*)=preview_engine;FIELD(preview_engine,0x768,void*)=preview_transport;FIELD(preview_transport,0x188,void*)=preview_record;
 stub(0x1e27360,audio_on);stub(0x1e270d0,audio_off);__builtin___clear_cache((char*)base,(char*)(base+0x7500000));
 assert(batch_copy_preview(base,context,65,1)==1&&!preview_on&&FIELD(context,0x50,unsigned)==1);
 worker_active=1;FN(0x2b59200,void(*)(void*,uint64_t*))(q,&queue_read);worker_active=0;
 assert(preview_on==1&&preview_pad_seen==65&&batch_copy_preview_status()==BATCH_PREVIEW_PLAYING&&dispatch_entered==3);
 assert(batch_copy_preview(base,context,65,0)==1);worker_active=1;FN(0x2b59200,void(*)(void*,uint64_t*))(q,&queue_read);worker_active=0;assert(preview_off==1&&batch_copy_preview_status()==BATCH_PREVIEW_IDLE);
 FIELD(preview_record,0x9e8,int)=1;assert(batch_copy_preview(base,context,65,1)==2&&batch_copy_preview_status()==BATCH_PREVIEW_RECORDING&&preview_on==1);
 FIELD(preview_record,0x9e8,int)=0;assert(batch_copy_preview(base,context,65,1)==1);FIELD(preview_record,0x9e8,int)=1;
 worker_active=1;FN(0x2b59200,void(*)(void*,uint64_t*))(q,&queue_read);worker_active=0;assert(preview_on==1&&batch_copy_preview_status()==BATCH_PREVIEW_RECORDING&&FIELD(context,0x50,unsigned)==0);
 assert(batch_copy_preview(base,context,65,0)==1);worker_active=1;FN(0x2b59200,void(*)(void*,uint64_t*))(q,&queue_read);worker_active=0;
 assert(preview_off==1); /* A blocked preview never owns another note's release. */
 puts("PASS queued audition: source65 preserved, no copy/Undo/current-pad setter, note-on/off on worker; actual record-None getter checked before enqueue and worker; audio backend remains a fixture");
 puts("PASS native Copy exit: actual EngineMode1c0d990 modal removal,1c0d420 mode handoff and1c09870 property change on native queue; Cancel and Done paths, no new command/Undo, rejected duplicate/stopped queue; mode activation/UI receivers remain adapters");
 puts("PASS actual CommandManager ctor19cf2b0 and dispatch19d0a00/transaction19cc1f0/command19c9a50; exact context68 manager self, four copies, retained one history entry and group Undo; history storage/notifications and instrument clones are adapters");
 puts("PASS actual native queue2b59300 copies action, runner2b59160 invokes it later; sequential four-copy job progress, source preparation/copy stages, context lifetime reference, duplicate start blocked and stopped queue cleanup; no new thread");
 return 0;
}
