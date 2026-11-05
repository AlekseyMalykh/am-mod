#define _GNU_SOURCE
/* Serialises "release every voice of a sound" with the audio render.
 *
 * Stop pressed twice broadcasts all-sounds-off on the UI thread. Each sound
 * player then walks its active-voice vector (+0x2838) and frees the voices
 * (0x2868eb0) while the audio thread may be inside the same vector in its
 * render loop (0x286c2a0). A voice released by both threads ends up with a
 * NULL owner (SIGSEGV at owner+0x1234) or a destroyed one (pure virtual).
 * Chords keep several voices per sound alive, which makes the window wide.
 *
 * The audio thread holds a recursive lock for the whole engine render
 * (0x18d7630 from the live audio callback); release-all takes the same lock. The UI side
 * waits at most 250 ms and otherwise runs unguarded as before, so a native
 * lock order we cannot see can never deadlock the device. */
#include "voice_guard.h"
#ifdef MPC_REAR_ROUTING
#include "rear_native.h"
#endif
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#define F(p,o,T) (*(T*)((char*)(p)+(o)))
static uintptr_t image,release_original;
static pthread_mutex_t render_lock;
static _Atomic unsigned guard_stats[3];
static void *audio_render(void *engine,void *a,void *b){
    pthread_mutex_lock(&render_lock);
#ifdef MPC_REAR_ROUTING
    void *r=rear_native_process(engine,a,b,(rear_render_fn)(image+0x18d7630));
#else
    void *r=((void*(*)(void*,void*,void*))(image+0x18d7630))(engine,a,b);
#endif
    pthread_mutex_unlock(&render_lock);
    return r;
}
static void release_all(void *player){
    struct timespec until;clock_gettime(CLOCK_REALTIME,&until);
    until.tv_nsec+=250000000;if(until.tv_nsec>=1000000000){until.tv_nsec-=1000000000;++until.tv_sec;}
    int locked=pthread_mutex_timedlock(&render_lock,&until)==0;
    atomic_fetch_add_explicit(&guard_stats[locked?0:1],1,memory_order_relaxed);
    ((void(*)(void*))release_original)(player);
    if(locked)pthread_mutex_unlock(&render_lock);
}
/* Other non-audio threads that start or stop voices (the Chords
 * arpeggiator timer) take the same lock as the render. No-op until installed. */
void voice_guard_lock(void){if(image)pthread_mutex_lock(&render_lock);}
void voice_guard_unlock(void){if(image)pthread_mutex_unlock(&render_lock);}
void voice_guard_stats(unsigned out[3]){for(int i=0;i<3;++i)out[i]=atomic_load_explicit(&guard_stats[i],memory_order_relaxed);}
static int patch_bl(uintptr_t site,uintptr_t target,uintptr_t to){
    uint32_t want=0x94000000u|((uint32_t)((target-site)/4)&0x3ffffffu);
    if(F((void*)site,0,uint32_t)!=want)return 0;
    int64_t d=(int64_t)to-(int64_t)site;
    if(d< -0x8000000||d>=0x8000000)return 0;
    void *page=(void*)(site&~(uintptr_t)4095);
    if(mprotect(page,4096,PROT_READ|PROT_WRITE))_exit(121);
    F((void*)site,0,uint32_t)=0x94000000u|((uint32_t)(d/4)&0x3ffffffu);
    __builtin___clear_cache((char*)site,(char*)site+4);
    if(mprotect(page,4096,PROT_READ|PROT_EXEC))_exit(121);
    return 1;
}
int voice_guard_install(uintptr_t base){
    if(image)return image==base;
    if(!base||sysconf(_SC_PAGESIZE)!=4096)return 0;
    const uint32_t expected[]={0xd503233f,0xa9bd7bfd,0x910003fd,0xa90153f3};
    void *entry=(void*)(base+0x2868eb0);
    if(memcmp(entry,expected,sizeof(expected)))return 0;
    const uintptr_t site=0x14e9cfc;
    if(F((void*)base,site,uint32_t)!=(0x94000000u|((uint32_t)((0x18d7630-site)/4)&0x3ffffffu)))return 0;
    pthread_mutexattr_t attr;pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr,PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&render_lock,&attr);pthread_mutexattr_destroy(&attr);
    /* Relay page within BL range of the render call sites. */
    void *relay=MAP_FAILED;
    for(uintptr_t off=0x7700000;off-site<0x7ff0000;off+=0x10000){
        relay=mmap((void*)(base+off),4096,PROT_READ|PROT_WRITE,
            MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
        if(relay!=MAP_FAILED)break;
    }
    if(relay==MAP_FAILED)return 0;
    /* +0: relay to audio_render. +32: original release-all prologue + jump back. */
    const uint32_t jump[]={0xd503245f,0x58000070,0xd61f0200,0xd503201f};
    memcpy(relay,jump,sizeof(jump));F(relay,16,uintptr_t)=(uintptr_t)audio_render;
    uint32_t original[]={0xd503245f,expected[0],expected[1],expected[2],expected[3],0x58000070,0xd61f0200,0xd503201f};
    memcpy((char*)relay+32,original,sizeof(original));F(relay,64,uintptr_t)=base+0x2868ec0;
    __builtin___clear_cache(relay,(char*)relay+80);
    if(mprotect(relay,4096,PROT_READ|PROT_EXEC)){munmap(relay,4096);return 0;}
    image=base;release_original=(uintptr_t)relay+32;
    if(!patch_bl(base+site,base+0x18d7630,(uintptr_t)relay))_exit(121);
    void *page=(void*)((uintptr_t)entry&~(uintptr_t)4095);
    if(mprotect(page,4096,PROT_READ|PROT_WRITE))_exit(121);
    const uint32_t hook[]={0x58000050,0xd61f0200};memcpy(entry,hook,8);F(entry,8,uintptr_t)=(uintptr_t)release_all;
    __builtin___clear_cache(entry,(char*)entry+16);
    if(mprotect(page,4096,PROT_READ|PROT_EXEC))_exit(121);
    fprintf(stderr,"VOICE_GUARD RENDER_RELEASE_SERIALISED\n");
    return 1;
}
