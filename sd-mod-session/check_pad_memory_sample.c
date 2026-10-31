/* Real Sample/MemorySample constructors, metadata, buffer allocation,
 * provider install, native reader and cleanup. Only services/gate are fixtures. */
#define CHOP_PCM_MAIN memory_trim_main
#include "check_chop_pcm.c"
#include "src/pad_memory_sample.h"
#include "src/pad_sample_source.h"
#include "src/pad_sample_pool.h"
#include "src/pad_assignment.h"
static unsigned char memory_queues[0x200],memory_storage[8],memory_gate[8];
static uintptr_t memory_queue_vtable[5],memory_gate_vtable[6];
static unsigned char real_pool[0x88],real_properties[0x1300],real_program[0x2900],real_instrument[0x2468],real_service[8];
static void*real_pads[128];
static unsigned source_refs,source_drops,deletes,gates,deny_gate;
static void memory_stack_failed(void){fprintf(stderr,"native stack guard fail return=%lx queues=%u gates=%u\n",(unsigned long)__builtin_return_address(0)-base,deletes,gates);_exit(1);}
static int memory_retain(void*p){assert(p==pcm&&source_refs);++source_refs;return 1;}
static void memory_release(void*p){assert(p==pcm&&source_refs);if(!--source_refs)++source_drops;}
static uintptr_t memory_no_thread(unsigned key){(void)key;return 0;}
static int memory_queue(void*p,void*action,uintptr_t tag){
 (void)tag;assert(p==memory_queues+0x30);++deletes;
 ((void(*)(void*))FIELD(action,0,uintptr_t))((char*)action+16);return 1;
}
static int memory_reserve(void*p,int frames,int channels,int size){assert(p==memory_gate&&frames>0&&channels==2&&size==4);++gates;return !deny_gate;}
static int memory_permission(void*p,uint64_t flags,int mode){(void)flags;assert(p==memory_gate&&mode==3);return !deny_gate;}
static void read_memory_sample(uintptr_t sample[2],const float*expected,unsigned frames){
 struct pad_render_stream reader;assert(pad_sample_stream_open(base,sample,&reader)==PAD_RENDER_WORKING);
 assert(reader.frames==frames&&reader.rate==48000);float data[512];
 for(unsigned done=0;done<frames;){unsigned n=frames-done;if(n>256)n=256;assert(reader.read(reader.user,done,n,data));
  for(unsigned i=0;i<n*2;++i)assert(data[i]==expected[done*2+i]);
  done+=n;
 }
 reader.close(reader.user);
}
#ifndef MEMORY_PROOF_MAIN
#define MEMORY_PROOF_MAIN main
#endif
int MEMORY_PROOF_MAIN(int argc,char**argv){
 assert(!memory_trim_main(argc,argv));phase="real RAM Sample construction and reader";
 stub(0xb53508,memory_no_thread);stub(0xb51488,memory_stack_failed);__builtin___clear_cache((char*)base,(char*)(base+0x7500000));
 memory_queue_vtable[2]=(uintptr_t)memory_queue;FIELD(memory_queues,0x30,void*)=memory_queue_vtable;
 memory_gate_vtable[4]=(uintptr_t)memory_reserve;memory_gate_vtable[5]=(uintptr_t)memory_permission;FIELD(memory_gate,0,void*)=memory_gate_vtable;
 struct pad_memory_services services={memory_queues,memory_storage,memory_gate};
 source_refs=1;struct pad_output_reference audio={pcm,pcm,16,48000,memory_retain,memory_release};enum pad_render_status error;
 struct pad_memory_job*j=pad_memory_begin(base,&services,&audio,"Resample-Probe",0,&error);assert(j&&error==PAD_RENDER_WORKING&&source_refs==2&&gates==1);
 uintptr_t result[2]={};assert(!pad_memory_take(j,result));uint64_t done,total;pad_memory_progress(j,&done,&total);assert(!done&&total==16);
 assert(pad_memory_step(j,7)==PAD_RENDER_WORKING);pad_memory_progress(j,&done,&total);assert(done==7&&!pad_memory_take(j,result));
 assert(pad_memory_step(j,4097)==PAD_RENDER_DONE&&source_refs==1);assert(pad_memory_take(j,result)&&result[0]&&result[1]);assert(!pad_memory_take(j,(uintptr_t[2]){0}));pad_memory_destroy(j);
 void*s=(void*)result[0];assert(FIELD(s,0,uintptr_t)==base+0x6dbd868&&FIELD((void*)FIELD(s,0x170,uintptr_t),0,uintptr_t)==base+0x6dbd5f0);
 assert(!strcmp(FIELD(s,0xa8,const char*),"Resample-Probe"));assert(FN(0x2941f80,int(*)(void*))(s)==16&&FN(0x2941f70,int(*)(void*))(s)==2&&FN(0x2941f90,int(*)(void*))(s)==48000);
 phase="actual SamplePool registers generated RAM Sample";FN(0x292e310,void(*)(void*))(real_pool);
 assert(!pad_sample_pool_contains(base,real_pool,result)&&pad_sample_pool_register(base,real_pool,result)&&pad_sample_pool_contains(base,real_pool,result));
 FIELD(real_properties,0x1278,void*)=real_pool;FIELD(real_program,0x2808,void*)=real_pads;FIELD(real_program,0x2810,void*)=real_pads+128;real_pads[65]=real_instrument;FIELD(real_instrument,0x2310,void*)=real_program;
 FN(0xf535c0,void(*)(void*,const char*))(real_program+0xdc8,"memory-project");uintptr_t command[2]={};
 assert(pad_assignment_prepare(base,real_properties,real_program,real_service,real_pool,result,65,0,command));pad_assignment_release(base,command);
 assert(pad_sample_pool_remove(base,real_pool,result)&&!pad_sample_pool_contains(base,real_pool,result));
 FN(0x29348b0,void(*)(void*))(real_pool);FN(0xf0a290,void(*)(void*))(real_program+0xdc8);
 read_memory_sample(result,pcm,16);FN(0x123bca0,void(*)(void*))((void*)result[1]);
 assert(deletes>0&&source_refs==1);memory_release(pcm);assert(source_drops==1);
 puts("PASS real native Sample factory/metadata and MemorySample storage -> provider -> actual native PCM reader; actual SamplePool ctor/register/name lookup/index/remove and assignment builder accepts this generated Sample; bounded preparation, independent ownership and deferred provider cleanup; services remain adapters, no device sampler/complete Apply/file persistence");
 return 0;
}
