/* Real native AssignSampleCommand ctor/perform/undo/redo. SamplePool and
 * parameter receiver are explicit adapters; no device model or disk writes. */
#define CHOP_PCM_MAIN trim_assignment_main
#include "check_chop_pcm.c"
#include "src/pad_assignment.h"
static unsigned char assign_program[0x2900],assign_properties[0x1300],assign_pool[8],assign_service[8];
static unsigned char assign_instrument[0x2468],assign_layers[8][0xb30];
static unsigned char assign_samples[9][0x200],assign_metadata[9][0x400];
static uintptr_t assign_refs[9][4];static void*assign_pads[128];
static unsigned sample_keys[8],notifications,writes,hidden_sample;
static unsigned expected_pad=65;static unsigned sample_parameters[8],tune_parameters[8];
static unsigned refreshes;
static unsigned char assign_manager[0x200],assign_environment[0x100],assign_transaction[0x88];
static uintptr_t assign_environment_vtable[8],history_command[2];static unsigned dispatches,history_entries;
static int valid_assignment_context(void*p,void*flags,int mode){assert(p==assign_environment&&flags&&mode==3);return 1;}
static void assignment_monitor(void*p,void*manager,int enable){assert(p==assign_environment+8&&manager==assign_manager&&enable==1);}
static void assignment_enter(void*p){assert(p==assign_manager&&FIELD(p,0,uintptr_t)==base+0x6d97818);++dispatches;}
static void assignment_history_trim(void*p,uint64_t limit){assert(p==assign_manager);(void)limit;}
static void assignment_history_add(void*p,uintptr_t*command,int index){
 assert(p==assign_transaction&&index==0&&!history_command[0]);++history_entries;
 history_command[0]=command[0];history_command[1]=command[1];++FIELD((void*)command[1],8,unsigned);
}
static void restore_assignment_entry(const char*path,uintptr_t off){FILE*f=fopen(path,"rb");assert(f);assert(!fseek(f,(long)off,SEEK_SET));assert(fread((void*)(base+off),1,16,f)==16);fclose(f);}
void assignment_find(void*p,const uint64_t*key,uintptr_t*out){
 assert(p==assign_pool);out[0]=out[1]=0;const char*name=*(const char*const*)key;
 for(unsigned i=0;i<9;++i)if(i!=hidden_sample&&!strcmp(name,FIELD(assign_samples[i],0xa8,char*))){out[0]=(uintptr_t)assign_samples[i];out[1]=(uintptr_t)assign_refs[i];++FIELD(assign_refs[i],8,unsigned);return;}
}
static uint64_t assignment_index(void*p,const uint64_t*key){
 assert(p==assign_pool);
 for(unsigned i=0;i<9;++i)if(i!=hidden_sample&&key[0]==(uintptr_t)assign_samples[i])return (UINT64_C(1)<<32)|i;
 return 0;
}
static void assignment_parameter(void*p,int pad,int parameter,float value){
 assert(p==assign_program&&pad==(int)expected_pad);++writes;
 for(unsigned i=0;i<8;++i){
  if((unsigned)parameter==sample_parameters[i]){
   assert(value>=0&&value<=9);unsigned key=(unsigned)value;sample_keys[i]=key;
   FIELD(assign_layers[i],0x4a8,void*)=key?assign_samples[key-1]:NULL;
   FIELD(assign_layers[i],0x4b0,void*)=key?assign_refs[key-1]:NULL;return;
  }
  if((unsigned)parameter==tune_parameters[i])return;
 }
 assert(!"unexpected parameter written");
}
static void assignment_refresh(void*p,int pad){assert(p==assign_properties&&pad==(int)expected_pad);++notifications;++refreshes;}
extern void check_assignment_find(void);
static int perform(uintptr_t command[2]){void*p=(void*)command[0];return ((int(*)(void*))FIELD((void*)FIELD(p,0,uintptr_t),8*8,uintptr_t))(p);}
static int undo(uintptr_t command[2]){void*p=(void*)command[0];return ((int(*)(void*))FIELD((void*)FIELD(p,0,uintptr_t),9*8,uintptr_t))(p);}
int main(int argc,char**argv){
 assert(!trim_assignment_main(argc,argv));phase="native resample single-layer assignment";hidden_sample=99;
 stub(0x292ed50,check_assignment_find);stub(0x292eb00,assignment_index);stub(0x28c6790,assignment_parameter);stub(0x290c470,assignment_refresh);
 __builtin___clear_cache((char*)base,(char*)(base+0x7500000));
 FIELD(assign_program,0x2808,void*)=assign_pads;FIELD(assign_program,0x2810,void*)=assign_pads+128;assign_pads[expected_pad]=assign_instrument;
 FIELD(assign_instrument,0x22e8,void*)=assign_layers;FIELD(assign_instrument,0x22f0,void*)=assign_layers+8;FIELD(assign_instrument,0x2310,void*)=assign_program;
 FIELD(assign_properties,0x1278,void*)=assign_pool;FIELD(assign_properties,0x5f8,int)=127;FIELD(assign_properties,0x558,int)=7;
 FN(0xf535c0,void(*)(void*,const char*))(assign_program+0xdc8,"resample-test-program");
 for(unsigned i=0;i<9;++i){char name[32];snprintf(name,sizeof(name),"rendered-sample-%u",i);FN(0xf535c0,void(*)(void*,const char*))(assign_samples[i]+0xa8,name);
  FIELD(assign_samples[i],0x130,void*)=assign_metadata[i];FN(0x20c56d0,void(*)(void*,int))(assign_metadata[i],35);FN(0x20c56b0,void(*)(void*,int))(assign_metadata[i],2);FN(0x20c56f0,void(*)(void*,int))(assign_metadata[i],48000);FIELD(assign_refs[i],8,unsigned)=1000;
 }
 for(unsigned i=0;i<8;++i){sample_keys[i]=i+1;FIELD(assign_layers[i],0x4a8,void*)=assign_samples[i];FIELD(assign_layers[i],0x4b0,void*)=assign_refs[i];FIELD(assign_layers[i],0x418,int)=60+i;
  sample_parameters[i]=(unsigned)FN(0x278cd80,int(*)(int,int))(0x40d,(int)i);tune_parameters[i]=(unsigned)FN(0x278cd80,int(*)(int,int))(0x45e,(int)i);
 }
 uintptr_t sample[2]={(uintptr_t)assign_samples[8],(uintptr_t)assign_refs[8]},command[2]={};
 assert(pad_assignment_prepare(base,assign_properties,assign_program,assign_service,assign_pool,sample,expected_pad,0,command));
 assert(command[0]&&command[1]&&!writes&&!notifications);assert(FIELD((void*)command[0],0,uintptr_t)==base+0x6e414e0);
 assert(!perform(command)&&sample_keys[0]==9&&notifications==1);
 for(unsigned i=1;i<8;++i)assert(sample_keys[i]==i+1);
 assert(!undo(command)&&sample_keys[0]==1&&notifications==2);for(unsigned i=0;i<8;++i)assert(sample_keys[i]==i+1);
 assert(!perform(command)&&sample_keys[0]==9&&notifications==3);assert(!undo(command)&&sample_keys[0]==1);
 pad_assignment_release(base,command);assert(!command[0]&&!command[1]&&!FIELD(assign_program,0x378,void*));pad_assignment_release(base,command);
 unsigned before=writes;hidden_sample=8;
 assert(!pad_assignment_prepare(base,assign_properties,assign_program,assign_service,assign_pool,sample,expected_pad,0,command)&&!command[0]&&writes==before);
 hidden_sample=99;assert(!pad_assignment_prepare(base,assign_properties,assign_program,assign_service,assign_pool,sample,128,0,command));
 assert(!pad_assignment_prepare(base,assign_properties,assign_program,assign_service,assign_pool,sample,expected_pad,8,command));
 FN(0x20c56d0,void(*)(void*,int))(assign_metadata[8],0);
 assert(!pad_assignment_prepare(base,assign_properties,assign_program,assign_service,assign_pool,sample,expected_pad,0,command));
 FN(0x20c56d0,void(*)(void*,int))(assign_metadata[8],35);
 FIELD(assign_instrument,0x2310,void*)=NULL;
 assert(!pad_assignment_prepare(base,assign_properties,assign_program,assign_service,assign_pool,sample,expected_pad,0,command));
 FIELD(assign_instrument,0x2310,void*)=assign_program;
 assert(!pad_assignment_prepare(base,assign_properties,assign_program,assign_service,assign_pool,sample,0,0,command));assert(writes==before);
 /* Genuine CommandManager dispatch, validation and command execution.
  * Only history storage and model/audio notifications remain adapters. */
 phase="native CommandManager executes resample assignment";
 restore_assignment_entry(argv[1],0x19d0a00);stub(0x19d12e0,assignment_monitor);stub(0x19cd820,assignment_enter);
 stub(0x19cd250,assignment_history_trim);stub(0x19cbc30,assignment_history_add);
 __builtin___clear_cache((char*)base,(char*)(base+0x7500000));
 assign_environment_vtable[5]=(uintptr_t)valid_assignment_context;FIELD(assign_environment,0,void*)=assign_environment_vtable;
 FN(0x19cf2b0,void(*)(void*,void*,void*,uint64_t))(assign_manager,assign_environment,(void*)1,UINT64_MAX);
 FIELD(assign_manager,0x1b0,void*)=assign_transaction;
 assert(pad_assignment_prepare(base,assign_properties,assign_program,assign_service,assign_pool,sample,expected_pad,0,command));
 assert(!FN(0x19d0a00,int(*)(void*,const uintptr_t*))(assign_manager,command));
 assert(dispatches==1&&history_entries==1&&sample_keys[0]==9);
 pad_assignment_release(base,command);assert(history_command[0]&&!command[0]);
 assert(!undo(history_command)&&sample_keys[0]==1);assert(!perform(history_command)&&sample_keys[0]==9);assert(!undo(history_command)&&sample_keys[0]==1);
 pad_assignment_release(base,history_command);assert(!FIELD(assign_program,0x378,void*));
 for(unsigned i=0;i<9;++i)assert(FIELD(assign_refs[i],8,unsigned)==1000);
 for(unsigned i=0;i<9;++i)FN(0xf0a290,void(*)(void*))(assign_samples[i]+0xa8);
 FN(0xf0a290,void(*)(void*))(assign_program+0xdc8);
 puts("PASS real AssignSampleCommand1995f90/199a3b0/199af30 and CommandManager19cf2b0/19d0a00: prepare does not assign/change pad parameters, bank-E pad65 layer0 assignment, native Undo/Redo restores all eight original layer assignments, history retains command after caller release; other layers unaffected, unregistered/empty result, invalid pad/layer and mismatched instrument rejected, native subscription/shared ownership cleanup; SamplePool/history storage, parameter receiver, real audio/Apply queue remain adapters");
}
