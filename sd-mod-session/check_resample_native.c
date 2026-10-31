#define HISTORY_PROOF_MAIN history_proof_main
#include "check_pad_resample_history.c"
#include "src/resample_native.h"
#include "src/resample_queue.h"
#include "src/resample_panel.h"
static unsigned char owner_context[0x100],owner_queue[0x260],owner_slots[4][0xc0];
static unsigned char source_instrument[0x2468],source_layers[8][0xb30];
static struct resample_worker*queue_worker;
static struct resample_queue*queue_driver;
static unsigned owner_ticks,extra_wake,leases;
static uint64_t queue_cursor,after_event;
static int context_retain(void*p){assert(p==&project_generation);++leases;return 1;}
static void context_release(void*p){assert(p==&project_generation&&leases);--leases;}
static void native_mutex_fixture(void*p){(void)p;}
static void init_queue(unsigned size){
 memset(owner_queue,0,sizeof(owner_queue));memset(owner_slots,0,sizeof(owner_slots));queue_cursor=0;
 void*q=owner_queue+0x1d8;FIELD(q,0,uint64_t)=size;FIELD(q,0x30,void*)=owner_slots;
 for(unsigned i=0;i<size;++i){FIELD(owner_slots[i],0,uintptr_t)=base+0xb57a7c;FIELD(owner_slots[i],8,uintptr_t)=base+0x15265b0;}
 FIELD(owner_context,0x48,void*)=owner_queue;
}
static int owner_tick(void*p){
 assert(p==&owner_ticks);++owner_ticks;
 if(extra_wake){extra_wake=0;assert(resample_queue_wake(queue_driver));}
 resample_worker_step(queue_worker);return resample_worker_work_pending(queue_worker);
}
static void pump_one(void){assert(resample_queue_status(queue_driver)==1);FN(0x2b59200,void(*)(void*,uint64_t*))(owner_queue+0x1d8,&queue_cursor);}
static void drain_queue(void){unsigned n=0;while(resample_queue_status(queue_driver)==1){assert(++n<1000);pump_one();}assert(resample_queue_status(queue_driver)==0&&FIELD(owner_context,0x50,unsigned)==0);}
static struct resample_worker_event event(void){struct resample_worker_event e;assert(resample_worker_poll(queue_worker,&after_event,&e));return e;}
static void send_action(enum resample_action action,uint64_t token){
 struct resample_command c={.action=action,.token=token,.spec={.source=64,.destination=65,.repeats=1,.velocity=127,.revision=1}};
 if(action==RS_ACT_RENDER){c.spec.fx.count=1;c.spec.fx.stage[0].kind=FX_STAGE_EQ;fx_eq_default(&c.spec.fx.stage[0].eq);c.spec.fx.stage[0].eq.band[1].gain_db=6;}
 assert(resample_worker_send(queue_worker,&c)&&resample_queue_wake(queue_driver));
}
static unsigned two_ticks;
static int short_task(void*p){assert(p==&two_ticks);return ++two_ticks<2;}
static void panel_button(struct resample_panel*p,unsigned k){assert(resample_panel_button(p,0x6b+k,1)&&resample_panel_button(p,0x6b+k,0));}
static void panel_drain(struct resample_panel*p){unsigned loops=0;do{
 if(resample_queue_status(queue_driver)==1)pump_one();
 resample_panel_tick(p);assert(++loops<1000);
 }while(resample_queue_status(queue_driver)==1);
 assert(resample_queue_status(queue_driver)==0);
}
#ifndef NATIVE_PROOF_MAIN
#define NATIVE_PROOF_MAIN main
#endif
int NATIVE_PROOF_MAIN(int argc,char**argv){
 assert(!history_proof_main(argc,argv));phase="native owner queue -> worker -> RAM Sample -> history";
 source_refs=1;entries=enters=0;guard_ok=project_generation=1;
 FN(0x292e310,void(*)(void*))(real_pool);FN(0xf535c0,void(*)(void*,const char*))(real_program+0xdc8,"queued-resample");
 for(unsigned i=0;i<8;++i)FIELD(commit_layers[i],0x418,int)=60+i;
 struct pad_memory_services services={memory_queues,memory_storage,memory_gate};
 float*input=calloc(6000*2,sizeof(float));assert(input);for(unsigned i=0;i<6000;++i){input[i*2]=(float)sin(i*.03)*.1f;input[i*2+1]=-input[i*2];}
 struct pad_output_reference audio={pcm,input,6000,48000,memory_retain,memory_release};enum pad_render_status error;
 struct pad_memory_job*job=pad_memory_begin(base,&services,&audio,"QueueSource",0,&error);assert(job);while(pad_memory_step(job,4096)==PAD_RENDER_WORKING){}
 uintptr_t original[2]={};assert(pad_memory_take(job,original));pad_memory_destroy(job);free(input);assert(pad_sample_pool_register(base,real_pool,original));
 real_pads[64]=source_instrument;FIELD(source_instrument,0x2310,void*)=real_program;FIELD(source_instrument,0x22e8,void*)=source_layers;FIELD(source_instrument,0x22f0,void*)=source_layers+8;
 FIELD(source_layers[0],0x4a8,uintptr_t)=original[0];FIELD(source_layers[0],0x4b0,uintptr_t)=original[1];FIELD(source_layers[0],0x7b8,int64_t)=0;FIELD(source_layers[0],0x810,int64_t)=6000;
 struct pad_memory_services captured;assert(pad_memory_services_from_sample(base,original,&captured)&&captured.queue_context==services.queue_context&&captured.storage==services.storage&&captured.resources==services.resources);
 struct resample_native_config config={.commit={base,real_properties,real_program,real_service,real_pool,&project_generation,commit_guard,context_retain,context_release},.manager=history_manager,.hold_recovery=history_hold};
 struct resample_native*native=resample_native_create(&config);assert(native&&leases==1);
 struct resample_worker_backend backend;resample_native_backend(native,&backend);queue_worker=resample_worker_create(&backend);assert(queue_worker);
 init_queue(4);stub(0xf40e50,native_mutex_fixture);stub(0xf40e90,native_mutex_fixture);__builtin___clear_cache((char*)base,(char*)(base+0x7500000));
 queue_driver=resample_queue_create(base,owner_context,owner_tick,&owner_ticks);assert(queue_driver);
 send_action(RS_ACT_PREPARE,1);extra_wake=1;assert(!resample_queue_destroy(queue_driver));drain_queue();assert(owner_ticks==2&&event().update.reply==RS_REPLY_PREPARED);
 send_action(RS_ACT_RENDER,1);pump_one();assert(event().update.reply==RS_REPLY_PROGRESS);drain_queue();assert(event().update.reply==RS_REPLY_RENDERED&&!entries);
 send_action(RS_ACT_APPLY,1);pump_one();assert(!resample_native_destroy(native)&&!resample_worker_destroy(queue_worker));
 uint64_t done,total;resample_native_progress(native,&done,&total);assert(done==4096&&total==6000&&!entries);
 send_action(RS_ACT_CANCEL,1);drain_queue();assert(event().update.reply==RS_REPLY_CANCELLED&&!entries&&!FIELD(commit_layers[0],0x4a8,void*)&&leases==1);
 /* Close after native Sample preparation but before dispatcher entry. */
 send_action(RS_ACT_PREPARE,2);drain_queue();assert(event().update.reply==RS_REPLY_PREPARED);send_action(RS_ACT_RENDER,2);drain_queue();assert(event().update.reply==RS_REPLY_RENDERED);
 send_action(RS_ACT_APPLY,2);pump_one();pump_one();resample_native_progress(native,&done,&total);assert(done==total&&leases==3&&!entries);
 send_action(RS_ACT_CANCEL,2);drain_queue();assert(event().update.reply==RS_REPLY_CANCELLED&&leases==1&&!entries);
 /* Project change during copy is rejected before any pool/target mutation. */
 send_action(RS_ACT_PREPARE,3);drain_queue();(void)event();send_action(RS_ACT_RENDER,3);drain_queue();(void)event();
 send_action(RS_ACT_APPLY,3);pump_one();project_generation=2;drain_queue();assert(event().update.reply==RS_REPLY_FAILED&&!entries&&!FIELD(commit_layers[0],0x4a8,void*));
 project_generation=1;send_action(RS_ACT_CANCEL,3);drain_queue();assert(event().update.reply==RS_REPLY_CANCELLED);
 /* Built-in source Sample/Trim guard also rejects a changed source even when
  * the external project guard itself remains valid. */
 for(unsigned variant=0;variant<3;++variant){uint64_t token=5+variant;
  send_action(RS_ACT_PREPARE,token);drain_queue();(void)event();send_action(RS_ACT_RENDER,token);drain_queue();(void)event();send_action(RS_ACT_APPLY,token);pump_one();
  if(!variant)FIELD(source_layers[0],0x810,int64_t)=5999;else if(variant==1){FIELD(source_layers[0],0x4a8,uintptr_t)=0;FIELD(source_layers[0],0x4b0,uintptr_t)=0;}else FIELD(commit_layers[2],0x418,int)=99;
  drain_queue();assert(event().update.reply==RS_REPLY_FAILED&&!entries&&!FIELD(commit_layers[0],0x4a8,void*));
  if(variant==2)assert(FIELD(commit_layers[2],0x418,int)==99); /* Preserve the user's newer change. */
  FIELD(commit_layers[2],0x418,int)=62;FIELD(source_layers[0],0x810,int64_t)=6000;FIELD(source_layers[0],0x4a8,uintptr_t)=original[0];FIELD(source_layers[0],0x4b0,uintptr_t)=original[1];send_action(RS_ACT_CANCEL,token);drain_queue();assert(event().update.reply==RS_REPLY_CANCELLED);
 }
 /* Actual UI controller -> copied command -> native queue. Failed initial
  * wake stays queued exactly once; the UI timer retries without DSP/model IO. */
 struct resample_panel panel;resample_panel_init(&panel,queue_worker,queue_driver);
 assert(resample_panel_open(&panel,64,"QueueSource","Test sequence",1,NULL));
 assert(!resample_panel_button(&panel,0x51,1)&&!resample_panel_button(&panel,0x52,1));
 FIELD(owner_queue+0x1d8,0x2d,unsigned char)=1;panel_button(&panel,2);assert(panel.flow.page==RS_PREPARING&&resample_queue_status(queue_driver)==-1);
 FIELD(owner_queue+0x1d8,0x2d,unsigned char)=0;resample_panel_tick(&panel);panel_drain(&panel);assert(panel.flow.page==RS_SAMPLE);
 struct fx_chain_config chain={.count=1,.stage={{.kind=FX_STAGE_EQ}}};fx_eq_default(&chain.stage[0].eq);chain.stage[0].eq.band[1].gain_db=6;assert(resample_flow_set_fx(&panel.flow,&chain));
 panel_button(&panel,2);panel_drain(&panel);assert(panel.flow.page==RS_READY&&!strcmp(resample_flow_button_label(&panel.flow,1),""));
 assert(resample_panel_knob(&panel,0,1)&&resample_panel_knob(&panel,2,1)&&resample_flow_destination(&panel.flow)==65);
 panel_button(&panel,2);pump_one();resample_panel_close(&panel);assert(panel.flow.page==RS_CANCELLING&&!resample_panel_can_detach(&panel));panel_drain(&panel);
 assert(resample_panel_can_detach(&panel)&&!panel.flow.applied&&!entries&&leases==1);
 /* Success through the same panel, later retaining Undo after panel closes. */
 assert(resample_panel_open(&panel,64,"QueueSource","Test sequence",1,NULL));panel_button(&panel,2);panel_drain(&panel);assert(panel.flow.page==RS_SAMPLE);
 assert(resample_flow_set_fx(&panel.flow,&chain));panel_button(&panel,2);panel_drain(&panel);assert(panel.flow.page==RS_READY);
 assert(resample_panel_knob(&panel,0,1)&&resample_panel_knob(&panel,2,1));panel_button(&panel,2);panel_drain(&panel);
 assert(resample_panel_can_detach(&panel)&&panel.flow.applied&&entries==1&&enters==1&&leases==3);
 /* A late UI close must preserve the already completed Apply acknowledgement. */
 send_action(RS_ACT_CANCEL,panel.flow.serial);drain_queue();assert(event().update.reply==RS_REPLY_APPLIED&&entries==1&&enters==1&&leases==3);
 uintptr_t result[2]={FIELD(commit_layers[0],0x4a8,uintptr_t),FIELD(commit_layers[0],0x4b0,uintptr_t)};assert(result[0]&&result[0]!=original[0]&&pad_sample_pool_contains(base,real_pool,result));
 struct pad_render_stream reader;assert(pad_sample_stream_open(base,result,&reader)==PAD_RENDER_WORKING&&reader.frames==6000&&reader.rate==48000);float actual[16];assert(reader.read(reader.user,0,8,actual));reader.close(reader.user);assert(isfinite(actual[2])&&fabsf(actual[2]-(float)sin(.03)*.1f)>1e-7f&&actual[3]==-actual[2]);
 assert(resample_queue_destroy(queue_driver));queue_driver=NULL;assert(resample_worker_destroy(queue_worker));queue_worker=NULL;assert(resample_native_destroy(native)&&leases==2);
 assert(!run_history(history_entry,9)&&!FIELD(commit_layers[0],0x4a8,void*));assert(!run_history(history_entry,8));assert(!run_history(history_entry,9));pad_assignment_release(base,history_entry);assert(!leases&&!quarantined&&!FIELD(real_program,0x378,void*));
 /* A two-slot queue cannot accept its own continuation yet. It must return
  * retry status, not wait/execute inline while holding the producer mutex. */
 init_queue(2);struct resample_queue*small=resample_queue_create(base,owner_context,short_task,&two_ticks);assert(small&&resample_queue_wake(small));
 FN(0x2b59200,void(*)(void*,uint64_t*))(owner_queue+0x1d8,&queue_cursor);assert(two_ticks==1&&resample_queue_status(small)==-1&&!FIELD(owner_context,0x50,unsigned));
 assert(resample_queue_wake(small));FN(0x2b59200,void(*)(void*,uint64_t*))(owner_queue+0x1d8,&queue_cursor);assert(two_ticks==2&&resample_queue_status(small)==0);
 FIELD(owner_queue+0x1d8,0x2d,unsigned char)=1;assert(!resample_queue_wake(small)&&resample_queue_status(small)==-1&&!FIELD(owner_context,0x50,unsigned));assert(resample_queue_destroy(small));
 memset(source_layers,0,sizeof(source_layers));assert(pad_sample_pool_remove(base,real_pool,original));FN(0x29348b0,void(*)(void*))(real_pool);release_sample(original);FN(0xf0a290,void(*)(void*))(real_program+0xdc8);memory_release(pcm);
 puts("PASS UI controller -> actual native queue posts bounded worker ticks -> actual source reader/EQ renderer -> chunked RAM Sample -> one CommandManager history entry; timer retries rejected initial wake once, Play/Stop pass, cancelled UI waits for ack, pre-dispatch cancellation in both preparation phases, project/source/target-change rejection, late Cancel preserves Apply, no lost wake, full/stopped queue refuses without waiting, context counters balance, history lease outlives adapter/worker and supports Undo/Redo; actual device hooks/context guard/sampler/Save still unconnected");return 0;
}
