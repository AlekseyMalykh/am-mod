#define MEMORY_PROOF_MAIN commit_memory_main
#include "check_pad_memory_sample.c"
#include "src/pad_resample_commit.h"
static unsigned char commit_layers[8][0xb30];
static unsigned commit_sample_parameters[8],commit_tune_parameters[8];
static unsigned commit_writes,commit_refreshes,guard_ok=1,fail_after_write,project_generation=1;
static void commit_parameter(void*p,int pad,int parameter,float value){
 assert(p==real_program&&pad==65);++commit_writes;
 for(unsigned i=0;i<8;++i){
  if((unsigned)parameter==commit_sample_parameters[i]){
   unsigned key=(unsigned)value;void*sample=NULL;void*ref=NULL;
   if(key){
    /* Audited292eb00: actual index vector begin/end at +38/+40. */
    uintptr_t*entries=FIELD(real_pool,0x38,uintptr_t*);
    uintptr_t*end=FIELD(real_pool,0x40,uintptr_t*);assert(key<=(unsigned)(end-entries)/2);
    sample=(void*)entries[(key-1)*2];ref=(void*)entries[(key-1)*2+1];
   }
   FIELD(commit_layers[i],0x4a8,void*)=sample;FIELD(commit_layers[i],0x4b0,void*)=ref;
   if(fail_after_write){unsigned mode=fail_after_write;fail_after_write=0;
    uintptr_t*entries=FIELD(real_pool,0x38,uintptr_t*);
    FIELD(commit_layers[i],0x4a8,uintptr_t)=mode==2?entries[0]:0;
    FIELD(commit_layers[i],0x4b0,uintptr_t)=mode==2?entries[1]:0;
   }
   return;
  }
  /* Audited199a32c..358 normalizes stored root-note as (value+2)/129. */
  if((unsigned)parameter==commit_tune_parameters[i]){FIELD(commit_layers[i],0x418,int)=(int)lroundf(value*129.f)-2;return;}
 }
 assert(!"unknown assignment parameter");
}
static void commit_refresh(void*p,int pad){assert(p==real_properties&&pad==65);++commit_refreshes;}
static int commit_guard(void*p,enum pad_commit_operation op){assert(p==&project_generation);(void)op;return guard_ok&&project_generation==1;}
static void release_sample(uintptr_t s[2]){pad_assignment_release(base,s);}
static void new_sample(const char*name,uintptr_t result[2]){
 struct pad_memory_services s={memory_queues,memory_storage,memory_gate};
 struct pad_output_reference audio={pcm,pcm,16,48000,memory_retain,memory_release};enum pad_render_status error;
 struct pad_memory_job*j=pad_memory_begin(base,&s,&audio,name,0,&error);assert(j);
 assert(pad_memory_step(j,16)==PAD_RENDER_DONE&&pad_memory_take(j,result));pad_memory_destroy(j);
}
static void rename_sample(uintptr_t sample[2],const char*name){
 uint64_t text=0;FN(0xf535c0,void(*)(void*,const char*))(&text,name);
 FN(0x13bf450,void(*)(void*,const uint64_t*))((char*)sample[0]+0x58,&text);FN(0xf0a290,void(*)(void*))(&text);
}
#ifndef COMMIT_PROOF_MAIN
#define COMMIT_PROOF_MAIN main
#endif
int COMMIT_PROOF_MAIN(int argc,char**argv){
 assert(!commit_memory_main(argc,argv));phase="combined native RAM result registration/assignment/Undo";
 source_refs=1;memset(real_pool,0,sizeof(real_pool));FN(0x292e310,void(*)(void*))(real_pool);
 FN(0xf535c0,void(*)(void*,const char*))(real_program+0xdc8,"commit-project");
 FIELD(real_instrument,0x22e8,void*)=commit_layers;FIELD(real_instrument,0x22f0,void*)=commit_layers+8;
 FIELD(real_properties,0x5f8,int)=127;FIELD(real_properties,0x558,int)=7;
 for(unsigned i=0;i<8;++i){commit_sample_parameters[i]=(unsigned)FN(0x278cd80,int(*)(int,int))(0x40d,(int)i);commit_tune_parameters[i]=(unsigned)FN(0x278cd80,int(*)(int,int))(0x45e,(int)i);FIELD(commit_layers[i],0x418,int)=60+i;}
 uintptr_t old[2]={},result[2]={},collision[2]={};new_sample("Original",old);new_sample("Rendered",result);
 assert(pad_sample_pool_register(base,real_pool,old));
 FIELD(commit_layers[0],0x4a8,uintptr_t)=old[0];FIELD(commit_layers[0],0x4b0,uintptr_t)=old[1];
 /* Non-target layer must survive every apply/undo cycle. */
 FIELD(commit_layers[3],0x4a8,uintptr_t)=old[0];FIELD(commit_layers[3],0x4b0,uintptr_t)=old[1];
 stub(0x28c6790,commit_parameter);stub(0x290c470,commit_refresh);__builtin___clear_cache((char*)base,(char*)(base+0x7500000));
 struct pad_commit_context c={base,real_properties,real_program,real_service,real_pool,&project_generation,commit_guard,NULL,NULL};
 struct pad_commit_context stale=c;stale.properties=stale.program=stale.pool=(void*)1;guard_ok=0;
 assert(!pad_commit_prepare(&stale,result,65,1));guard_ok=1; /* Guard before stale pointers. */
 assert(!pad_commit_prepare(&c,result,65,0)&&!commit_writes&&!pad_sample_pool_contains(base,real_pool,result));
 struct pad_resample_commit*j=pad_commit_prepare(&c,result,65,1);assert(j&&!commit_writes&&!pad_sample_pool_contains(base,real_pool,result));
 FIELD(real_program,0x2808,void*)=(void*)1;guard_ok=0;assert(!pad_commit_apply(j)&&!commit_writes);guard_ok=1;FIELD(real_program,0x2808,void*)=real_pads;
 FIELD(commit_layers[2],0x418,int)=99;assert(!pad_commit_apply(j)&&!commit_writes);FIELD(commit_layers[2],0x418,int)=62;
 /* Native registration reallocates the pool vector; fixture receiver reads
  * its current vector on every parameter write. */
 assert(pad_commit_apply(j)&&pad_commit_status(j)==PAD_COMMIT_APPLIED);
 assert(FIELD(commit_layers[0],0x4a8,uintptr_t)==result[0]&&FIELD(commit_layers[3],0x4a8,uintptr_t)==old[0]);
 assert(pad_sample_pool_contains(base,real_pool,result));assert(!pad_commit_apply(j));
 guard_ok=0;assert(!pad_commit_undo(j));guard_ok=1;
 assert(pad_commit_undo(j)&&pad_commit_status(j)==PAD_COMMIT_UNDONE&&!pad_sample_pool_contains(base,real_pool,result));
 assert(FIELD(commit_layers[0],0x4a8,uintptr_t)==old[0]&&FIELD(commit_layers[3],0x4a8,uintptr_t)==old[0]);
 new_sample("Rendered",collision);assert(pad_sample_pool_register(base,real_pool,collision));
 assert(pad_commit_apply(j)&&strcmp(FIELD((void*)result[0],0xa8,char*),FIELD((void*)collision[0],0xa8,char*)));
 assert(FIELD(commit_layers[0],0x4a8,uintptr_t)==result[0]);assert(pad_commit_undo(j));
 assert(pad_sample_pool_contains(base,real_pool,collision)&&!pad_sample_pool_contains(base,real_pool,result));
 assert(pad_commit_destroy(j));
 j=pad_commit_prepare(&c,result,65,1);assert(j&&pad_commit_apply(j));
 rename_sample(old,"RenamedOriginal");assert(!pad_commit_undo(j)&&pad_commit_status(j)==PAD_COMMIT_APPLIED);
 rename_sample(old,"Original");assert(pad_commit_undo(j)&&pad_commit_destroy(j));
 j=pad_commit_prepare(&c,result,65,1);assert(j&&pad_commit_apply(j));fail_after_write=1;
 assert(!pad_commit_undo(j)&&pad_commit_status(j)==PAD_COMMIT_RECOVERY_REQUIRED&&!pad_commit_destroy(j));
 assert(pad_sample_pool_contains(base,real_pool,result));guard_ok=0;assert(!pad_commit_recover(j));guard_ok=1;
 assert(!pad_commit_recover(j)); /* Host must restore the missing assignment. */
 FIELD(commit_layers[0],0x4a8,uintptr_t)=old[0];FIELD(commit_layers[0],0x4b0,uintptr_t)=old[1];
 assert(pad_commit_recover(j)&&pad_commit_status(j)==PAD_COMMIT_UNDONE&&!pad_sample_pool_contains(base,real_pool,result));assert(pad_commit_destroy(j));
 j=pad_commit_prepare(&c,result,65,1);assert(j);fail_after_write=1;
 assert(!pad_commit_apply(j)&&pad_commit_status(j)==PAD_COMMIT_READY&&!pad_sample_pool_contains(base,real_pool,result));
 assert(FIELD(commit_layers[0],0x4a8,uintptr_t)==old[0]&&FIELD(commit_layers[3],0x4a8,uintptr_t)==old[0]);assert(pad_commit_destroy(j));
 j=pad_commit_prepare(&c,result,65,1);assert(j);project_generation=2;assert(!pad_commit_apply(j));project_generation=1;assert(pad_commit_destroy(j));
 j=pad_commit_prepare(&c,result,65,1);assert(j&&pad_commit_apply(j)&&pad_commit_destroy(j));
 assert(pad_sample_pool_contains(base,real_pool,result)); /* History eviction keeps applied result. */
 memset(commit_layers,0,sizeof(commit_layers));assert(pad_sample_pool_remove(base,real_pool,result));assert(pad_sample_pool_remove(base,real_pool,old));assert(pad_sample_pool_remove(base,real_pool,collision));
 FN(0x29348b0,void(*)(void*))(real_pool);release_sample(old);release_sample(result);release_sample(collision);
 assert(!FIELD(real_program,0x378,void*));FN(0xf0a290,void(*)(void*))(real_program+0xdc8);memory_release(pcm);
 puts("PASS combined native RAM Sample/real SamplePool/AssignSampleCommand: prepare has no mutation, occupied target permission, target and project guards, Undo removes result and restores other layers, redo after name collision rebuilds name-bound command, original rename refuses unsafe Undo, invalid assignment postcondition rolls back, failed Undo retains ownership until explicit host restore, applied history eviction retains result; native history/UI/audio owner wiring still required");
 return 0;
}
