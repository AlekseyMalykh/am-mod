#define _GNU_SOURCE
#include "mixer_leds.h"
#include <math.h>
#include <stdatomic.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#if ATOMIC_POINTER_LOCK_FREE != 2 || ATOMIC_LLONG_LOCK_FREE != 2 || ATOMIC_INT_LOCK_FREE != 2
#error Mixer lighting requires lock-free snapshots
#endif
static uintptr_t image;
static int (*native_dispatch)(void*,void*);
static int (*native_refresh)(void*,unsigned);
static int (*native_event)(void*,unsigned,unsigned,unsigned,float);
static _Atomic unsigned enabled,bank;
static _Atomic uint64_t native_generation=1,last_sound[16];
static _Atomic uintptr_t published_program,background[128];
static _Atomic uint64_t version;
static _Atomic uint32_t colours[16];
/* Native callbacks own their arguments. No native pointer is retained for
 * later dereference on UI, RGB timer or another thread. */
struct origin {unsigned sound,valid;uintptr_t background;uint64_t generation;};
static _Thread_local struct origin origin;
static uintptr_t pointer(void *p,unsigned offset)
{uintptr_t v;memcpy(&v,(char*)p+offset,sizeof(v));return v;}
static unsigned word(void *p,unsigned offset)
{unsigned v;memcpy(&v,(char*)p+offset,sizeof(v));return v;}
static uintptr_t idle_program(void *layer,unsigned sound)
{
    if(sound>=128 || !( *((unsigned char*)layer+0x2b)&1)) return 0;
    float override;memcpy(&override,(char*)layer+0xc4,4);
    if(override!=-1.f)return 0; /* Native special brightness/FX indication. */
    void *things=(void*)pointer(layer,0xc8);
    if(!things)return 0;
    void *program=(void*)pointer(things,0xb8);
    if(!program)return 0;
    /* Same checked DrumProgram cast as15396b4..15396c8. */
    program=((void*(*)(void*,void*,void*,long))(image+0xb51d78))
        (program,(void*)pointer((void*)image,0x71657d8),(void*)pointer((void*)image,0x71658a8),0);
    if(!program || ((uintptr_t)program&1))return 0;
    void *slice=(void*)pointer(things,0x140);
    if(slice && (((int(*)(void*))(image+0x29c3fc0))(slice)&1))return 0;
    void *instrument=((void*(*)(void*,int))(image+0x286bbe0))(program,(int)sound);
    if(instrument){
        if(pointer(instrument,0x2310)!=(uintptr_t)program)return 0;
        unsigned mapped=word(program,0x12a8+sound*4);
        int (*voices)(void*,void*,unsigned)=(void*)(image+0x286c6d0);
        if((voices(program,instrument,sound)&1) ||
           (mapped!=sound && (voices(program,instrument,mapped)&1)))return 0;
    }
    return (uintptr_t)program;
}
static void won(unsigned sound,uintptr_t program)
{if(sound<128)atomic_store(&background[sound],program);}
static int dispatch(void *closure,void *layer)
{
    unsigned sound=word(closure,0);
    struct origin previous=origin;
    origin=(struct origin){sound,sound<128,0,atomic_load(&native_generation)};
    int playing=pointer(layer,0)==image+0x6e10068;
    int result=native_dispatch(closure,layer);
    /* Playing's own wrappers publish its actual voice state. Every other
     * chain winner is critical, including future/unknown native layers. */
    if((result&1)&&!playing)won(sound,0);
    origin=previous;
    return result;
}
static int refresh(void *layer,unsigned sound)
{
    struct origin previous=origin;
    uintptr_t program=idle_program(layer,sound);
    origin=(struct origin){sound,sound<128,program,atomic_load(&native_generation)};
    int result=native_refresh(layer,sound);
    if(result&1)won(sound,program);
    origin=previous;
    return result;
}
static int event(void *layer,unsigned sound,unsigned a,unsigned b,float intensity)
{
    struct origin previous=origin;
    /* A native pulse/tail remains native even after its voice disappeared. */
    uintptr_t program=isfinite(intensity)&&intensity<=0?idle_program(layer,sound):0;
    origin=(struct origin){sound,sound<128,program,atomic_load(&native_generation)};
    int result=native_event(layer,sound,a,b,intensity);
    if(result&1)won(sound,program);
    origin=previous;
    return result;
}
void mixer_leds_publish(void *program,unsigned selected_bank,const uint32_t rgb[16])
{
    if(!program || ((uintptr_t)program&1) || selected_bank>=8 || !rgb){mixer_leds_disable();return;}
    atomic_fetch_add(&version,1);
    atomic_store(&published_program,(uintptr_t)program);
    atomic_store(&bank,selected_bank);
    for(unsigned i=0;i<16;++i)atomic_store(&colours[i],rgb[i]&0xffffffu);
    atomic_fetch_add(&version,1);
    atomic_store(&enabled,1);
}
void mixer_leds_disable(void){atomic_store(&enabled,0);}
void mixer_leds_reset(void){mixer_leds_disable();atomic_fetch_add(&native_generation,1);}
void mixer_leds_native_rgb(unsigned pad)
{
    if(pad>=16)return;
    if(origin.valid && origin.sound%16==pad){
        won(origin.sound,origin.background);
        atomic_store(&last_sound[pad],(origin.generation<<8)|(origin.sound+1));
    }else atomic_store(&last_sound[pad],0);
}
int mixer_leds_light(unsigned pad,uint32_t native,uint64_t *out)
{
    if(pad>=16 || !out || !atomic_load(&enabled))return 0;
    uint64_t first=atomic_load(&version);
    unsigned sound=atomic_load(&bank)*16+pad;
    uintptr_t program=atomic_load(&published_program);
    uint32_t rgb=atomic_load(&colours[pad]);
    int allowed=atomic_load(&last_sound[pad])==((atomic_load(&native_generation)<<8)|(sound+1)) &&
        atomic_load(&background[sound])==program;
    if((first&1) || first!=atomic_load(&version) || !atomic_load(&enabled))allowed=0;
    *out=(UINT64_C(0x3f800000)<<32)|(allowed?rgb:native);
    return 1;
}
int mixer_leds_install(uintptr_t base)
{
    if(image)return image==base;
    if(!base || sysconf(_SC_PAGESIZE)!=4096)return 0;
    struct hook {unsigned offset;uint32_t expected[6];void *callback;} hooks[]={
        {0x1534a00,{0xd503245f,0xaa0003e2,0xaa0103e0,0xb9400041,0xf9400002,0xf9400c42},dispatch},
        {0x1539610,{0xd503233f,0xd101c3ff,0xa9017bfd,0x910043fd,0xa90253f3,0xaa0003f3},refresh},
        {0x15398d0,{0xd503233f,0xd10183ff,0x1e3e1001,0xa9017bfd,0x910043fd,0xa90253f3},event}
    };
    for(unsigned i=0;i<3;++i)if(memcmp((void*)(base+hooks[i].offset),hooks[i].expected,24))return 0;
    unsigned char *memory=mmap(NULL,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if(memory==MAP_FAILED)return 0;
    for(unsigned i=0;i<3;++i){
        uint32_t code[10]={0xd503245f};memcpy(code+1,hooks[i].expected,24);
        code[7]=0x58000070;code[8]=0xd61f0200;code[9]=0xd503201f;
        memcpy(memory+i*128,code,40);uintptr_t continuation=base+hooks[i].offset+24;
        memcpy(memory+i*128+40,&continuation,8);
    }
    __builtin___clear_cache((char*)memory,(char*)memory+384);
    if(mprotect(memory,4096,PROT_READ|PROT_EXEC)){munmap(memory,4096);return 0;}
    uintptr_t pages[2]={(base+0x1534a00)&~(uintptr_t)4095,(base+0x1539610)&~(uintptr_t)4095};
    if(mprotect((void*)pages[0],4096,PROT_READ|PROT_WRITE)){munmap(memory,4096);return 0;}
    if(mprotect((void*)pages[1],4096,PROT_READ|PROT_WRITE)){
        if(mprotect((void*)pages[0],4096,PROT_READ|PROT_EXEC))_exit(121);
        munmap(memory,4096);return 0;
    }
    native_dispatch=(void*)memory;native_refresh=(void*)(memory+128);native_event=(void*)(memory+256);image=base;
    for(unsigned i=0;i<3;++i){
        uint32_t code[]={0xd503245f,0x58000070,0xd61f0200,0xd503201f};
        void *entry=(void*)(base+hooks[i].offset);memcpy(entry,code,16);memcpy((char*)entry+16,&hooks[i].callback,8);
        __builtin___clear_cache(entry,(char*)entry+24);
    }
    for(unsigned i=0;i<2;++i)if(mprotect((void*)pages[i],4096,PROT_READ|PROT_EXEC))_exit(121);
    return 1;
}
