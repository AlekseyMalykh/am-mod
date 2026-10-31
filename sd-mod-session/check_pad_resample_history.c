#define COMMIT_PROOF_MAIN commit_proof_main
#include "check_pad_resample_commit.c"
#include "src/pad_resample_history.h"
static unsigned char history_manager[0x200],history_environment[0x100],history_transaction[0x88];
static uintptr_t history_env_table[8],history_entry[2];static unsigned entries,enters;static void*quarantined;
static int history_context(void*p,void*flags,int mode){assert(p==history_environment&&flags&&mode==3);return 1;}
static void history_monitor(void*p,void*m,int on){assert(p==history_environment+8&&m==history_manager&&on==1);}
static void history_enter(void*p){assert(p==history_manager);++enters;}
static void history_trim(void*p,uint64_t limit){assert(p==history_manager);(void)limit;}
static void history_add(void*p,uintptr_t*cmd,int index){assert(p==history_transaction&&index==0&&!history_entry[0]);++entries;history_entry[0]=cmd[0];history_entry[1]=cmd[1];++FIELD((void*)cmd[1],8,unsigned);}
static void history_hold(void*p,void*command){assert(p==&project_generation&&!quarantined);quarantined=command;}
static int run_history(uintptr_t command[2],unsigned slot){void*p=(void*)command[0];return ((int(*)(void*))FIELD((void*)FIELD(p,0,uintptr_t),slot*8,uintptr_t))(p);}
static void restore_dispatch(const char*path){FILE*f=fopen(path,"rb");assert(f);assert(!fseek(f,0x19d0a00,SEEK_SET));assert(fread((void*)(base+0x19d0a00),1,16,f)==16);fclose(f);}
#ifndef HISTORY_PROOF_MAIN
#define HISTORY_PROOF_MAIN main
#endif
int HISTORY_PROOF_MAIN(int argc,char**argv){
 assert(!commit_proof_main(argc,argv));phase="one real native history entry for resample";
 source_refs=1;FN(0x292e310,void(*)(void*))(real_pool);FN(0xf535c0,void(*)(void*,const char*))(real_program+0xdc8,"history-project");
 for(unsigned i=0;i<8;++i)FIELD(commit_layers[i],0x418,int)=60+i;
 uintptr_t original[2]={},result[2]={};new_sample("OriginalHistory",original);new_sample("RenderedHistory",result);assert(pad_sample_pool_register(base,real_pool,original));
 FIELD(commit_layers[0],0x4a8,uintptr_t)=original[0];FIELD(commit_layers[0],0x4b0,uintptr_t)=original[1];
 struct pad_commit_context c={base,real_properties,real_program,real_service,real_pool,&project_generation,commit_guard,NULL,NULL};
 uintptr_t command[2]={};assert(pad_history_prepare(&c,result,65,1,history_hold,command));
 assert(!pad_sample_pool_contains(base,real_pool,result)&&FIELD(commit_layers[0],0x4a8,uintptr_t)==original[0]);
 restore_dispatch(argv[1]);stub(0x19d12e0,history_monitor);stub(0x19cd820,history_enter);stub(0x19cd250,history_trim);stub(0x19cbc30,history_add);
 __builtin___clear_cache((char*)base,(char*)(base+0x7500000));
 history_env_table[5]=(uintptr_t)history_context;FIELD(history_environment,0,void*)=history_env_table;
 FN(0x19cf2b0,void(*)(void*,void*,void*,uint64_t))(history_manager,history_environment,(void*)1,UINT64_MAX);FIELD(history_manager,0x1b0,void*)=history_transaction;
 assert(!FN(0x19d0a00,int(*)(void*,const uintptr_t*))(history_manager,command));
 assert(entries==1&&enters==1&&FIELD(commit_layers[0],0x4a8,uintptr_t)==result[0]&&pad_sample_pool_contains(base,real_pool,result));
 pad_assignment_release(base,command);assert(!command[0]&&history_entry[0]);
 assert(!run_history(history_entry,9)&&FIELD(commit_layers[0],0x4a8,uintptr_t)==original[0]&&!pad_sample_pool_contains(base,real_pool,result));
 assert(!run_history(history_entry,8)&&FIELD(commit_layers[0],0x4a8,uintptr_t)==result[0]);
 assert(!run_history(history_entry,9));pad_assignment_release(base,history_entry);assert(!quarantined&&!FIELD(real_program,0x378,void*));
 /* Explicit recovery handoff when native history drops a failed Undo. */
 assert(pad_history_prepare(&c,result,65,1,history_hold,command)&&!run_history(command,8));fail_after_write=1;
 assert(run_history(command,9));pad_assignment_release(base,command);assert(quarantined&&pad_sample_pool_contains(base,real_pool,result));
 assert(!pad_history_finish_recovery(quarantined));FIELD(commit_layers[0],0x4a8,uintptr_t)=original[0];FIELD(commit_layers[0],0x4b0,uintptr_t)=original[1];
 assert(pad_history_finish_recovery(quarantined));quarantined=NULL;assert(!pad_sample_pool_contains(base,real_pool,result)&&!FIELD(real_program,0x378,void*));
 memset(commit_layers,0,sizeof(commit_layers));assert(pad_sample_pool_remove(base,real_pool,original));FN(0x29348b0,void(*)(void*))(real_pool);
 release_sample(original);release_sample(result);FN(0xf0a290,void(*)(void*))(real_program+0xdc8);memory_release(pcm);
 puts("PASS actual CommandManager dispatches one Resample Pad command containing real SamplePool registration + native assignment; single retained history entry survives caller release, Undo/Redo restores assignments/pool, final cleanup releases native subscriptions, failed Undo hands ownership to required host recovery; history storage/parameter receivers are fixtures, device scheduling/audio/Save remain unconnected");return 0;
}
