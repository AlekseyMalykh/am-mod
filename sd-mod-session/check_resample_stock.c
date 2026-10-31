/* Execute stock export wrappers/settings; deliberately stop at task submission.
 * No GUI, device, file render, sample assignment or fake audio success. */
#define main unused_eq_main
#include "check_fx_native.c"
#undef main
#include "src/resample_stock.h"
#include <pthread.h>
#define F(p,o,T) (*(T*)((char*)(p)+(o)))
static unsigned char things[0x700],sequence[0x500],runner[0xc0],engine[0x600];
static unsigned char path[8],callback[32],settings_copy[0x900];
static unsigned calls,kind,flags[2],rate,depth,pad;
static unsigned char mode[0x200],properties[0x1300],dummy_program[8],dummy_pool[8];
static unsigned starts,requested_target,other_thread,nested;
static unsigned transport_playing;
static unsigned transport_state(void*p){assert(p);return transport_playing;}
static unsigned observed,completed,invoked;
static unsigned name_test;static uintptr_t seq_name;
extern void stock_name_call(uintptr_t,uintptr_t*,const char*,const void*);
static void test_name(const char*expected){
 uint32_t insn=F((void*)base,0x153edc8,uint32_t);assert((insn&0xfc000000)==0x94000000);
 int32_t imm=(int32_t)(insn<<6)>>6;uintptr_t result=0;
 stock_name_call(base+0x153edc8+(int64_t)imm*4,&result,"Seq ",&seq_name);
 assert(result&&!strcmp((const char*)result,expected));FN(0xf0a290,void(*)(void*))(&result);
}
extern void stock_callback_init(void*),stock_callback_keep(void*),stock_callback_destroy(void*);
extern unsigned stock_callback_release(int);
extern unsigned stock_callback_invoke(void);
static void observer(void*u,int did_run){assert(u==&observed);++completed;invoked=(unsigned)did_run;}
static void patch(uintptr_t off,void*fn){assert(!mprotect((void*)((base+off)&~(uintptr_t)4095),4096,PROT_READ|PROT_WRITE|PROT_EXEC));uint32_t jump[]={0x58000050,0xd61f0200};memcpy((void*)(base+off),jump,8);memcpy((void*)(base+off+8),&fn,8);__builtin___clear_cache((char*)(base+off),(char*)(base+off+16));}
static void restore(const char*file,uintptr_t off){FILE*f=fopen(file,"rb");assert(f&&!fseek(f,(long)off,SEEK_SET));assert(fread((void*)(base+off),1,16,f)==16);fclose(f);__builtin___clear_cache((char*)(base+off),(char*)(base+off+16));}
static void wrapper_capture(void*r,void*p,void*t,unsigned source,unsigned option,unsigned a,unsigned b,unsigned hz,unsigned bits,void*cb){
 assert(r==runner&&p==path&&(t==things||t==properties)&&option==0&&cb);if(observed)stock_callback_keep(cb);else assert(cb==callback);++calls;kind=source;flags[0]=a;flags[1]=b;rate=hz;depth=bits;
}
static void submit_capture(void*r,void*s,unsigned seq,void*cb){assert(r==runner&&seq==7&&cb==callback);memcpy(settings_copy,s,sizeof(settings_copy));++calls;}
static void*unrelated_export(void*p){assert(!p);((void(*)(void*,void*,void*,unsigned,unsigned,unsigned,void*))F((void*)base,0x7167218,uintptr_t))(runner,path,things,0,44100,16,callback);assert(kind==2);return NULL;}
static void native_start_capture(void*behavior,unsigned target){
 assert(behavior==mode+0x48&&target==requested_target);++starts;
 if(name_test)test_name(name_test==1?"Kick Resample":"Seq A01");
 if(other_thread){pthread_t t;assert(!pthread_create(&t,NULL,unrelated_export,NULL));assert(!pthread_join(t,NULL));}
 if(nested)unrelated_export(NULL);
 ((void(*)(void*,void*,void*,unsigned,unsigned,unsigned,void*))F((void*)base,0x7167218,uintptr_t))(runner,path,properties,0,48000,24,callback);
}
#ifndef STOCK_PROOF_MAIN
#define STOCK_PROOF_MAIN main
#endif
int STOCK_PROOF_MAIN(int argc,char**argv){
 assert(argc==2);struct sigaction sa={.sa_sigaction=fault,.sa_flags=SA_SIGINFO};sigaction(SIGSEGV,&sa,NULL);sigaction(SIGILL,&sa,NULL);sigaction(SIGABRT,&sa,NULL);load(argv[1]);setbuf(stdout,NULL);
 phase="stock wrappers";patch(0x18c5a00,wrapper_capture);
 FN(0x18c62f0,void(*)(void*,void*,void*,unsigned,unsigned,unsigned,void*))(runner,path,things,99,48000,24,callback);
 assert(calls==1&&kind==2&&flags[0]==1&&flags[1]==1&&rate==48000&&depth==24);
 for(unsigned a=0;a<2;++a)for(unsigned b=0;b<2;++b){FN(0x18c63b0,void(*)(void*,void*,void*,unsigned,unsigned,unsigned,void*,unsigned,unsigned))(runner,path,things,0,44100,16,callback,a,b);assert(kind==3&&flags[0]==a&&flags[1]==b&&rate==44100&&depth==16);}
 puts("PASS actual stock Seq and single-pad export wrapper ABI, including both processing flags and stack arguments");
 restore(argv[1],0x18c5a00);patch(0x18c54d0,submit_capture);
 F(things,0x1c8,void*)=sequence;F(sequence,0x200,int)=7;F(sequence,0x480,int)=7;
 F(runner,0xb0,void*)=engine;
 FN(0xf535c0,void(*)(void*,const char*))(path,"/run/fixture-not-written.wav");
 /* Empty-program branch still constructs native settings and rejects no
  * sample silently: this fixture only proves settings, not a valid render. */
 phase="stock single-pad settings";
 for(pad=0;pad<128;++pad){F(things,0x500,unsigned)=pad;
  FN(0x18c63b0,void(*)(void*,void*,void*,unsigned,unsigned,unsigned,void*,unsigned,unsigned))(runner,path,things,0,48000,24,callback,1,1);
  assert(F(settings_copy,0x2c8,unsigned)==4&&F(settings_copy,0x690,unsigned)==pad);
  assert(F(settings_copy,0x218,unsigned)==48000&&F(settings_copy,0x1c0,unsigned)==24);
  assert(F(settings_copy,0x428,unsigned char)==1&&F(settings_copy,0x480,unsigned char)==1);
  assert(F(settings_copy,0x6e8,uintptr_t)==F(path,0,uintptr_t)); /* Initial submission settings ONLY. */
 }
 puts("PASS actual stock AudioExportSettings source=single instrument, all128 pad indexes, rate/depth and processing flags; task submission captured, audio/assignment NOT executed");
 phase="guarded stock dispatch adapter";
 assert(!resample_stock_install(0));uintptr_t original=F((void*)base,0x7167218,uintptr_t),song=F((void*)base,0x7167220,uintptr_t);assert(original==base+0x18c62f0);
 assert(resample_stock_install(base)&&resample_stock_install(base)&&!resample_stock_install(base+0x1000));assert(F((void*)base,0x7167220,uintptr_t)==song);assert(!strcmp((const char*)(base+0x45a10e0),"Resample"));
 F(mode,0,uintptr_t)=base+0x6e10598;F(mode,0x48,uintptr_t)=base+0x6e105f0;F(mode,0x90,uintptr_t)=base+0x6e10678;
 F(mode+0x48,0xb0,void*)=things;F(mode+0x48,0xe8,void*)=properties;F(properties,0x1278,void*)=dummy_pool;F(properties,0x1c8,void*)=sequence;
 F(properties,0xb8,void*)=dummy_program;F(things,0xb8,void*)=dummy_program;
 /* Capture the native entry, not a pretend complete app. The adapter's
  * wrapper runs; no settings RTTI is fabricated for this dummy Program. */
 patch(0x153ed50,native_start_capture);patch(0x18c5a00,wrapper_capture);
 F(properties,0x500,unsigned)=F(things,0x5f8,unsigned)=18;
 for(requested_target=0;requested_target<128;++requested_target){assert(resample_stock_start(base,mode,1,18,requested_target));assert(kind==3);}
 assert(starts==128);
 static uintptr_t transport_vtable[4];static uintptr_t transport_fixture;
 transport_vtable[3]=(uintptr_t)transport_state;transport_fixture=(uintptr_t)transport_vtable;
 F(mode+0x48,0x110,void*)=&transport_fixture;transport_playing=1;
 assert(!resample_stock_start(base,mode,1,18,4)&&starts==128);
 transport_vtable[3]=0;assert(!resample_stock_start(base,mode,1,18,4)&&starts==128);
 transport_vtable[3]=(uintptr_t)transport_state;transport_playing=0;requested_target=4;
 assert(resample_stock_start(base,mode,1,18,4)&&starts==129);
 F(mode+0x48,0x110,void*)=NULL;
 F(properties,0x500,unsigned)=19;assert(!resample_stock_start(base,mode,1,18,4)&&starts==129);F(properties,0x500,unsigned)=18;
 F(mode+0x48,0x80,unsigned)=1;assert(!resample_stock_start(base,mode,1,18,4));F(mode+0x48,0x80,unsigned)=0;
 assert(!resample_stock_start(base,mode+0x48,1,18,4));assert(!resample_stock_start(base,mode,1,128,4));assert(!resample_stock_start(base,mode,1,18,128));
 requested_target=2;assert(resample_stock_start(base,mode,0,18,2)&&kind==2);
 FN(0xf535c0,void(*)(void*,const char*))(&seq_name,"A01");test_name("Seq A01");
 name_test=1;assert(resample_stock_start_named_observed(base,mode,1,18,2,"Kick",NULL,NULL)&&kind==3);
 name_test=2;assert(resample_stock_start_named_observed(base,mode,0,18,2,"Ignored",NULL,NULL)&&kind==2);name_test=0;test_name("Seq A01");FN(0xf0a290,void(*)(void*))(&seq_name);
 puts("PASS scoped native filename callsite: Sample uses copied source name, Seq and unscoped native callers retain sequence name; native String allocation/destruction and indirect-result ABI execute");
 other_thread=1;assert(resample_stock_start(base,mode,1,18,2)&&kind==3);other_thread=0;
 nested=1;assert(resample_stock_start(base,mode,1,18,2)&&kind==3);nested=0;
 assert(!resample_stock_start(0,mode,1,18,2));
 stock_callback_init(callback);observed=1;
 assert(resample_stock_start_observed(base,mode,1,18,2,observer,&observed)&&!completed);
 assert(stock_callback_invoke()==1&&completed==1&&invoked);
 assert(stock_callback_release(0)==1&&completed==1); /* Retained native copy cannot delay completion or notify twice. */
 assert(resample_stock_start_observed(base,mode,1,18,2,observer,&observed)&&completed==1);
 assert(stock_callback_release(0)==1&&completed==2&&!invoked);
 stock_callback_destroy(callback);
 puts("PASS observed export: import callback runs first; retained copies do not delay completion; final disposal without invocation reports cancellation exactly once");
 FN(0xf0a290,void(*)(void*))(path);
 puts("PASS stock dispatch adapter: complete-mode/+48 base validation, selected source guard,128 destinations, Seq unchanged, unrelated concurrent/nested export unchanged; entry and task boundaries are fixtures");return 0;
}
