#define NATIVE_PROOF_MAIN native_proof_main
#include "check_resample_native.c"
#include "src/resample_context.h"
static unsigned char captured_things[0x700];
static void drop_config(struct resample_native_config*c){c->commit.release(c->commit.user);memset(c,0,sizeof(*c));}
#ifndef CONTEXT_PROOF_MAIN
#define CONTEXT_PROOF_MAIN main
#endif
int CONTEXT_PROOF_MAIN(int argc,char**argv){
 assert(!native_proof_main(argc,argv));phase="owner captures pinned app context and selection";
 source_refs=1;FN(0x292e310,void(*)(void*))(real_pool);FN(0xf535c0,void(*)(void*,const char*))(real_program+0xdc8,"captured-project");
 uintptr_t original[2]={},result[2]={};new_sample("CapturedSource",original);new_sample("CapturedResult",result);assert(pad_sample_pool_register(base,real_pool,original));
 FIELD(source_layers[0],0x4a8,uintptr_t)=original[0];FIELD(source_layers[0],0x4b0,uintptr_t)=original[1];FIELD(source_layers[0],0x7b8,int64_t)=0;FIELD(source_layers[0],0x810,int64_t)=16;
 for(unsigned i=0;i<8;++i)FIELD(commit_layers[i],0x418,int)=60+i;
 FIELD(commit_layers[3],0x4a8,uintptr_t)=original[0];FIELD(commit_layers[3],0x4b0,uintptr_t)=original[1];
 init_queue(4);FN(0x1fd30f0,void(*)(void*))(owner_context);FIELD(owner_context,0,uintptr_t)=base+0x6e0dfd8;FIELD(owner_context,0x78,void*)=captured_things;
 FIELD(owner_context,0xc8,void*)=real_properties;FIELD(owner_context,0x68,void*)=history_manager;FIELD(owner_context,0x70,void*)=real_service;
 /* Exercise the real CurrentThings pad setter; Program is absent only
  * during this standalone property proof, avoiding a fake RTTI object. */
 FIELD(captured_things,0xb8,void*)=NULL;
 FN(0x236f0d0,void(*)(void*))(captured_things+0x4b0);FN(0x236f0d0,void(*)(void*))(captured_things+0x5a8);
 FN(0x28f9d60,void(*)(void*,int))(captured_things,64);
 assert(FIELD(captured_things,0x500,int)==64&&FIELD(captured_things,0x5f8,int)==64);
 FIELD(captured_things,0xb8,void*)=real_program;FIELD(captured_things,0x60,void*)=(void*)1;
 struct resample_context*tracking=resample_context_create(base,owner_context);assert(tracking);
 struct resample_native_config config;struct resample_selection selected;
 assert(resample_context_capture(tracking,&config,&selected));
 assert(selected.pad==64&&!strcmp(selected.sample,"CapturedSource")&&selected.occupied[0]==0&&selected.occupied[1]==3);
 assert(config.commit.properties==real_properties&&config.commit.program==real_program&&config.commit.pool==real_pool&&config.commit.service==real_service&&config.manager==history_manager);
 assert(config.memory.queue_context==memory_queues&&config.memory.storage==memory_storage&&config.memory.resources==memory_gate);
 /* A hardware capture refusal must identify its boundary, without changing
  * the selected layer or starting a worker/assignment. */
 drop_config(&config);
 for(int invalid=-1;invalid<=128;invalid+=129){FIELD(captured_things,0x5f8,int)=invalid;
  assert(!resample_context_capture(tracking,&config,&selected)&&!strcmp(resample_context_error(tracking),"Selected pad not in program")&&!config.commit.user);}
 FIELD(captured_things,0x5f8,int)=64;
 real_pads[64]=NULL;assert(!resample_context_capture(tracking,&config,&selected)&&!strcmp(resample_context_error(tracking),"Selected pad unavailable"));real_pads[64]=source_instrument;
 FIELD(source_instrument,0x2310,void*)=NULL;assert(!resample_context_capture(tracking,&config,&selected)&&!strcmp(resample_context_error(tracking),"Selected pad unavailable"));FIELD(source_instrument,0x2310,void*)=real_program;
 real_pads[0]=source_instrument;assert(!resample_context_capture(tracking,&config,&selected)&&!strcmp(resample_context_error(tracking),"Selected pad not in program"));real_pads[0]=NULL;
 FIELD(source_layers[0],0x4a8,uintptr_t)=0;FIELD(source_layers[0],0x4b0,uintptr_t)=0;
 assert(!resample_context_capture(tracking,&config,&selected)&&!strcmp(resample_context_error(tracking),"Selected pad is empty")&&!selected.sample[0]&&!config.commit.user);
 FIELD(source_layers[0],0x4a8,uintptr_t)=original[0];FIELD(source_layers[0],0x4b0,uintptr_t)=original[1];
 void*storage=FIELD((void*)original[0],0x160,void*);FIELD((void*)original[0],0x160,void*)=NULL;
 assert(!resample_context_capture(tracking,&config,&selected)&&!strcmp(resample_context_error(tracking),"Sample services unavailable"));
 FIELD((void*)original[0],0x160,void*)=storage;
 assert(pad_sample_pool_remove(base,real_pool,original));
 assert(!resample_context_capture(tracking,&config,&selected)&&!strcmp(resample_context_error(tracking),"Sample absent from project pool"));
 assert(pad_sample_pool_register(base,real_pool,original));
 assert(resample_context_capture(tracking,&config,&selected));
 assert(!FIELD(owner_context,0x50,unsigned)&&config.commit.validate(config.commit.user,PAD_COMMIT_APPLY));
 /* Mismatched current model is rejected before captured Program/Pool use. */
 FIELD(captured_things,0xb8,void*)=(void*)1;assert(!config.commit.validate(config.commit.user,PAD_COMMIT_UNDO));FIELD(captured_things,0xb8,void*)=real_program;
 resample_context_changed(tracking);FIELD(owner_context,0x78,void*)=(void*)1;assert(!config.commit.validate(config.commit.user,PAD_COMMIT_APPLY));FIELD(owner_context,0x78,void*)=captured_things;drop_config(&config);
 assert(resample_context_capture(tracking,&config,&selected));
 /* Occupied is detected in ANY layer. Snapshot + native result/Undo in bank E. */
 uintptr_t command[2]={};assert(!pad_history_prepare(&config.commit,result,65,0,config.hold_recovery,command));
 assert(pad_history_prepare(&config.commit,result,65,1,config.hold_recovery,command));assert(!run_history(command,8)&&!run_history(command,9));
 assert(FIELD(commit_layers[3],0x4a8,uintptr_t)==original[0]&&!FIELD(owner_context,0x50,unsigned));
 /* Quarantine callback is a real preallocated tracker implementation, not
  * the earlier fixture. Native history may drop command before recovery. */
 assert(!run_history(command,8));fail_after_write=2;assert(run_history(command,9));pad_assignment_release(base,command);
 assert(resample_context_recovery_count(tracking)==1&&!resample_context_finish_recovery(tracking));
 FIELD(commit_layers[3],0x4a8,uintptr_t)=original[0];FIELD(commit_layers[3],0x4b0,uintptr_t)=original[1];
 /* Failed write is layer0, which originally was empty. */
 FIELD(commit_layers[0],0x4a8,uintptr_t)=0;FIELD(commit_layers[0],0x4b0,uintptr_t)=0;
 uintptr_t second[2]={};new_sample("CapturedSecond",second);
 assert(pad_history_prepare(&config.commit,second,65,1,config.hold_recovery,command));assert(!run_history(command,8));fail_after_write=2;
 assert(run_history(command,9));pad_assignment_release(base,command);assert(resample_context_recovery_count(tracking)==2);
 FIELD(commit_layers[0],0x4a8,uintptr_t)=0;FIELD(commit_layers[0],0x4b0,uintptr_t)=0;
 assert(resample_context_finish_recovery(tracking)&&resample_context_recovery_count(tracking)==1);
 assert(resample_context_finish_recovery(tracking)&&resample_context_recovery_count(tracking)==0&&!FIELD(real_program,0x378,void*));release_sample(second);
 drop_config(&config);
 /* App stopping with a history ref: tracker invalidates before touching
  * native context; it does not pin context+50 and force its destructor wait. */
 assert(resample_context_capture(tracking,&config,&selected));
 assert(pad_history_prepare(&config.commit,result,65,1,config.hold_recovery,command));
 resample_context_stop(tracking);assert(!FIELD(owner_context,0x50,unsigned));FN(0x1517c70,void(*)(void*))(owner_context);
 memset(owner_context,0xa5,sizeof(owner_context));assert(!config.commit.validate(config.commit.user,PAD_COMMIT_UNDO));
 resample_context_release(tracking);drop_config(&config);assert(run_history(command,8));pad_assignment_release(base,command);assert(!FIELD(real_program,0x378,void*));
 memset(source_layers,0,sizeof(source_layers));memset(commit_layers,0,sizeof(commit_layers));assert(pad_sample_pool_remove(base,real_pool,original));FN(0x29348b0,void(*)(void*))(real_pool);release_sample(original);release_sample(result);FN(0xf0a290,void(*)(void*))(real_program+0xdc8);memory_release(pcm);
 puts("PASS actual CurrentThings pad setter28f9d60 selects64 at5f8, unrelated60 poisoned; capture resolves native Program member with inverse/parent checks, rejects negative/128/missing/foreign/duplicate mapping, all-layer occupancy and Sample services; concrete generation/stop guards, Undo and quarantine, native CopyContext destruction with history alive and queue zero; actual device shutdown remains untested");return 0;
}
