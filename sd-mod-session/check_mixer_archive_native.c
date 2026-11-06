#define main unused_archive_fx_main
#include "check_fx_native.c"
#undef main
extern int mixer_archive_native_probe(uintptr_t);
int main(int argc,char**argv)
{
    assert(argc==2);struct sigaction sa={.sa_sigaction=fault,.sa_flags=SA_SIGINFO};
    sigaction(SIGSEGV,&sa,NULL);sigaction(SIGILL,&sa,NULL);sigaction(SIGABRT,&sa,NULL);
    load(argv[1]);phase="Mixer native JSON writer/music archive integration";
    return mixer_archive_native_probe(base);
}
