#define main original_steps_main
#include "check_steps.c"
#undef main
static uintptr_t kept[2];
static int native_command_result;
static void*expected_manager;
static int run_real_command(void*p,uintptr_t*shared){
 assert(p==expected_manager);void*c=(void*)shared[0];
 if(kept[1])FN(0x123bca0,void(*)(void*))((void*)kept[1]);
 kept[0]=shared[0];kept[1]=shared[1];++FIELD((void*)kept[1],8,unsigned);
 native_command_result=((int(*)(void*))FIELD((void*)FIELD(c,0,uintptr_t),0x40,uintptr_t))(c);return native_command_result;
}

#include "src/batch_copy.h"
static void *pads[128],*source_vptr[40];
static unsigned char instruments_copy[128][0x2468],copy_program[0x2900],properties[0x1300],registry[0x20],project_item[0xbc0],context[0x100],copy_current[0x400],copy_factory[0x200];
static unsigned clones,deleted;
static int fail_clone;
static void(*copy_observer)(void);

static void*resolve_project(void*p,void*id,const void*key){(void)p;(void)id;(void)key;return project_item;}
static void*registry_id(void*p){return p;}
static void*cast_program(void*p,void*a,void*b,long hint){(void)a;(void)b;(void)hint;return p;}
struct info_pair{uintptr_t a,b;};
static struct info_pair instrument_info(void*p,int mode){(void)p;(void)mode;return(struct info_pair){0,0};}
static void*clone_instrument(void*p,int mode,const struct info_pair*info){(void)mode;(void)info;if(copy_observer)copy_observer();if(fail_clone&&++clones==(unsigned)fail_clone)return NULL;void*n=malloc(0x2468);assert(n);memcpy(n,p,0x2468);return n;}
static void free_instrument(void*p){if(p>= (void*)instruments_copy&&p<(void*)(instruments_copy+128))return;++deleted;free(p);}
static void set_instrument(void*p,int index,void*instrument,int selected){(void)selected;assert(p==copy_program&&index>=0&&index<128);void*n=malloc(0x2468);assert(n);memcpy(n,instrument,0x2468);free_instrument(pads[index]);pads[index]=n;}
static void set_key(void*p,int index,int note){assert(p==copy_program+0x16a8);FIELD(p,0x210+index*4,int)=note;}
static void refresh_copy(void*p,int index){assert(p==properties&&index>=0&&index<128);}
static void restore_entry(const char*path,uintptr_t off){FILE*f=fopen(path,"rb");assert(f);fseek(f,(long)off,SEEK_SET);assert(fread((void*)(base+off),1,16,f)==16);fclose(f);}
#ifndef BATCH_PROOF_MAIN
#define BATCH_PROOF_MAIN main
#endif
int BATCH_PROOF_MAIN(int argc,char**argv){assert(argc==2);struct sigaction a={0};a.sa_sigaction=fault;a.sa_flags=SA_SIGINFO;sigaction(SIGSEGV,&a,NULL);sigaction(SIGBUS,&a,NULL);sigaction(SIGILL,&a,NULL);load_elf(argv[1],0x1800000000ULL);
restore_entry(argv[1],0x123bca0);stub(0x19d0a00,run_real_command);stub(0x2925dc0,registry_id);stub(0x2926140,resolve_project);stub(0xb51d78,cast_program);stub(0x2836cd0,set_key);stub(0x290c470,refresh_copy);
source_vptr[1]=free_instrument;source_vptr[7]=instrument_info;source_vptr[14]=clone_instrument;
static uintptr_t program_vtable[0x230/8];program_vtable[0x220/8]=(uintptr_t)set_instrument;
FIELD(copy_program,0,void*)=program_vtable;FIELD(copy_program,0x2808,void*)=pads;FIELD(copy_program,0x2810,void*)=pads+128;
for(unsigned i=0;i<128;++i){pads[i]=instruments_copy[i];FIELD(pads[i],0,void*)=source_vptr;FIELD(pads[i],0x100,int)=(int)i+1000;FIELD(copy_program,0x16a8+(i+0x84)*4,int)=(int)i;}
FIELD(project_item,0xbb8,void*)=copy_program;FIELD(properties,0x1288,void*)=registry;FIELD(copy_current,0xb8,void*)=copy_program;FIELD(context,0x78,void*)=copy_current;FIELD(context,0xc8,void*)=properties;FIELD(context,0x68,void*)=copy_factory;
expected_manager=copy_factory;FIELD(copy_factory,0,uintptr_t)=base+0x6d97818;
/* A signal field deliberately looks unlike a dispatcher: old +60 routing
 * is rejected by the exact pointer assertion, not masked by this adapter. */
FIELD(copy_factory,0x60,void*)=(void*)0x60bad;
/* Exercise the production source callback from the exact stock callsite. */
stub(0x6d40000,batch_copy_source);uint32_t branch=0x94000000|((0x6d40000-0x19875ac)/4&0x3ffffff);FIELD((void*)base,0x19875ac,uint32_t)=branch;
__builtin___clear_cache((char*)base,(char*)(base+0x7500000));
FN(0xf535c0,void(*)(void*,const char*))(copy_program+0xdc8,"native-test-program");
unsigned src[3]={0,1,2},dst[3]={1,2,0};phase="native overlapping batch copy";
assert(batch_copy_submit(base,context,src,dst,3));
assert(FIELD(pads[1],0x100,int)==1000&&FIELD(pads[2],0x100,int)==1001&&FIELD(pads[0],0x100,int)==1002);
assert(FIELD(copy_program,0x16a8+0x84*4,int)==2&&FIELD(copy_program,0x16a8+0x85*4,int)==0&&FIELD(copy_program,0x16a8+0x86*4,int)==1);
void*cmd=(void*)kept[0];assert(((int(*)(void*))FIELD((void*)FIELD(cmd,0,uintptr_t),0x48,uintptr_t))(cmd)==0);
for(unsigned i=0;i<3;++i){assert(FIELD(pads[i],0x100,int)==(int)i+1000);assert(FIELD(copy_program,0x16a8+(i+0x84)*4,int)==(int)i);}
assert(((int(*)(void*))FIELD((void*)FIELD(cmd,0,uintptr_t),0x40,uintptr_t))(cmd)==0);
assert(FIELD(pads[2],0x100,int)==1001);
/* A failed snapshot must leave every destination untouched. */
unsigned original[3];for(unsigned i=0;i<3;++i)original[i]=(unsigned)FIELD(pads[i],0x100,int);
fail_clone=2;clones=0;assert(!batch_copy_submit(base,context,src,dst,3));assert(native_command_result==1);
for(unsigned i=0;i<3;++i)assert((unsigned)FIELD(pads[i],0x100,int)==original[i]);
fail_clone=0;
unsigned duplicate[2]={4,4};assert(!batch_copy_submit(base,context,src,duplicate,2));
assert(!batch_copy_submit(base,context,src,src,1));
unsigned many_src[17],many_dst[17];for(unsigned i=0;i<17;++i){many_src[i]=i;many_dst[i]=48+i;}
/* The core must enforce the UI's cap before touching any native state. */
assert(!batch_copy_submit(base,context,many_src,many_dst,17));
assert(batch_copy_submit(base,context,many_src,many_dst,BATCH_MAX_ITEMS));
for(unsigned i=0;i<BATCH_MAX_ITEMS;++i)assert(FIELD(pads[48+i],0x100,int)==FIELD(pads[i],0x100,int));
cmd=(void*)kept[0];assert(((int(*)(void*))FIELD((void*)FIELD(cmd,0,uintptr_t),0x48,uintptr_t))(cmd)==0);
for(unsigned i=0;i<BATCH_MAX_ITEMS;++i)assert(FIELD(pads[48+i],0x100,int)==(int)(1048+i));
puts("PASS actual native CopyPadCommand ctor/perform/undo through production source callback; cycle sources snapshotted; mapping restored; duplicate/self rejected; one group Undo/Redo");return 0;}
