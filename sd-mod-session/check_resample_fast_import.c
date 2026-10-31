/* The production callback guard runs. Native submission and the fast error
 * publisher are fixtures; this does not prove a device export. */
#define STOCK_PROOF_MAIN stock_proof_not_run
#include "check_resample_stock.c"
#include "src/resample_fast_native.h"
extern void stock_callback_init_action(void*,void(*)(void));
static unsigned assignments;
static unsigned char export_services[0x700];
static const char*fast_error;
static unsigned ready;
int resample_fast_native_install(uintptr_t b){(void)b;assert(!"Render must not install live patches");return 0;}
int resample_fast_native_ready(uintptr_t b){assert(b==base);return (int)ready;}
int resample_fast_native_arm(void*s,unsigned source,const char*p){assert(s==export_services&&s!=engine&&source==18&&p&&p[0]=='/');return 1;}
extern int output_callback_capture(const char*,const char*);
const char*resample_fast_native_error(void){return fast_error;}
static void assigned(void){++assignments;}
static void small_wav(const char*file,int audible){
 unsigned char b[48]={0};memcpy(b,"RIFF",4);b[4]=40;memcpy(b+8,"WAVEfmt ",8);b[16]=16;b[20]=1;b[22]=1;b[24]=0x44;b[25]=0xac;b[28]=0x88;b[29]=0x58;b[30]=1;b[32]=2;b[34]=16;memcpy(b+36,"data",4);b[40]=4;b[45]=audible?32:0;
 FILE*f=fopen(file,"wb");assert(f&&fwrite(b,1,sizeof(b),f)==sizeof(b)&&!fclose(f));
}
int main(int argc,char**argv){
 assert(argc==2);struct sigaction sa={.sa_sigaction=fault,.sa_flags=SA_SIGINFO};sigaction(SIGSEGV,&sa,NULL);sigaction(SIGILL,&sa,NULL);sigaction(SIGABRT,&sa,NULL);load(argv[1]);phase="production result import guard";
 assert(resample_stock_install(base));patch(0x153ed50,native_start_capture);patch(0x18c5a00,wrapper_capture);
 F(mode,0,uintptr_t)=base+0x6e10598;F(mode,0x48,uintptr_t)=base+0x6e105f0;F(mode,0x90,uintptr_t)=base+0x6e10678;
 F(mode+0x48,0xb0,void*)=things;F(mode+0x48,0xe8,void*)=properties;F(properties,0x1278,void*)=dummy_pool;F(properties,0x1c8,void*)=sequence;
 F(properties,0xb8,void*)=dummy_program;F(things,0xb8,void*)=dummy_program;F(things,0x1c8,void*)=sequence;F(properties,0x500,unsigned)=F(things,0x5f8,unsigned)=18;
 requested_target=19;observed=1;stock_callback_init_action(callback,assigned);
 F(runner,0xb0,void*)=export_services;
 assert(!resample_stock_start_named_observed(base,mode,1,18,19,"Fixture",observer,&observed)&&!starts);
 ready=1;
 char output[]="/tmp/mpc-fast-guard-XXXXXX.wav";int fd=mkstemps(output,4);assert(fd>=0);close(fd);FN(0xf535c0,void(*)(void*,const char*))(path,output);
 for(unsigned attempt=0;attempt<4;++attempt){
  if(attempt==0)unlink(output);else small_wav(output,attempt!=1);
  fast_error=attempt==3?"Effect tail did not decay within render budget":NULL;
  assert(resample_stock_start_named_observed(base,mode,1,18,19,"Fixture",observer,&observed));
  unsigned previous=completed;assert(stock_callback_invoke()==(unsigned)(attempt>=2)&&completed==previous+1);
  assert(invoked==(attempt==2)&&assignments==(unsigned)(attempt>=2));
  assert((resample_stock_error()!=NULL)==(attempt!=2));
  assert(stock_callback_invoke()==(unsigned)(attempt>=2)&&completed==previous+1);
  assert(stock_callback_release(0)==(unsigned)(attempt>=2)&&completed==previous+1);
 }
 /* Device r20f: audible sibling stem but the requested WAV contains zero.
  * Real finalization helper must run before the production import guard. */
 small_wav(output,0);char stem[256];assert(snprintf(stem,sizeof(stem),"%.*s-Track-001-Drum 001.wav",(int)strlen(output)-4,output)>0);small_wav(stem,1);
 assert(output_callback_capture(output,stem));unlink(stem);fast_error=NULL;
 assert(resample_stock_start_named_observed(base,mode,1,18,19,"Fixture",observer,&observed));
 assert(stock_callback_invoke()==2&&invoked&&assignments==2&&!resample_stock_error());
 assert(stock_callback_invoke()==2&&assignments==2);stock_callback_release(0);
 /* Seq remains native: its deliberately missing fixture file is diagnostic
  * only; Sample's result gate must not suppress the user's Seq callback. */
 fast_error=NULL;unlink(output);assert(resample_stock_start_named_observed(base,mode,0,18,19,"Unused",observer,&observed));
 assert(stock_callback_invoke()==3&&invoked&&assignments==3);stock_callback_release(0);
 stock_callback_destroy(callback);FN(0xf0a290,void(*)(void*))(path);
 puts("PASS production Sample import guard rejects absent WAV, complete zero PCM and failed tail before native assignment; valid audible PCM imports; device r20f mismatched stem finalizes before the guard even after native cleanup removes its name; observer once despite retained copies; Seq behavior unchanged. Task submission and assignment actions are fixtures.");return 0;
}
