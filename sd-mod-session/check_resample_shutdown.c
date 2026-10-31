#define RUNTIME_PROOF_MAIN runtime_proof_main
#include "check_resample_runtime.c"
enum close_stage { READING,PROCESSING,RAM_COPY,BEFORE_DISPATCH };
static _Atomic unsigned stop_entered,stop_release,stop_calls;
static int stopped_tick(void*p){assert(p==&stop_calls);atomic_fetch_add(&stop_calls,1);return 1;}
static int in_flight_tick(void*p){
 assert(p==&stop_calls);atomic_fetch_add(&stop_calls,1);atomic_store(&stop_entered,1);
 while(!atomic_load(&stop_release))sched_yield();
 return 1;
}
static void stop_hook(void*p){resample_queue_stop(p);}
static void*one_owner_tick(void*p){assert(p==owner_queue);FN(0x2b59200,void(*)(void*,uint64_t*))(owner_queue+0x1d8,&queue_cursor);return NULL;}
static void context_stop_tests(void){
 native_context_fields();struct resample_context*c=resample_context_create(base,owner_context);assert(c);
 struct resample_queue*q=resample_queue_create(base,owner_context,stopped_tick,&stop_calls);assert(q);
 struct resample_context_hook hook={stop_hook,q,NULL};assert(resample_context_add_hook(c,&hook));
 assert(resample_queue_wake(q)&&FIELD(owner_context,0x50,unsigned)==1);resample_context_stop(c);
 assert(!resample_queue_wake(q));one_owner_tick(owner_queue);
 assert(!atomic_load(&stop_calls)&&resample_queue_status(q)==-2&&!FIELD(owner_context,0x50,unsigned));
 resample_context_remove_hook(c,&hook);assert(resample_queue_destroy(q));FN(0x1517c70,void(*)(void*))(owner_context);resample_context_release(c);
 /* Stop need not wait for an in-flight reader. It prevents the callback's
  * next post; native destructor still owns joining its lifetime counter. */
 native_context_fields();c=resample_context_create(base,owner_context);assert(c);
 q=resample_queue_create(base,owner_context,in_flight_tick,&stop_calls);assert(q);hook=(struct resample_context_hook){stop_hook,q,NULL};assert(resample_context_add_hook(c,&hook));
 assert(resample_queue_wake(q));pthread_t owner;assert(!pthread_create(&owner,NULL,one_owner_tick,owner_queue));
 unsigned spins=0;while(!atomic_load(&stop_entered)){sched_yield();assert(++spins<2000000);}
 resample_context_stop(c);assert(!atomic_load(&stop_release)&&FIELD(owner_context,0x50,unsigned)==1);
 atomic_store(&stop_release,1);assert(!pthread_join(owner,NULL));
 assert(atomic_load(&stop_calls)==1&&resample_queue_status(q)==-2&&!native_queue_pending()&&!FIELD(owner_context,0x50,unsigned));
 FN(0x1517c70,void(*)(void*))(owner_context);memset(owner_context,0xa5,sizeof(owner_context));assert(!resample_queue_wake(q));
 resample_context_remove_hook(c,&hook);assert(resample_queue_destroy(q));resample_context_release(c);
}
static unsigned references(uintptr_t sample[2]){return FIELD((void*)sample[1],8,unsigned);}
static unsigned pool_size(void){return (unsigned)(FIELD(real_pool,0x40,uintptr_t*)-FIELD(real_pool,0x38,uintptr_t*))/2;}
static void long_source(uintptr_t sample[2]){
 float*input=calloc(6000*2,sizeof(float));assert(input);
 for(unsigned i=0;i<6000;++i){input[2*i]=(float)sin(i*.037)*.2f;input[2*i+1]=-input[2*i];}
 struct pad_memory_services services={memory_queues,memory_storage,memory_gate};
 struct pad_output_reference audio={pcm,input,6000,48000,memory_retain,memory_release};enum pad_render_status error;
 struct pad_memory_job*j=pad_memory_begin(base,&services,&audio,"ShutdownSource",0,&error);assert(j);
 while(pad_memory_step(j,4096)==PAD_RENDER_WORKING){}
 assert(pad_memory_take(j,sample));pad_memory_destroy(j);free(input);
}
static void render_open(struct resample_runtime*r){
 assert(resample_runtime_open(r));runtime_until(r,RS_CHOOSE);runtime_button(r,2);runtime_until(r,RS_SAMPLE);
 assert(resample_runtime_knob(r,0,3)); /* Four repeats, enough for partial DSP/copy. */
 struct fx_chain_config chain={.count=1,.stage={{.kind=FX_STAGE_EQ}}};fx_eq_default(&chain.stage[0].eq);chain.stage[0].eq.band[1].gain_db=3;
 assert(resample_runtime_set_fx(r,&chain));runtime_button(r,2);
}
static int is_stage(struct resample_runtime*r,enum close_stage stage){
 struct resample_runtime_progress p;resample_runtime_progress(r,&p);
 if(stage==READING)return p.page==RS_RENDERING&&p.source_done>0&&p.source_done<p.source_total;
 if(stage==PROCESSING)return p.page==RS_RENDERING&&p.source_done==6000&&p.done>0&&p.done<p.total;
 if(stage==RAM_COPY)return p.page==RS_APPLYING&&p.ram_done>0&&p.ram_done<p.ram_total;
 return p.page==RS_APPLYING&&p.ram_done&&p.ram_done==p.ram_total;
}
static void at_stage(struct resample_runtime*r,enum close_stage stage){
 if(stage>=RAM_COPY){runtime_until(r,RS_READY);assert(resample_runtime_knob(r,0,1)&&resample_runtime_knob(r,2,1));runtime_button(r,2);}
 unsigned n=0;while(!is_stage(r,stage)){assert(++n<1000);runtime_tick(r);}
 assert(!FIELD(commit_layers[0],0x4a8,void*));
}
static void unchanged(struct resample_runtime*r,uintptr_t original[2],unsigned refs,unsigned before){
 assert(resample_runtime_closed(r)&&!resample_runtime_applied(r)&&!FIELD(owner_context,0x50,unsigned));
 assert(!FIELD(commit_layers[0],0x4a8,void*)&&entries==before&&pool_size()==1&&references(original)==refs);
}
#ifndef SHUTDOWN_PROOF_MAIN
#define SHUTDOWN_PROOF_MAIN main
#endif
int SHUTDOWN_PROOF_MAIN(int argc,char**argv){
 assert(!runtime_proof_main(argc,argv));phase="active runtime cancellation and native history lifetime";
 source_refs=1;entries=enters=0;
 FN(0x292e310,void(*)(void*))(real_pool);FN(0xf535c0,void(*)(void*,const char*))(real_program+0xdc8,"shutdown-project");
 uintptr_t original[2]={};long_source(original);assert(pad_sample_pool_register(base,real_pool,original));
 FIELD(source_layers[0],0x4a8,uintptr_t)=original[0];FIELD(source_layers[0],0x4b0,uintptr_t)=original[1];FIELD(source_layers[0],0x7b8,int64_t)=0;FIELD(source_layers[0],0x810,int64_t)=6000;
 for(unsigned i=0;i<8;++i)FIELD(commit_layers[i],0x418,int)=60+i;
 native_context_fields();struct resample_context*tracking=resample_context_create(base,owner_context);assert(tracking);
 struct resample_runtime*r=resample_runtime_create(base,owner_context,tracking);assert(r);
 unsigned refs=references(original);
 /* Actual native queue, source reader, EQ, Sample preparation and history;
  * no mutation and every original reference returns after each early close. */
 for(enum close_stage stage=READING;stage<=BEFORE_DISPATCH;++stage){
  render_open(r);at_stage(r,stage);resample_runtime_close(r);runtime_closed(r);unchanged(r,original,refs,0);
 }
 /* Queue stops while DSP is active. Cancel remains pending; UI retries only
  * its wake after resume, not copied commands. No native blocking-post path. */
 render_open(r);at_stage(r,PROCESSING);FIELD(owner_queue+0x1d8,0x2d,unsigned char)=1;
 runtime_tick(r);resample_runtime_close(r);resample_runtime_tick(r);assert(!resample_runtime_closed(r));
 for(unsigned i=0;i<10;++i)resample_runtime_tick(r);
 FIELD(owner_queue+0x1d8,0x2d,unsigned char)=0;runtime_closed(r);unchanged(r,original,refs,0);
 /* If dispatch already won, close cannot turn its success into Cancel or
  * free result still retained by the real single-entry native history. */
 render_open(r);at_stage(r,BEFORE_DISPATCH);
 assert(native_queue_pending());FN(0x2b59200,void(*)(void*,uint64_t*))(owner_queue+0x1d8,&queue_cursor);assert(entries==1);
 resample_runtime_close(r);runtime_closed(r);assert(resample_runtime_applied(r)&&pool_size()==2&&FIELD(commit_layers[0],0x4a8,void*));
 assert(!run_history(history_entry,9)&&pool_size()==1&&!FIELD(commit_layers[0],0x4a8,void*));
 assert(!run_history(history_entry,8)&&pool_size()==2);assert(!run_history(history_entry,9));pad_assignment_release(base,history_entry);
 assert(pool_size()==1&&references(original)==refs);
 /* Project replacement is signalled before cached model pointers become
  * invalid; cancelling a native copy does not query poisoned CurrentThings. */
 render_open(r);at_stage(r,RAM_COPY);resample_context_changed(tracking);
 FIELD(captured_things,0xb8,void*)=(void*)1;resample_runtime_close(r);runtime_closed(r);
 FIELD(captured_things,0xb8,void*)=real_program;unchanged(r,original,refs,1);
 assert(resample_runtime_destroy(r));resample_context_stop(tracking);resample_context_release(tracking);
 FN(0x1517c70,void(*)(void*))(owner_context);assert(!FIELD(owner_context,0x50,unsigned));
 context_stop_tests();
 memset(source_layers,0,sizeof(source_layers));assert(pad_sample_pool_remove(base,real_pool,original));FN(0x29348b0,void(*)(void*))(real_pool);release_sample(original);FN(0xf0a290,void(*)(void*))(real_program+0xdc8);memory_release(pcm);
 puts("PASS active cancellation during source read, EQ DSP, partial RAM copy and prepared pre-dispatch command; stopped queue resume; late close after actual dispatch preserves Apply and Undo/Redo; project change before poisoned CurrentThings; native pool/target/original references unchanged after every cancelled job, owner cleanup acknowledged before native context destruction; lifetime hook skips stopped queued tick and prevents repost from in-flight pthread tick without waiting on it; wake refuses poisoned destroyed context");return 0;
}
