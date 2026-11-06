#define main unused_fx_main
#include "check_fx_native.c"
#undef main
#include "src/mixer_sources.h"
#include "src/mixer_archive.h"
#include <pthread.h>
#define FIELD(p,o,T) (*(T*)((char*)(p)+(o)))
static _Alignas(16) unsigned char context[0x100],owner[0x260],slots[4][0xc0],things[0x100];
static _Alignas(16) unsigned char program[0x2900],instrument[3][0x2468],layers[3][8][0xb30],control[0x20];
static void *pads[128];
static uint64_t cursor;
static void lock(void *p){assert(!pthread_mutex_lock(p));}
static void unlock(void *p){assert(!pthread_mutex_unlock(p));}
static void stub(uintptr_t offset,void *fn)
{
    uint32_t code[]={0x58000050,0xd61f0200};
    memcpy((void*)(base+offset),code,8);memcpy((void*)(base+offset+8),&fn,8);
}
static void pump(void)
{
    FN(0x2b59200,void(*)(void*,uint64_t*))(owner+0x1d8,&cursor);
    assert(!FIELD(context,0x50,unsigned));
}
static int poll(uint64_t epoch,struct mixer_sources_snapshot *out)
{return mixer_sources_poll(program,epoch,out);}
static void sample(unsigned inst,unsigned layer,int live)
{
    FIELD(layers[inst][layer],0x4a8,void*)=live?(void*)0x1234:NULL;
    FIELD(layers[inst][layer],0x4b0,void*)=live?control:NULL;
}
int main(int argc,char **argv)
{
    assert(argc==2);setbuf(stdout,NULL);load(argv[1]);phase="native Mixer occupied-source snapshot";
    struct sigaction sa={.sa_sigaction=fault,.sa_flags=SA_SIGINFO};
    sigaction(SIGSEGV,&sa,NULL);sigaction(SIGABRT,&sa,NULL);sigaction(SIGILL,&sa,NULL);
    mixer_archive_init(base);
    FIELD(context,0,uintptr_t)=base+0x6e0dfd8;
    FIELD(context,0x48,void*)=owner;FIELD(context,0x78,void*)=things;
    FIELD(things,0xb8,void*)=program;
    FIELD(owner+0x1d8,0,uint64_t)=4;FIELD(owner+0x1d8,0x30,void*)=slots;
    assert(!pthread_mutex_init((void*)(owner+0x220),NULL));
    for(unsigned i=0;i<4;++i){FIELD(slots[i],0,uintptr_t)=base+0xb57a7c;FIELD(slots[i],8,uintptr_t)=base+0x15265b0;}
    stub(0xf40e50,lock);stub(0xf40e90,unlock);
    __builtin___clear_cache((char*)base,(char*)base+0x7500000);
    FIELD(program,0x2808,void*)=pads;FIELD(program,0x2810,void*)=pads+128;
    for(unsigned i=0;i<3;++i){
        FIELD(instrument[i],0x2310,void*)=program;
        FIELD(instrument[i],0x22e8,void*)=layers[i];FIELD(instrument[i],0x22f0,void*)=layers[i]+8;
    }
    pads[0]=instrument[0];pads[64]=instrument[1];pads[127]=instrument[2];
    FIELD(control,8,unsigned)=2;FIELD(control,12,unsigned)=2;
    sample(0,0,1);sample(2,7,1); /* Last pad, last layer, unrelated to bank A. */
    mixer_sources_bind(base,context);
    struct mixer_sources_snapshot out;
    assert(!poll(10,&out)&&FIELD(context,0x50,unsigned)==1);
    assert(!out.serial); /* No getter before the actual owner queue executes. */
    pump();assert(poll(10,&out)&&out.present[0]==1&&out.present[1]==(UINT64_C(1)<<63));
    assert(out.captured_ms&&FIELD(control,8,unsigned)==2&&FIELD(control,12,unsigned)==2);
    uint64_t first=out.serial;
    sample(1,3,1);sample(2,7,0);pump();
    assert(poll(10,&out)&&out.serial>first&&out.present[0]==1&&out.present[1]==1);
    pump();
    assert(!poll(11,&out));pump();assert(poll(11,&out)&&out.source_epoch==11);pump();
    /* A changed Program is rejected before looking at its old instruments. */
    FIELD(things,0xb8,void*)=(void*)1;
    assert(poll(11,&out));pump();assert(!poll(11,&out));pump();
    FIELD(things,0xb8,void*)=program;
    assert(!poll(11,&out));pump();assert(poll(11,&out));pump();
    /* Reject malformed layers rather than call the unchecked native getter. */
    FIELD(instrument[0],0x22f0,void*)=(void*)1;
    assert(poll(11,&out));pump();assert(!poll(11,&out));pump();
    FIELD(instrument[0],0x22f0,void*)=layers[0]+8;
    assert(!poll(11,&out));pump();assert(poll(11,&out));
    mixer_archive_reset(); /* Invalidate a request already in the native queue. */
    pump();assert(!poll(11,&out));pump();assert(poll(11,&out));pump();
    /* Saturation refuses the native blocking-post path; later poll retries. */
    FIELD(owner+0x1d8,0x2d,unsigned char)=1;
    assert(poll(11,&out)&&!FIELD(context,0x50,unsigned));
    FIELD(owner+0x1d8,0x2d,unsigned char)=0;
    assert(poll(11,&out)&&FIELD(context,0x50,unsigned)==1);
    mixer_sources_stop(context);FIELD(context,0x78,void*)=(void*)1;
    pump();assert(!poll(11,&out)&&!FIELD(context,0x50,unsigned));
    assert(FIELD(control,8,unsigned)==2&&FIELD(control,12,unsigned)==2);
    assert(!pthread_mutex_destroy((void*)(owner+0x220)));
    puts("PASS native Mixer sources: actual Program/layer getters and Copy queue, 128 pads/eight layers, balanced weak locks, empty/update/epoch/project/malformed guards, saturated post retry and queued stop before poisoned context");
    return 0;
}
