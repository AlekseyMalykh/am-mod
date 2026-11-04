/* Isolated native factory research; never loaded into MPC. */
#define main legacy_eq_main
#include "check_fx_native.c"
#undef main
#include "check_fx_metadata.h"
uint32_t requested_fx=0x61465444;
extern int modern_fx_probe(uintptr_t,const char**);
static void modern_fault(int signal,siginfo_t*info,void*context){
 ucontext_t*u=context;fprintf(stderr,"LR=%llx X0=%llx X1=%llx X7=%llx\n",u->uc_mcontext.regs[30]-base,u->uc_mcontext.regs[0],u->uc_mcontext.regs[1],u->uc_mcontext.regs[7]);
 uintptr_t*fp=(void*)u->uc_mcontext.regs[29];for(int i=0;i<12&&fp;++i){fprintf(stderr,"frame %d lr=%lx\n",i,fp[1]-base);fp=(void*)fp[0];}
 fault(signal,info,context);
}
int main(int argc,char**argv){
 assert(argc==2||argc==3);if(argc==3)requested_fx=strtoul(argv[2],NULL,16);setbuf(stdout,NULL);
 struct sigaction sa={.sa_sigaction=modern_fault,.sa_flags=SA_SIGINFO};sigaction(SIGSEGV,&sa,NULL);sigaction(SIGABRT,&sa,NULL);sigaction(SIGILL,&sa,NULL);
 load(argv[1]);phase="native FX metadata initialization";check_fx_metadata(base);
 return modern_fx_probe(base,&phase);
}
