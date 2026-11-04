#define _GNU_SOURCE
#include "mixer_live.h"
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <stdatomic.h>
#include <math.h>
#if ATOMIC_POINTER_LOCK_FREE != 2 || ATOMIC_INT_LOCK_FREE != 2
#error Native Mixer attachment requires lock-free pointers/counters
#endif
static uintptr_t image;
static struct mixer_live_ops ops;
static _Atomic(struct mixer_live*) attached;
static _Atomic(struct mixer_live*) retired;
static _Atomic unsigned callbacks;
static _Atomic uint64_t binding_epoch = 1, attached_epoch;
static atomic_flag binding_lock = ATOMIC_FLAG_INIT;
static struct mixer_native_binding binding, attached_binding;
static int same_binding(const struct mixer_native_binding *a,const struct mixer_native_binding *b)
{
    return a->epoch == b->epoch && a->engine == b->engine && a->program == b->program &&
        a->rate == b->rate && a->capacity == b->capacity;
}
static void *ptr(void*,unsigned);
static struct mixer_native_binding observe(void *engine,unsigned frames)
{
    struct mixer_native_binding result = {0};
    result.epoch = atomic_load_explicit(&binding_epoch,memory_order_acquire);
    void *mixer = ptr(engine,0x110);
    if (!mixer || !frames || frames > 8192) return result;
    uintptr_t first = (uintptr_t)ptr(mixer,0x100);
    uintptr_t end = (uintptr_t)ptr(mixer,0x108);
    /* The Sample kit has one native Program. Other graphs stay unavailable
     * until their complete multi-Program routing is supported. */
    if (!first || end < first || end - first != sizeof(void*)) return result;
    void *program;
    memcpy(&program,(void*)first,sizeof(program));
    if (!program) return result;
    /* Program ctor28ce220 constructs its Instrument base at+78.
     *277157c reads that base's sample-rate property at638, then passes it to
     * the native buffer allocator. Read on the render owner while valid. */
    void *property = ptr(program,0x78+0x638);
    if (!property) return result;
    float rate;
    memcpy(&rate,(char*)property+0x50,sizeof(rate));
    unsigned capacity;
    memcpy(&capacity,(char*)property+0xa8,sizeof(capacity));
    if (!isfinite(rate) || rate < 8000 || rate > 192000 ||
        capacity < frames || capacity > 8192) return result;
    result.engine = engine;
    result.program = program;
    result.rate = rate;
    result.capacity = capacity;
    return result;
}
static void publish_binding(const struct mixer_native_binding *next)
{
    if (atomic_flag_test_and_set_explicit(&binding_lock,memory_order_acquire)) return;
    if (next->epoch == atomic_load_explicit(&binding_epoch,memory_order_acquire)) binding = *next;
    atomic_flag_clear_explicit(&binding_lock,memory_order_release);
}
static void engine(void*p,void*c,unsigned n)
{
    atomic_fetch_add_explicit(&callbacks,1,memory_order_seq_cst);
    struct mixer_live*s=atomic_load_explicit(&attached,memory_order_seq_cst);
    uint64_t expected = atomic_load_explicit(&attached_epoch,memory_order_acquire);
    uintptr_t caller = (uintptr_t)__builtin_extract_return_addr(__builtin_return_address(0));
    if (s && expected) {
        struct mixer_native_binding current = observe(p,n);
        if (caller != image + 0x18dc010 || !same_binding(&current,&attached_binding) ||
            expected != atomic_load_explicit(&binding_epoch,memory_order_acquire)) s = NULL;
    }
    if(s)mixer_live_render(s,p,c,n);else ops.engine(p,c,n);
    /* Only the real live callsite may discover a session. Offline renderers
     * call the same entry from18b556c/18b98bc and have independent lifetimes. */
    if (caller == image + 0x18dc010) {
        struct mixer_native_binding current = observe(p,n);
        publish_binding(&current);
    }
    atomic_fetch_sub_explicit(&callbacks,1,memory_order_seq_cst);
}
static void program(void*p,void*c,unsigned n){mixer_live_program(&ops.route,p,c,n);}
static int sum(void*i,void*s,void*d,const void*r,unsigned n){return mixer_route_sum(&ops.route,i,s,d,r,n);}
static void stage(void*p,void*b,unsigned n,unsigned s){mixer_route_stage(&ops.route,p,b,n,s);}
static void*ptr(void*p,unsigned offset){void*v;memcpy(&v,(char*)p+offset,8);return v;}
static void*final_output(void*e)
{
    void*owner=ptr(e,0x110),*router=owner?ptr(owner,0x130):NULL;
    if(!router)return NULL;
    const unsigned route[8]={2,0,3,0,0,1,0x3f800000,0};return ops.route.resolve(router,route);
}
struct hook {unsigned offset;uint32_t expected[6];void*callback;void*original;};
int mixer_live_native_install(uintptr_t base)
{
    if(image)return image==base;
    if(!base||sysconf(_SC_PAGESIZE)!=4096)return 0;
    struct hook hooks[]={
        {0x2226fb0,{0xd503233f,0xa9be7bfd,0x910003fd,0xa90153f3,0xaa0003f3,0x2a0203f4},engine,NULL},
        {0x286be30,{0xd503233f,0xd10303ff,0xb00247c1,0xf941cc21,0xa9067bfd,0x910183fd},program,NULL},
        {0x2769bf0,{0xd503233f,0xa9bc7bfd,0x52800000,0x910003fd,0xa90153f3,0xaa0103f3},sum,NULL},
        {0x276a310,{0xd503233f,0xa9bc7bfd,0xd2800604,0x910003fd,0xa90153f3,0x93407c73},stage,NULL}
    };
    for(unsigned i=0;i<4;++i)if(memcmp((void*)(base+hooks[i].offset),hooks[i].expected,24))return 0;
    unsigned char*memory=mmap(NULL,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if(memory==MAP_FAILED)return 0;
    for(unsigned i=0;i<4;++i){unsigned char*t=memory+i*128;hooks[i].original=t;
        uint32_t code[12]={0xd503245f};memcpy(code+1,hooks[i].expected,24);
        /* BTI + six stolen instructions + absolute continuation. Only the
         * Program prologue has PC-relative ADRP; relocate it via literal x1. */
        if(i==1)code[3]=0x58000161; /* ldr x1,[pc+44] -> literal at56 */
        code[7]=0x58000070;code[8]=0xd61f0200;code[9]=0xd503201f;
        memcpy(t,code,40);uintptr_t continuation=base+hooks[i].offset+24;
        memcpy(t+40,&continuation,8);
        uintptr_t guard_page=base+0x7164000;memcpy(t+56,&guard_page,8);
        /* ldr at28 needs literal40: encoded imm3 above. */
    }
    __builtin___clear_cache((char*)memory,(char*)memory+512);
    if(mprotect(memory,4096,PROT_READ|PROT_EXEC)){munmap(memory,4096);return 0;}
    unsigned protected_count=0;
    for(;protected_count<4;++protected_count){uintptr_t page=(base+hooks[protected_count].offset)&~(uintptr_t)4095;
        if(mprotect((void*)page,4096,PROT_READ|PROT_WRITE))break;
    }
    if(protected_count!=4){
        for(unsigned i=0;i<protected_count;++i)if(mprotect((void*)((base+hooks[i].offset)&~(uintptr_t)4095),4096,PROT_READ|PROT_EXEC))_exit(121);
        munmap(memory,4096);return 0;
    }
    ops.engine=(void(*)(void*,void*,unsigned))hooks[0].original;
    ops.route.render=(void(*)(void*,void*,unsigned))hooks[1].original;
    ops.route.sum=(int(*)(void*,void*,void*,const void*,unsigned))hooks[2].original;
    ops.route.stage=(void(*)(void*,void*,unsigned,unsigned))hooks[3].original;
    ops.route.buffer=(void*(*)(void*,unsigned long))(base+0x276abc0);
    ops.route.buffer_count=(unsigned long(*)(void*))(base+0x276abe0);
    ops.route.resolve=(void*(*)(void*,const void*))(base+0x18ccbc0);ops.final_output=final_output;
    for(unsigned i=0;i<4;++i){uint32_t code[]={0xd503245f,0x58000070,0xd61f0200,0xd503201f};
        void*entry=(void*)(base+hooks[i].offset);memcpy(entry,code,16);memcpy((char*)entry+16,&hooks[i].callback,8);
        __builtin___clear_cache(entry,(char*)entry+24);
    }
    for(unsigned i=0;i<4;++i)if(mprotect((void*)((base+hooks[i].offset)&~(uintptr_t)4095),4096,PROT_READ|PROT_EXEC))_exit(121);
    image=base;return 1;
}
const struct mixer_live_ops*mixer_live_native_ops(void){return image?&ops:NULL;}
int mixer_live_native_attach(struct mixer_live*s)
{
    if(!image||atomic_load_explicit(&retired,memory_order_acquire))return 0;
    if (s) atomic_store_explicit(&attached_epoch,0,memory_order_release);
    struct mixer_live*old=atomic_exchange_explicit(&attached,s,memory_order_seq_cst);
    if(old&&old!=s)atomic_store_explicit(&retired,old,memory_order_release);
    return 1;
}
struct mixer_live*mixer_live_native_take_retired(void)
{
    if(atomic_load_explicit(&callbacks,memory_order_seq_cst))return NULL;
    return atomic_exchange_explicit(&retired,NULL,memory_order_acquire);
}

int mixer_live_native_binding(struct mixer_native_binding *out)
{
    if (!out) return 0;
    if (atomic_flag_test_and_set_explicit(&binding_lock,memory_order_acquire)) return -1;
    *out = binding;
    atomic_flag_clear_explicit(&binding_lock,memory_order_release);
    return out->engine && out->epoch == atomic_load_explicit(&binding_epoch,memory_order_acquire);
}
void mixer_live_native_invalidate(void)
{
    atomic_fetch_add_explicit(&binding_epoch,1,memory_order_acq_rel);
}
int mixer_live_native_attach_binding(struct mixer_live *session,const struct mixer_native_binding *source)
{
    if (!session || !source || !source->epoch || !source->engine || !source->program || !image ||
        atomic_load_explicit(&retired,memory_order_acquire) ||
        atomic_load_explicit(&attached,memory_order_acquire) ||
        atomic_load_explicit(&callbacks,memory_order_seq_cst)) return 0;
    if (atomic_flag_test_and_set_explicit(&binding_lock,memory_order_acquire)) return 0;
    int valid = source->epoch == atomic_load_explicit(&binding_epoch,memory_order_acquire) &&
        same_binding(source,&binding);
    if (valid) {
        attached_binding = *source;
        atomic_store_explicit(&attached_epoch,source->epoch,memory_order_release);
        atomic_store_explicit(&attached,session,memory_order_seq_cst);
    }
    atomic_flag_clear_explicit(&binding_lock,memory_order_release);
    return valid;
}
