/* Private stock image; no native Project, Track, Program or GUI is fabricated. */
#define main unused_fx_main
#include "check_fx_native.c"
#undef main
extern int check_arrangement_material(uintptr_t);
extern void __register_frame(void*);
extern void *__deregister_frame(void*);
int main(int argc,char**argv){
    assert(argc==2);struct sigaction sa={.sa_sigaction=fault,.sa_flags=SA_SIGINFO};
    sigaction(SIGSEGV,&sa,NULL);sigaction(SIGABRT,&sa,NULL);sigaction(SIGILL,&sa,NULL);
    setbuf(stdout,NULL);load(argv[1]);phase="owned native Arrange material";
    /* Pinned .eh_frame, not a simulated native allocator failure: allow real
     * stock cleanup/landing pads to unwind into the C++ test/adapter. */
    __register_frame((void*)(base+0x6275af0));
    int result=check_arrangement_material(base);
    __deregister_frame((void*)(base+0x6275af0));return result;
}
