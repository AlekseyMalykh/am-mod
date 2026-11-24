/* Pinned native question/string lifetime; no native GUI or OS shutdown. */
#define main unused_fx_main
#include "check_fx_native.c"
#undef main
extern void check_power_native(uintptr_t);
int main(int argc,char**argv){
 assert(argc==2);struct sigaction sa={.sa_sigaction=fault,.sa_flags=SA_SIGINFO};
 sigaction(SIGSEGV,&sa,NULL);sigaction(SIGABRT,&sa,NULL);
 load(argv[1]);phase="native power question options";check_power_native(base);return 0;
}
