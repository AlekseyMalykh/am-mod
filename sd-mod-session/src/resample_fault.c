#define _GNU_SOURCE
#include "resample_fault.h"
#include <dlfcn.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <ucontext.h>
#include <unistd.h>
#if !defined(__aarch64__) || ATOMIC_POINTER_LOCK_FREE != 2 || ATOMIC_INT_LOCK_FREE != 2
#error Fault diagnostics require AArch64 lock-free atomics
#endif
static pthread_mutex_t install_mutex=PTHREAD_MUTEX_INITIALIZER;
static struct sigaction previous,previous_abort,previous_bus;
static _Atomic uintptr_t application,library;
static _Atomic unsigned stage;
struct line {char bytes[1024];size_t n;};
static void word(struct line*b,const char*s){while(*s&&b->n<sizeof(b->bytes))b->bytes[b->n++]=*s++;}
static void number(struct line*b,const char*name,uintptr_t n){
 static const char digit[]="0123456789abcdef";word(b," ");word(b,name);word(b,"=0x");
 for(int shift=60;shift>=0;shift-=4)if(b->n<sizeof(b->bytes))b->bytes[b->n++]=digit[(n>>shift)&15];
}
static void fault(int sig,siginfo_t*info,void*context){
 ucontext_t*u=context;struct line b={{0},0};uintptr_t pc=u->uc_mcontext.pc;
 uintptr_t app=atomic_load_explicit(&application,memory_order_relaxed),so=atomic_load_explicit(&library,memory_order_relaxed);
 word(&b,"RESAMPLE_FAULT");number(&b,"stage",atomic_load_explicit(&stage,memory_order_relaxed));number(&b,"signal",(uintptr_t)sig);
 number(&b,"code",(uintptr_t)info->si_code);number(&b,"address",(uintptr_t)info->si_addr);number(&b,"pc",pc);
 number(&b,"native_pc",app&&pc-app<0x7500000?pc-app:UINTPTR_MAX);number(&b,"mod_pc",so&&pc-so<0x1000000?pc-so:UINTPTR_MAX);
 number(&b,"lr",u->uc_mcontext.regs[30]);number(&b,"sp",u->uc_mcontext.sp);
 number(&b,"app_base",app);number(&b,"mod_base",so);
 uintptr_t lr=u->uc_mcontext.regs[30];number(&b,"native_lr",app&&lr-app<0x7500000?lr-app:UINTPTR_MAX);number(&b,"mod_lr",so&&lr-so<0x1000000?lr-so:UINTPTR_MAX);
 for(unsigned i=0;i<6;++i){char key[3]={'x',(char)('0'+i),0};number(&b,key,u->uc_mcontext.regs[i]);}
 word(&b,"\n");ssize_t written=write(STDERR_FILENO,b.bytes,b.n);(void)written;
 /* Bounded AArch64 frame-record walk (x29 chain) for aborts such as
  * "pure virtual method called": the caller of __cxa_pure_virtual is a few
  * frames above abort(). Only reads aligned addresses inside the current
  * stack window, so a corrupt chain stops instead of faulting. */
 {struct line t={{0},0};uintptr_t fp=u->uc_mcontext.regs[29],sp=u->uc_mcontext.sp;
  word(&t,"FAULT_FRAMES");
  for(unsigned depth=0;depth<24&&fp>=sp&&fp<sp+0x200000&&!(fp&7);++depth){
   uintptr_t next=((const uintptr_t*)fp)[0],ret=((const uintptr_t*)fp)[1];
   if(app&&ret-app<0x7500000)number(&t,"n",ret-app);else if(so&&ret-so<0x1000000)number(&t,"m",ret-so);else number(&t,"x",ret);
   if(next<=fp)break;
   fp=next;}
  word(&t,"\n");written=write(STDERR_FILENO,t.bytes,t.n);(void)written;}
 /* Keep normal failure semantics, including the independent stock recovery.
  * No stdio, allocation, mutex, unwinding or unsafe stack reads in handler. */
 struct sigaction old=sig==SIGABRT?previous_abort:sig==SIGBUS?previous_bus:previous,restore=old;
 if(old.sa_handler==SIG_IGN||(old.sa_flags&SA_RESETHAND))restore.sa_handler=SIG_DFL;
 if(sigaction(sig,&restore,NULL))_exit(128+sig);
 if(old.sa_handler!=SIG_DFL&&old.sa_handler!=SIG_IGN){
  /* Preserve the original fault address/context for a native crash handler,
   * rather than replacing its siginfo with a fresh user-generated signal. */
  if(old.sa_flags&SA_SIGINFO)old.sa_sigaction(sig,info,context);else old.sa_handler(sig);
  return;
 }
 if(raise(sig))_exit(128+sig);
}
int resample_fault_install(uintptr_t base){
 if(!base||pthread_mutex_lock(&install_mutex))return 0;
 int ok=0;struct sigaction current;
 if(!sigaction(SIGSEGV,NULL,&current)){
  atomic_store_explicit(&application,base,memory_order_relaxed);
  Dl_info module;if(dladdr((void*)(uintptr_t)resample_fault_install,&module))atomic_store_explicit(&library,(uintptr_t)module.dli_fbase,memory_order_relaxed);
  if((current.sa_flags&SA_SIGINFO)&&current.sa_sigaction==fault)ok=1;
  else {previous=current;struct sigaction next={0};next.sa_mask=current.sa_mask;next.sa_flags=SA_SIGINFO|SA_ONSTACK|(current.sa_flags&(SA_NODEFER|SA_RESTART));next.sa_sigaction=fault;ok=sigaction(SIGSEGV,&next,NULL)==0;}
 }
 if(ok&&!sigaction(SIGABRT,NULL,&current)&&!((current.sa_flags&SA_SIGINFO)&&current.sa_sigaction==fault)){
  previous_abort=current;struct sigaction next={0};next.sa_mask=current.sa_mask;
  next.sa_flags=SA_SIGINFO|SA_ONSTACK|(current.sa_flags&(SA_NODEFER|SA_RESTART));next.sa_sigaction=fault;
  if(sigaction(SIGABRT,&next,NULL))ok=0;
 }
 if(ok&&!sigaction(SIGBUS,NULL,&current)&&!((current.sa_flags&SA_SIGINFO)&&current.sa_sigaction==fault)){
  previous_bus=current;struct sigaction next={0};next.sa_mask=current.sa_mask;
  next.sa_flags=SA_SIGINFO|SA_ONSTACK|(current.sa_flags&(SA_NODEFER|SA_RESTART));next.sa_sigaction=fault;
  if(sigaction(SIGBUS,&next,NULL))ok=0;
 }
 pthread_mutex_unlock(&install_mutex);return ok;
}
void resample_fault_stage(unsigned value){atomic_store_explicit(&stage,value,memory_order_relaxed);}
