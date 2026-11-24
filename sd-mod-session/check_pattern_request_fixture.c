#define pattern_native_proof pattern_request_proof
#include "check_pattern_native_fixture.c"
#undef pattern_native_proof
#include <stdatomic.h>
static _Alignas(8) unsigned char request_context[0x100],queues[0x260],slots[8][0xc0];
static uint64_t reader;
static _Thread_local int on_worker;
static _Atomic int context_alive;
static void base_destroy_fixture(void*p){assert(p==request_context&&!F(p,0x50,unsigned));atomic_store(&context_alive,0);}
static unsigned other_queue_cancel_fixture(void*p,uintptr_t key){assert(p==queues+8&&key==((uintptr_t)(request_context+0x48)&~(uintptr_t)3));return 0;}
void*pattern_request_context(void){return request_context;}
void*pattern_request_queue(void){return queues+0x1d8;}
int pattern_request_on_worker(void){return on_worker;}
int pattern_request_context_alive(void){return atomic_load(&context_alive);}
unsigned pattern_request_references(void){return __atomic_load_n((unsigned*)(request_context+0x50),__ATOMIC_ACQUIRE);}
unsigned pattern_request_active_tag(void){
    uintptr_t key=(uintptr_t)(request_context+0x48)&~(uintptr_t)3;
    for(unsigned i=0;i<8;++i){uintptr_t tag=__atomic_load_n((uintptr_t*)(slots[i]+0xb0),__ATOMIC_ACQUIRE);
        if((tag&~(uintptr_t)3)==key)return (unsigned)(tag&3);}
    return 0;
}
void pattern_request_context_reset(void){
    assert(!pattern_request_references());memset(request_context,0,sizeof request_context);
    F(request_context,0,uintptr_t)=base+0x6d8cd88;F(request_context,0x48,void*)=queues;
    F(request_context,0x78,void*)=things;F(request_context,0xc8,void*)=project;atomic_store(&context_alive,1);
}
void pattern_request_setup(void){
    F(queues+0x1d8,0,uint64_t)=8;F(queues+0x1d8,0x30,void*)=slots;
    for(unsigned i=0;i<8;++i){F(slots[i],0,uintptr_t)=base+0xb57a7c;F(slots[i],8,uintptr_t)=base+0x15265b0;}
    FN(0xf406b0,void(*)(void*))(queues+0x220); /* Real recursive producer mutex. */
    redirect(0x1fd2d20,base_destroy_fixture);redirect(0x2b55650,other_queue_cancel_fixture);
    pattern_request_context_reset();
}
void pattern_request_drain(unsigned count){
    ++on_worker;for(unsigned i=0;i<count;++i)FN(0x2b59200,void(*)(void*,uint64_t*))(queues+0x1d8,&reader);--on_worker;
}
void pattern_request_reclaim(void){FN(0x2b58c70,void(*)(void*))(queues+0x1d8);}
void pattern_request_destroy_context(void){FN(0x1517c70,void(*)(void*))(request_context);}
