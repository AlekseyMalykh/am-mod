#define CONTEXT_PROOF_MAIN context_proof_main
#include "check_resample_context.c"
#include "src/resample_runtime.h"
#include <pthread.h>
#include <sched.h>
static int native_queue_pending(void){void*q=owner_queue+0x1d8;return FIELD(q,0x20,uint64_t)!=FIELD(q,0x18,uint64_t);}
static void runtime_tick(struct resample_runtime*r){
 if(native_queue_pending())FN(0x2b59200,void(*)(void*,uint64_t*))(owner_queue+0x1d8,&queue_cursor);
 resample_runtime_tick(r);
}
static void runtime_until(struct resample_runtime*r,enum resample_page page){unsigned loops=0;while(resample_runtime_page(r)!=page){assert(++loops<1000);runtime_tick(r);}}
static void runtime_closed(struct resample_runtime*r){unsigned loops=0;while(!resample_runtime_closed(r)){assert(++loops<1000);runtime_tick(r);}}
static void runtime_button(struct resample_runtime*r,unsigned k){assert(resample_runtime_button(r,0x6b+k,1)&&resample_runtime_button(r,0x6b+k,0));}
static _Atomic int owner_running;
static void*runtime_owner_thread(void*p){assert(p==owner_queue);
 void*q=owner_queue+0x1d8;
 while(atomic_load(&owner_running)){
  if(__atomic_load_n((uint64_t*)((char*)q+0x20),__ATOMIC_ACQUIRE)!=__atomic_load_n((uint64_t*)((char*)q+0x18),__ATOMIC_ACQUIRE))
   FN(0x2b59200,void(*)(void*,uint64_t*))(q,&queue_cursor);
  else sched_yield();
 }
 return NULL;
}
static void threaded_page(struct resample_runtime*r,enum resample_page page){unsigned spins=0;
 while(resample_runtime_page(r)!=page){resample_runtime_tick(r);sched_yield();assert(++spins<2000000);}
}
static void native_context_fields(void){
 memset(owner_context,0,sizeof(owner_context));init_queue(4);FN(0x1fd30f0,void(*)(void*))(owner_context);
 FIELD(owner_context,0,uintptr_t)=base+0x6e0dfd8;FIELD(owner_context,0x78,void*)=captured_things;FIELD(owner_context,0xc8,void*)=real_properties;
 FIELD(owner_context,0x68,void*)=history_manager;FIELD(owner_context,0x70,void*)=real_service;
 FIELD(captured_things,0xb8,void*)=real_program;FIELD(captured_things,0x60,void*)=(void*)1;FIELD(captured_things,0x5f8,int)=64;
}
#ifndef RUNTIME_PROOF_MAIN
#define RUNTIME_PROOF_MAIN main
#endif
int RUNTIME_PROOF_MAIN(int argc,char**argv){
 assert(!context_proof_main(argc,argv));phase="runtime creates and closes native worker on owner queue";
 source_refs=1;entries=enters=0;
 FN(0x292e310,void(*)(void*))(real_pool);FN(0xf535c0,void(*)(void*,const char*))(real_program+0xdc8,"runtime-project");
 uintptr_t original[2]={};new_sample("RuntimeSource",original);assert(pad_sample_pool_register(base,real_pool,original));
 FIELD(source_layers[0],0x4a8,uintptr_t)=original[0];FIELD(source_layers[0],0x4b0,uintptr_t)=original[1];FIELD(source_layers[0],0x7b8,int64_t)=0;FIELD(source_layers[0],0x810,int64_t)=16;
 for(unsigned i=0;i<8;++i)FIELD(commit_layers[i],0x418,int)=60+i;
 native_context_fields();struct resample_context*tracking=resample_context_create(base,owner_context);assert(tracking);
 struct resample_runtime*r=resample_runtime_create(base,owner_context,tracking);assert(r&&resample_runtime_closed(r));
 assert(resample_runtime_open(r)&&resample_runtime_page(r)==RS_PREPARING&&!resample_runtime_destroy(r));
 /* All UI calls above posted closure only; no native assignment or capture. */
 assert(!entries&&FIELD(owner_context,0x50,unsigned)==1);
 resample_runtime_close(r);runtime_closed(r);assert(!entries&&!FIELD(owner_context,0x50,unsigned));
 /* Queue-full/stopped before boot leaves a single request for UI timer retry. */
 FIELD(owner_queue+0x1d8,0x2d,unsigned char)=1;assert(resample_runtime_open(r));resample_runtime_tick(r);assert(resample_runtime_page(r)==RS_PREPARING&&!entries);
 FIELD(owner_queue+0x1d8,0x2d,unsigned char)=0;runtime_until(r,RS_CHOOSE);resample_runtime_close(r);runtime_closed(r);assert(!entries);
 /* Failure captures no worker and can close/reopen without a dangling pair. */
 FIELD(captured_things,0x5f8,int)=-1;assert(resample_runtime_open(r));runtime_until(r,RS_ERROR);assert(!entries);resample_runtime_close(r);runtime_closed(r);FIELD(captured_things,0x5f8,int)=64;
 /* Native sequence handoff request is retained through owner teardown. */
 assert(resample_runtime_open(r));runtime_until(r,RS_CHOOSE);runtime_button(r,1);runtime_closed(r);assert(resample_runtime_take_sequence(r)&&!resample_runtime_take_sequence(r)&&!entries);
 /* Start Sample, request FX through B2/B3; real host can now open editor. */
 assert(resample_runtime_open(r));runtime_until(r,RS_CHOOSE);runtime_button(r,2);runtime_until(r,RS_SAMPLE);
 runtime_button(r,1);assert(resample_runtime_page(r)==RS_FX);runtime_button(r,2);resample_runtime_tick(r);unsigned slot=99;assert(resample_runtime_take_fx_request(r,&slot)==RS_ACT_CHOOSE_FX&&slot==0);
 struct fx_chain_config chain={.count=1,.stage={{.kind=FX_STAGE_EQ}}};fx_eq_default(&chain.stage[0].eq);chain.stage[0].eq.band[1].gain_db=6;assert(resample_runtime_set_fx(r,&chain));
 runtime_button(r,0);assert(resample_runtime_page(r)==RS_SAMPLE);runtime_button(r,2);runtime_until(r,RS_READY);
 assert(!resample_runtime_button(r,0x51,1)&&!resample_runtime_button(r,0x51,0)&&!resample_runtime_button(r,0x52,1)&&!resample_runtime_button(r,0x31,1)&&!resample_runtime_button(r,0x31,0));
 assert(resample_runtime_knob(r,0,1)&&resample_runtime_knob(r,2,1));runtime_button(r,2);assert(resample_runtime_page(r)==RS_APPLYING);
 runtime_closed(r);assert(resample_runtime_applied(r)&&entries==1&&enters==1&&!FIELD(owner_context,0x50,unsigned));
 assert(!run_history(history_entry,9)&&!FIELD(commit_layers[0],0x4a8,void*));assert(!run_history(history_entry,8));assert(!run_history(history_entry,9));pad_assignment_release(base,history_entry);
 /* Owned button-up cannot leak into stock after Cancel closed the panel. */
 assert(resample_runtime_open(r));runtime_until(r,RS_CHOOSE);assert(resample_runtime_button(r,0x6b,1));runtime_closed(r);assert(resample_runtime_button(r,0x6b,0)&&!resample_runtime_applied(r));
 /* Real owner queue on a separate pthread: UI polls/sends only, no DSP/model
  * getters on the UI path. Repeated open/render/Cancel must fully join. */
 atomic_store(&owner_running,1);pthread_t owner;assert(!pthread_create(&owner,NULL,runtime_owner_thread,owner_queue));
 for(unsigned trial=0;trial<20;++trial){
  assert(resample_runtime_open(r));threaded_page(r,RS_CHOOSE);runtime_button(r,2);threaded_page(r,RS_SAMPLE);
  runtime_button(r,2);threaded_page(r,RS_READY);resample_runtime_close(r);unsigned spins=0;
  while(!resample_runtime_closed(r)){resample_runtime_tick(r);sched_yield();assert(++spins<2000000);}
  assert(!resample_runtime_applied(r)&&!FIELD(owner_context,0x50,unsigned));
 }
 atomic_store(&owner_running,0);assert(!pthread_join(owner,NULL));assert(entries==1);
 assert(resample_runtime_destroy(r)&&!FIELD(real_program,0x378,void*));resample_context_stop(tracking);resample_context_release(tracking);FN(0x1517c70,void(*)(void*))(owner_context);
 memset(source_layers,0,sizeof(source_layers));assert(pad_sample_pool_remove(base,real_pool,original));FN(0x29348b0,void(*)(void*))(real_pool);release_sample(original);FN(0xf0a290,void(*)(void*))(real_program+0xdc8);memory_release(pcm);
 puts("PASS UI runtime async-open -> real owner capture -> native worker construction -> panel/FX request/render/Apply -> owner teardown; cancel before capture, stopped queue retry, failed capture/reopen, Seq handoff, paired late button-up, Play/Stop/Shift passthrough, native single-entry Undo/Redo and cleanup with no queue-counter pin;20 open/render/Cancel cycles with native queue on a separate pthread; physical input/paint hooks and whole-device shutdown still uninstalled");return 0;
}
