/* Native UInt Play command up to the controller queue. The former test
 * invented a JUCE ValueSource at +170, hiding r24b's observer-list crash. */
#define main old_fx_main
#include "check_fx_native.c"
#undef main
#undef FN
#define base note_image
#include "src/note_native.c"
#undef base
#include <sys/wait.h>
static _Alignas(8) unsigned char sample_root[0x7400],things[0x700],instrument[0x1700],controls[0xb00],scheduler[0x300];
static unsigned validates,queued;
static unsigned char task[0x30];
static void redirect(uintptr_t offset,void*fn){
    uint32_t code[]={0x58000050,0xd61f0200};memcpy((void*)(base+offset),code,8);
    FIELD((void*)base,offset+8,uintptr_t)=(uintptr_t)fn;
    __builtin___clear_cache((char*)base+offset,(char*)base+offset+16);
}
static void mutex_only(void*p){assert(p==scheduler+0x220);}
static void validate(void *fn,void *owner,const unsigned *value){
    assert(fn==controls+0x928+0x68&&owner==(void*)0x1234&&*value==0);++validates;
}
static int queue(void *p,const void *command){
    assert(p==scheduler+0x1d8);memcpy(task,command,sizeof(task));++queued;
    assert(FIELD(task,0,uintptr_t)==base+0x165fe90);
    assert(FIELD(task,0x18,void*)==controls+0x928);
    assert(FIELD(task,0x28,unsigned)==0&&FIELD(task,0x2c,unsigned)==1);
    return 0; /* Decline ownership; native cleanup must keep the property alive. */
}
static void old_crash(int sig){(void)sig;_exit(77);}
int main(int argc,char **argv){
    assert(argc==2);struct sigaction sa={.sa_sigaction=fault,.sa_flags=SA_SIGINFO};
    sigaction(SIGSEGV,&sa,NULL);sigaction(SIGABRT,&sa,NULL);sigaction(SIGILL,&sa,NULL);
    phase="native unsigned Play command";load(argv[1]);note_native_init(base);
    root=sample_root;FIELD(root,0,uintptr_t)=base+SAMPLE_VPTR;
    FIELD(root,0x148,void*)=things;FIELD(things,0x668,void*)=instrument;
    FIELD(root,0x178,void*)=controls;
    void *play=sample_root+0x6df8;FIELD(play,0,uintptr_t)=base+SLIDER_VPTR;
    /* Build the actual intrusive subscription rather than a fake ValueSource. */
    _Alignas(8) unsigned char listener[0x48]={0};
    ((void(*)(void*,void*,void*,int))(base+0x236b190))(listener,(char*)play+0x170,listener,0);
    ((void(*)(void*,void*))(base+0x236b1e0))(listener,controls+0x928+0x18);
    FIELD(listener,0,uintptr_t)=base+0x6e21bb0;
    pid_t child=fork();assert(child>=0);
    if(!child){signal(SIGSEGV,old_crash);signal(SIGILL,old_crash);
        uint64_t var[2]={0};*(uintptr_t*)(base+0x71e3af0)=base+0x6d76600;
        ((void(*)(void*,int))(base+0xf31190))(var,0);
        ((void(*)(void*,const void*))(base+0xfb5680))((char*)play+0x170,var);_exit(78);}
    int status;assert(waitpid(child,&status,0)==child&&WIFEXITED(status)&&WEXITSTATUS(status)==77);
    void *property=controls+0x928;FIELD(property,0x34,unsigned)=1;
    FIELD(property,0x38,void*)=scheduler;FIELD(property,0x40,unsigned)=2;
    FIELD(property,0x78,uintptr_t)=1;FIELD(property,0x80,uintptr_t)=(uintptr_t)validate;
    FIELD(property,0x88,void*)=(void*)0x1234;
    redirect(0xf40e50,mutex_only);redirect(0xf40e90,mutex_only);redirect(0x2b59300,queue);
    FIELD(instrument,0x1690,int)=1;
    assert(note_native_request_poly()==2&&validates==1&&queued==1);
    assert(FIELD(instrument,0x1690,int)==1); /* No unsafe audio-model writes. */
    FIELD(instrument,0x1690,int)=0;
    assert(note_native_request_poly()==1&&queued==1);
    FIELD(instrument,0x1690,int)=1;FIELD(property,0x88,void*)=NULL;
    assert(note_native_request_poly()==0&&queued==1);
    FIELD(root,0x178,void*)=NULL;assert(note_native_request_poly()==0&&queued==1);
    FIELD(things,0x668,void*)=NULL;assert(note_native_request_poly()==0&&queued==1);
    puts("PASS real observer layout reproduces r24b SIGSEGV; native unsigned Play command validates and queues Poly=0, preserving model until completion");
    return 0;
}
