/* Inspect the real AC50 descriptor constructor under QEMU. Execution stops
 * after descriptor construction, before dependencies/engine subscriptions.
 * Input fixtures contain no devices, projects or user audio. */
#define main eq_contract_main
#include "check_fx_native.c"
#undef main
static unsigned char controller_data[0x200],metadata[0x400],dependencies[0x800];
static void capture(void *controller)
{
    uintptr_t begin=*(uintptr_t*)((char*)controller+0x180),end=*(uintptr_t*)((char*)controller+0x188);
    assert(end>=begin && (end-begin)%0x158==0 && end-begin<0x10000);
    for(uintptr_t p=begin;p<end;p+=0x158){
        printf("FX id=%08x name=%s page=%s value=%d",*(unsigned*)p,
            *(char**)(p+8),*(char**)(p+0x10),*(int*)(p+0x18));
        for(unsigned i=0;i<6;++i)printf(" | p%d %s",*(int*)(p+0x20+0x30*i),*(char**)(p+0x28+0x30*i));
        puts("");
    }
    printf("PASS actual descriptor catalogue count=%lu (engine subscriptions not executed)\n",(unsigned long)((end-begin)/0x158));
    _exit(0);
}
int main(int argc,char **argv)
{
    assert(argc==2);setbuf(stdout,NULL);struct sigaction sa={.sa_sigaction=fault,.sa_flags=SA_SIGINFO};
    sigaction(SIGSEGV,&sa,NULL);sigaction(SIGABRT,&sa,NULL);sigaction(SIGILL,&sa,NULL);
    load(argv[1]);phase="native AC50 catalogue constructor";
    /* MOV X0,X20; LDR X16,#12; BR X16; NOP; capture pointer. */
    uint32_t code[]={0xaa1403e0,0x58000070,0xd61f0200,0xd503201f};
    memcpy((void*)(base+0x155b900),code,sizeof(code));void *fn=capture;
    memcpy((void*)(base+0x155b910),&fn,sizeof(fn));
    __builtin___clear_cache((char*)base+0x155b900,(char*)base+0x155b918);
    FN(0x15581d0,void(*)(void*,void*,void*,void*,void*,void*,void*))
        (controller_data,dependencies,dependencies,dependencies,metadata,dependencies,dependencies);
    assert(!"capture did not run");
}
