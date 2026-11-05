#include "src/phone_events.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdatomic.h>
#include <sched.h>
static _Atomic int producer_done;
static struct phone_event movement(unsigned cc,unsigned v)
{
    struct phone_event e={0};e.kind=PHONE_CONTROLLER;e.generation=1;e.step_ticket=2;e.workflow_ticket=3;
    e.native[8]=(unsigned char)cc;e.native[9]=(unsigned char)v;return e;
}
static void expect(unsigned kind,unsigned cc,unsigned value)
{
    struct phone_event e;assert(phone_events_pop(&e));assert(e.kind==kind&&e.native[8]==cc&&e.native[9]==value);
}
static void barriers(void)
{
    for(unsigned kind=PHONE_BUTTON;kind<=PHONE_RESAMPLE_PAD;++kind){
        struct phone_event a=movement(0x10,25),b=a;b.kind=kind;b.native[8]=0x31;b.native[9]=1;
        assert(phone_events_push(&a)&&phone_events_push(&b));a.native[9]=0;assert(phone_events_push(&a));
        assert(phone_events_pending()==3);expect(PHONE_CONTROLLER,0x10,25);expect(kind,0x31,1);expect(PHONE_CONTROLLER,0x10,0);
    }
    struct phone_event a=movement(0x10,33),e=movement(0x64,127);
    assert(phone_events_push(&a)&&phone_events_push(&e)&&phone_events_push(&e));a.native[9]=0;assert(phone_events_push(&a));
    assert(phone_events_pending()==4);expect(PHONE_CONTROLLER,0x10,33);expect(PHONE_CONTROLLER,0x64,127);expect(PHONE_CONTROLLER,0x64,127);expect(PHONE_CONTROLLER,0x10,0);
}
static void contexts(void)
{
    for(unsigned change=0;change<6;++change){
        struct phone_event a=movement(0x11,56),b=a;b.native[9]=127;
        if(change==0)++b.generation;
        if(change==1)++b.step_ticket;
        if(change==2)++b.workflow_ticket;
        if(change==3)b.pad=32;
        if(change==4)b.native[0]=1;
        if(change==5)b.native[15]=1;
        assert(phone_events_push(&a)&&phone_events_push(&b)&&phone_events_pending()==2);
        struct phone_event out;assert(phone_events_pop(&out)&&!memcmp(&out,&a,sizeof(a)));assert(phone_events_pop(&out)&&!memcmp(&out,&b,sizeof(b)));
    }
}
static void*producer(void*unused)
{
    (void)unused;for(unsigned n=0;n<50000;++n){struct phone_event e=movement(0x10+n%3,n%128);e.when_ms=n;while(!phone_events_push(&e))sched_yield();}
    atomic_store(&producer_done,1);
    return NULL;
}
int main(void)
{
    barriers();contexts();
    /* A producer Shift change must not merge two different edit gestures. */
    struct phone_event before=movement(0x10,0),after=before;
    assert(phone_events_push(&before));before.native[9]=32;assert(phone_events_push(&before));
    after.native[9]=64;after.down=1;assert(phone_events_push(&after));
    assert(phone_events_pending()==3);expect(PHONE_CONTROLLER,0x10,0);expect(PHONE_CONTROLLER,0x10,32);
    struct phone_event shifted;assert(phone_events_pop(&shifted)&&shifted.down==1&&shifted.native[9]==64);
    /* Interleaved sweeps keep each final position AND final focus order. */
    struct phone_event a=movement(0x10,33),b=movement(0x11,56);
    assert(phone_events_push(&a)&&phone_events_push(&b));a.native[9]=0;a.when_ms=900;assert(phone_events_push(&a));
    assert(phone_events_pending()==3);expect(PHONE_CONTROLLER,0x10,33);expect(PHONE_CONTROLLER,0x11,56);struct phone_event out;assert(phone_events_pop(&out)&&out.native[8]==0x10&&out.native[9]==0&&out.when_ms==900);
    /* Ring wrap, including replacement in a completely full queue. */
    for(unsigned wrap=0;wrap<300;++wrap){
        struct phone_event key=movement(0x31,1);key.kind=PHONE_BUTTON;
        for(unsigned i=0;i<PHONE_EVENT_CAPACITY-2;++i)assert(phone_events_push(&key));
        a=movement(0x12,33);assert(phone_events_push(&a));a.native[9]=56;assert(phone_events_push(&a));a.native[9]=127;assert(phone_events_push(&a));
        assert(phone_events_pending()==PHONE_EVENT_CAPACITY&&!phone_events_push(&key));
        for(unsigned i=0;i<PHONE_EVENT_CAPACITY-2;++i)expect(PHONE_BUTTON,0x31,1);
        expect(PHONE_CONTROLLER,0x12,33);
        expect(PHONE_CONTROLLER,0x12,127);
    }
    /* Producer/UI races cannot corrupt or reverse the per-knob positions. */
    pthread_t thread;assert(!pthread_create(&thread,NULL,producer,NULL));
    uint64_t previous[3]={0};unsigned seen=0;
    while(!atomic_load(&producer_done)||phone_events_pending())if(phone_events_pop(&out)){unsigned k=out.native[8]-0x10;assert(k<3&&(!(seen&(1u<<k))||out.when_ms>=previous[k]));previous[k]=out.when_ms;seen|=1u<<k;}else sched_yield();
    assert(!pthread_join(thread,NULL));
    while(phone_events_pop(&out)){unsigned k=out.native[8]-0x10;assert(k<3&&(!(seen&(1u<<k))||out.when_ms>=previous[k]));previous[k]=out.when_ms;seen|=1u<<k;}
    assert(seen==7&&previous[0]==49998&&previous[1]==49999&&previous[2]==49997);
    puts("PASS absolute knob queue: latest values/focus order, action and ownership barriers, relative encoder preserved, full ring replacement, 50000 concurrent updates");return 0;
}
