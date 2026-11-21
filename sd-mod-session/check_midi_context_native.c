#define main unused_fx_probe
#include "check_fx_native.c"
#undef main
#include "src/midi_context.h"
#include "src/midi_native.h"
#include <stddef.h>

static unsigned char track[0x3000],program[0x16a8];
static struct step_event nested;
static unsigned depth,visits;
static void (*dispatch)(void*,uint64_t,const struct step_event*,void*);
/* Replace only C++ RTTI traversal of our fixture Program. The real patched
 * event entry, relocated native prologue and native return execute in ELF. */
static void *cast_observer(void *p,const void *from,const void *to,ptrdiff_t hint)
{
    (void)from;(void)to;(void)hint;assert(p==program);
    struct am_midi_context value;assert(am_midi_context_snapshot(&value));
    assert(value.track_id==(uintptr_t)track&&value.channel==2);
    assert(value.sound==(depth?15:14));++visits;
    if(!depth){
        assert(value.event.tune==3&&value.event.slice==2);
        ++depth;dispatch(track,128,&nested,NULL);--depth;
        assert(am_midi_context_snapshot(&value)&&value.sound==14);
    }
    return NULL;
}
void *check_native_symbol(const char *name,void *resolved)
{return !strcmp(name,"__dynamic_cast")?(void*)cast_observer:resolved;}
int main(int argc,char **argv)
{
    assert(argc==2);struct sigaction sa={.sa_sigaction=fault,.sa_flags=SA_SIGINFO};
    sigaction(SIGSEGV,&sa,NULL);sigaction(SIGILL,&sa,NULL);sigaction(SIGABRT,&sa,NULL);
    load(argv[1]);phase="installed native MIDI context hook";
    struct step_event (*make)(unsigned,int64_t,int64_t,unsigned,unsigned,float)=FN(0x27e54d0,struct step_event(*)(unsigned,int64_t,int64_t,unsigned,unsigned,float));
    void (*set)(void*,int,int,int)=FN(0x27e4400,void(*)(void*,int,int,int));
    struct step_event source=make(50,960,240,100,2,.8f);nested=make(51,1024,120,100,2,.6f);
    set(source.data+4,0,0,3);set(source.data+4,15,0,2);
    void *p=program;memcpy(track+0xbb8,&p,sizeof p);track[0x4f0]=1;
    for(unsigned i=0;i<2;++i){int pad=(int)i+14,note=(int)i+50;memcpy(program+0x12a8+(unsigned)pad*4,&note,4);memcpy(program+0x14a8+(unsigned)note*4,&pad,4);}
    assert(am_midi_native_install(base));assert(am_midi_native_install(base));assert(!am_midi_native_install(base+4096));
    dispatch=FN(0x29caa30,void(*)(void*,uint64_t,const struct step_event*,void*));
    struct am_midi_context value;assert(!am_midi_context_snapshot(&value));
    dispatch(track,96,&source,NULL);assert(visits==2);
    assert(!am_midi_context_snapshot(&value));
    puts("PASS installed native MIDI hook: real entry/trampoline/return, Sound A15, Tune/Slice, nested dispatch and context restoration");
    return 0;
}
