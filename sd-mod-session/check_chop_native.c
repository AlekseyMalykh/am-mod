#define BATCH_PROOF_MAIN batch_proof_main
#include "check_batch_native.c"
#undef BATCH_PROOF_MAIN
static unsigned char extract_queue_owner[0x260],extract_action[40];
static int extract_queued,extract_worker;static void*expected_queues=(void*)0x66;
static void extract_queue_lock(void*p){assert(p==extract_queue_owner+0x220);}
static int extract_enqueue(void*p,void*action,uintptr_t tag){
 assert(p==extract_queue_owner+0x1d8&&!extract_queued&&tag==((uintptr_t)(context+0x48)&~(uintptr_t)3));
 memcpy(extract_action,action,40);extract_queued=1;return 1;
}
static unsigned char slice_a[0x368],slice_b[0x368],sample_native[0x200],sampledata[0x100],service_locator[0x60];
static uintptr_t slice_vector[16],sample_reference[4];
static void*sample_edit;
static unsigned native_source_requests,native_create_requests;
void empty_audio_source(void*sample,uintptr_t*out){
 assert(sample==sample_native);out[0]=out[1]=0;++native_source_requests;
}
void fail_sample_creation(void*functions,const uintptr_t*sample,const uintptr_t*generator,int add,int fresh,uintptr_t*out){
 assert(functions==sample_edit&&sample[0]==(uintptr_t)sample_native&&!generator[0]&&!generator[1]&&add==1&&fresh==1);
 out[0]=out[1]=0;++native_create_requests;
}
extern void check_empty_audio_source(void),check_fail_sample_creation(void);
static unsigned exported,exported_at[128],fail_extract_at,extract_attempts,partial_failure,partial_undone;
static unsigned bad_assignment_at;
static unsigned char export_inner[128][0xc8],export_samples[128][0x200],export_data[128][0x20],export_layers[128][0x600];
static uintptr_t export_refs[128][4];
static unsigned char partial_inner[0xc8];static uintptr_t partial_vtable[11];
static int undo_partial(void*p){assert(p==partial_inner&&FIELD(p,0xb8,void*));FIELD(p,0xb8,void*)=NULL;--exported;++partial_undone;return 0;}
static void*lookup_service(void*p,const uintptr_t*type){assert(p==service_locator&&*type==base+0x6e09000);return &sample_edit;}
static int service_lock(void*p){(void)p;return 0;}
void unique_name(void*p,const uint64_t*input,const uintptr_t*none,uint64_t*out){assert(p==(void*)0x1234&&!none[0]&&!none[1]);FN(0xf1c640,void(*)(void*,const void*))(out,input);}
extern void check_unique_name(void);
static void* slice_ctor(void*p,const uint64_t*name,int dst,void*slice,void*instrument,void*project,void*functions,void*loader,void*queues){
 memset(p,0,0xb0);assert(instrument==pads[0]&&project==properties&&functions==sample_edit&&loader==(void*)0x55&&queues==expected_queues);
 assert(FIELD(slice,0x78,int64_t)>=0&&FIELD(slice,0xd0,int64_t)>FIELD(slice,0x78,int64_t));assert(name&&*(const char**)(void*)name);
 FIELD(p,0,uintptr_t)=base+0x6e07640;FIELD(p,0x58,unsigned)=(unsigned)dst;FIELD(p,0x60,void*)=slice;return p;
}
static int extract_perform(void*p){
 if(extract_queued)assert(extract_worker);
 ++extract_attempts;if(fail_extract_at==extract_attempts){if(partial_failure){FIELD(p,0x98,void*)=partial_inner;FIELD(partial_inner,0,void*)=partial_vtable;partial_vtable[9]=(uintptr_t)undo_partial;FIELD(partial_inner,0xb8,void*)=(void*)1;++exported;}return 3;}
 void*slice=FIELD(p,0x60,void*);unsigned dst=FIELD(p,0x58,unsigned);assert(dst&&dst<128);
 exported_at[dst]=(unsigned)FIELD(slice,0x78,int64_t);FIELD(pads[dst],0x100,int)=2000+(int)exported_at[dst];
 FIELD(p,0x98,void*)=export_inner[dst];FIELD(export_inner[dst],0xb8,void*)=export_samples[dst];
 FIELD(export_samples[dst],0x130,void*)=export_data[dst];FIELD(export_data[dst],4,int)=(int)(FIELD(slice,0xd0,int64_t)-FIELD(slice,0x78,int64_t));
 FIELD(export_layers[dst],0x4a8,void*)=bad_assignment_at==extract_attempts?NULL:export_samples[dst];FIELD(export_layers[dst],0x4b0,void*)=export_refs[dst];FIELD(export_refs[dst],8,unsigned)=1000;
 FIELD(pads[dst],0x22e8,void*)=export_layers[dst];
 FIELD(p,0xa0,int)=1;++exported;return 0;
}
static int extract_undo(void*p){if(FIELD(p,0xa0,int)){--exported;FIELD(p,0xa0,int)=0;}return 0;}
static void extract_delete(void*p){free(p);}
static void chop_fault(int sig,siginfo_t*i,void*ctx){ucontext_t*u=ctx;fprintf(stderr,"CHOP_FAULT %d pc=%llx lr=%llx x0=%llx x1=%llx x2=%llx x3=%llx addr=%p\n",sig,u->uc_mcontext.pc,u->uc_mcontext.regs[30],u->uc_mcontext.regs[0],u->uc_mcontext.regs[1],u->uc_mcontext.regs[2],u->uc_mcontext.regs[3],i->si_addr);_exit(1);}
int main(int argc,char**argv){assert(argc==2);/* Establish pinned ELF, real CopyPadCommand and registry adapters. */assert(batch_proof_main(argc,argv)==0);struct sigaction a={0};a.sa_sigaction=chop_fault;a.sa_flags=SA_SIGINFO;sigaction(SIGSEGV,&a,NULL);
/* The old proof leaves the first pads permuted. Chops preserve that source. */
stub(0x143ad50,lookup_service);stub(0xb51828,service_lock);stub(0xb51808,service_lock);stub(0x292f0e0,check_unique_name);
stub(0x143ed40,slice_ctor);stub(0x143f0b0,extract_perform);stub(0x143ee90,extract_undo);FIELD((void*)base,0x6e07640+8,uintptr_t)=(uintptr_t)extract_delete;
__builtin___clear_cache((char*)base,(char*)(base+0x7500000));
FN(0x2949dd0,void(*)(void*,int64_t,int64_t,int64_t,int,int64_t))(slice_a,100,100,500,0,0);
FN(0x2949dd0,void(*)(void*,int64_t,int64_t,int64_t,int,int64_t))(slice_b,700,700,1000,0,0);
slice_vector[0]=(uintptr_t)slice_a;slice_vector[1]=(uintptr_t)slice_b;
FIELD(sample_native,0x130,void*)=sampledata;FIELD(sampledata,0x70,void*)=slice_vector;FIELD(sampledata,0x78,void*)=slice_vector+2;
unsigned char one_layer[0xb30]={0};FIELD(one_layer,0x4a8,void*)=sample_native;FIELD(one_layer,0x4b0,void*)=sample_reference;sample_reference[1]=1000;FIELD(pads[0],0x22e8,void*)=one_layer;
FIELD(properties,0x1278,void*)=(void*)0x1234;FIELD(context,0x70,void*)=(void*)0x55;FIELD(context,0x48,void*)=(void*)0x66;sample_edit=(void*)0x7777;
 FIELD(pads[0],0x2310,void*)=copy_program;FIELD(sampledata,4,int)=1600;
/* Execute the real outer constructor/destructor to pin the two service args,
 * stack ninth argument, owning references and empty inner-command fields. */
restore_entry(argv[1],0x143ed40);__builtin___clear_cache((char*)(base+0x143ed40),(char*)(base+0x143ed50));
void*outer=malloc(0xb0);uint64_t outer_name=0;FN(0xf535c0,void(*)(void*,const char*))(&outer_name,"native-extract-abi");
FN(0x143ed40,void(*)(void*,const uint64_t*,int,void*,void*,void*,void*,void*,void*))(outer,&outer_name,4,slice_a,pads[0],properties,sample_edit,(void*)0x55,(void*)0x66);
assert(FIELD(outer,0x58,unsigned)==4&&FIELD(outer,0x60,void*)==slice_a&&FIELD(outer,0x68,void*)==pads[0]);
assert(FIELD(outer,0x70,void*)==properties&&FIELD(outer,0x78,void*)==sample_edit&&FIELD(outer,0x80,void*)==(void*)0x55);
assert(FIELD(outer,0x88,void*)==(void*)0x1234&&FIELD(outer,0x90,void*)==(void*)0x66&&!FIELD(outer,0x98,void*)&&!FIELD(outer,0xa0,void*));
FN(0x143feb0,void(*)(void*))(outer);FN(0xf0a290,void(*)(void*))(&outer_name);
stub(0x143ed40,slice_ctor);__builtin___clear_cache((char*)(base+0x143ed40),(char*)(base+0x143ed50));
unsigned slices[2]={1,0},targets[2]={4,5};int before4=FIELD(pads[4],0x100,int),before5=FIELD(pads[5],0x100,int),source=FIELD(pads[0],0x100,int);
unsigned too_many[17]={0};assert(!batch_chop_submit(base,context,service_locator,0,too_many,too_many,17));
phase="chop snapshot metadata and transactional assignment";
#ifdef MPC_PAD_EQ
music_reset();assert(music_set_pad_colour(0,5)&&music_set_pad_colour(4,8)&&music_set_pad_colour(5,12));
#endif
assert(batch_chop_submit(base,context,service_locator,0,slices,targets,2));assert(exported==2&&exported_at[4]==700&&exported_at[5]==100);assert(FIELD(pads[0],0x100,int)==source);
#ifdef MPC_PAD_EQ
assert(music_pad_colour(4)==5&&music_pad_colour(5)==5);
#endif
assert(FIELD(slice_a,0x78,int64_t)==100&&FIELD(slice_b,0xd0,int64_t)==1000); /* source markers unchanged */
void*cmd=(void*)kept[0];assert(((int(*)(void*))FIELD((void*)FIELD(cmd,0,uintptr_t),0x48,uintptr_t))(cmd)==0);assert(!exported&&FIELD(pads[4],0x100,int)==before4&&FIELD(pads[5],0x100,int)==before5);
assert(((int(*)(void*))FIELD((void*)FIELD(cmd,0,uintptr_t),0x40,uintptr_t))(cmd)==0);assert(exported==2);
#ifdef MPC_PAD_EQ
assert(music_pad_colour(4)==5&&music_pad_colour(5)==5);
#endif
assert(((int(*)(void*))FIELD((void*)FIELD(cmd,0,uintptr_t),0x48,uintptr_t))(cmd)==0);assert(!exported);
#ifdef MPC_PAD_EQ
assert(music_pad_colour(4)==8&&music_pad_colour(5)==12);
#endif
extract_attempts=0;fail_extract_at=2;assert(!batch_chop_submit(base,context,service_locator,0,slices,targets,2));assert(!exported&&FIELD(pads[4],0x100,int)==before4&&FIELD(pads[5],0x100,int)==before5);
extract_attempts=0;partial_failure=1;assert(!batch_chop_submit(base,context,service_locator,0,slices,targets,2));assert(!exported&&partial_undone==1&&FIELD(pads[4],0x100,int)==before4&&FIELD(pads[5],0x100,int)==before5);partial_failure=0;
#ifdef MPC_PAD_EQ
assert(music_pad_colour(4)==8&&music_pad_colour(5)==12);
#endif
 fail_extract_at=0;bad_assignment_at=2;extract_attempts=0;
 assert(!batch_chop_submit(base,context,service_locator,0,slices,targets,2));
 assert(!exported&&FIELD(pads[4],0x100,int)==before4&&FIELD(pads[5],0x100,int)==before5);bad_assignment_at=0;
fail_extract_at=0;targets[0]=0;assert(!batch_chop_submit(base,context,service_locator,0,slices,targets,2));targets[0]=4;slices[0]=2;assert(!batch_chop_submit(base,context,service_locator,0,slices,targets,2));
 slices[0]=1;FIELD(slice_a,0xd0,int64_t)=1700;
 assert(!batch_chop_submit(base,context,service_locator,0,slices,targets,2)&&!exported);
 FIELD(slice_a,0xd0,int64_t)=500;
 unsigned char sixteen_slices[16][0x368];unsigned sixteen_indices[16],sixteen_targets[16];
 for(unsigned i=0;i<16;++i){FN(0x2949dd0,void(*)(void*,int64_t,int64_t,int64_t,int,int64_t))(sixteen_slices[i],i*30,i*30,i*30+20,0,0);
  slice_vector[i]=(uintptr_t)sixteen_slices[i];sixteen_indices[i]=i;sixteen_targets[i]=48+i;}
 FIELD(sampledata,0x78,void*)=slice_vector+16;
 assert(batch_chop_submit(base,context,service_locator,0,sixteen_indices,sixteen_targets,16)&&exported==16);
 cmd=(void*)kept[0];assert(((int(*)(void*))FIELD((void*)FIELD(cmd,0,uintptr_t),0x48,uintptr_t))(cmd)==0&&!exported);
 for(unsigned i=0;i<16;++i)FN(0x294a7f0,void(*)(void*))(sixteen_slices[i]);
 slice_vector[0]=(uintptr_t)slice_a;slice_vector[1]=(uintptr_t)slice_b;FIELD(sampledata,0x78,void*)=slice_vector+2;
 puts("PASS sixteen Chop rows with native Copy commands, frame-count and target validation, one group Undo; out-of-source range and silent assignment failure rejected");
/* Shared async dispatch must not perform PCM/extract in the UI handler.
 * Queue receiver is a fixture; metadata and Copy commands are native. */
slices[0]=1;expected_queues=extract_queue_owner;FIELD(context,0x48,void*)=extract_queue_owner;
stub(0xf40e50,extract_queue_lock);stub(0xf40e90,extract_queue_lock);stub(0x2b59300,extract_enqueue);
__builtin___clear_cache((char*)base,(char*)(base+0x7500000));
unsigned before_attempts=extract_attempts;
assert(batch_chop_begin(base,context,service_locator,0,slices,targets,2));
struct batch_copy_progress status;batch_copy_poll(&status);
assert(extract_queued&&status.phase==BATCH_JOB_PREPARE&&!status.done&&status.total==2&&extract_attempts==before_attempts);
assert(FIELD(context,0x50,unsigned)==1&&!batch_copy_release());
assert(!batch_chop_begin(base,context,service_locator,0,slices,targets,2));
extract_worker=1;((void(*)(void*))FIELD(extract_action,0,uintptr_t))(extract_action+16);extract_worker=0;
batch_copy_poll(&status);assert(status.phase==BATCH_JOB_DONE&&status.done==2&&exported==2&&!FIELD(context,0x50,unsigned));
assert(batch_copy_release());cmd=(void*)kept[0];assert(((int(*)(void*))FIELD((void*)FIELD(cmd,0,uintptr_t),0x48,uintptr_t))(cmd)==0&&!exported);
puts("PASS async Chop job: no extraction before worker, sequential two native Copy-plus-extract rows, progress, context lifetime, duplicate start blocked, Done acknowledgement and one Undo; queue and PCM remain fixtures");
/* Execute the actual ExtractSliceCommand and SampleEditFunctions pipeline
 * up to native Sample creation, instead of replacing the entire perform.
 * Mode1 deliberately uses the source generator directly (no crop wrapper).
 * Creation returns empty, so no crop/PCM/SamplePool success claim. */
restore_entry(argv[1],0x19aaae0);restore_entry(argv[1],0x209c5b0);
stub(0x2943250,check_empty_audio_source);stub(0x209b1f0,check_fail_sample_creation);
__builtin___clear_cache((char*)base,(char*)(base+0x7500000));
unsigned char actual_inner[0xc8]={0};uint64_t before_marker=FIELD(slice_a,0x78,uint64_t);
FIELD(actual_inner,0x50,void*)=sample_native;
FIELD(actual_inner,0x60,int64_t)=100;FIELD(actual_inner,0x68,int64_t)=500;
FN(0xf535c0,void(*)(void*,const char*))(actual_inner+0x98,"no-output-probe");
FIELD(actual_inner,0xa0,void*)=(void*)0x1234;FIELD(actual_inner,0xa8,int)=1;
FIELD(actual_inner,0xac,int)=24;FIELD(actual_inner,0xb0,void*)=sample_edit;
assert(FN(0x19aaae0,int(*)(void*))(actual_inner)==3);
assert(native_source_requests==1&&native_create_requests==1&&!FIELD(actual_inner,0xb8,void*)&&!FIELD(actual_inner,0xc0,void*));
assert(FIELD(slice_a,0x78,uint64_t)==before_marker&&!strcmp(*(const char**)(actual_inner+0x98),"no-output-probe"));
FN(0xf0a290,void(*)(void*))(actual_inner+0x98);
puts("PASS actual ExtractSliceCommand19aaae0 and SampleEditFunctions209c5b0 mode1 no-output failure: error3 propagated, no output assigned; audio source and failed Sample creation remain fixtures; crop wrapper, successful PCM and SamplePool untested");
puts("PASS native SliceData ctor/copy/dtor, original markers protected, slice mapping, native CopyPadCommands, grouped Undo/Redo and rollback after second extract failure; extraction/PCM and project persistence remain adapters requiring separate validation");return 0;}
