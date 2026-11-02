#include "music_lighting.h"
#include "music_settings.h"
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>

/* Request through the exact native lighting queue used by the layer's own
 * constructor at 0x153ad90..0x153adf4. UI never calls RefreshAll or the final
 * MIDI sender directly. The native +0x48 reference counter keeps the object
 * alive until callback 0x1538cd0 executes and releases that reference.
 * The destructor at 0x1547d44 waits for the same queued references. */
static uintptr_t base;
static pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;
#define MAX_LAYERS 8
static struct { void *layer; uint64_t previous,created; } layers[MAX_LAYERS];
static uint64_t created;
static unsigned records;
#define FIELD(p,off,type) (*(type *)((char *)(p)+(off)))
#define FN(off,type) ((type)(base+(off)))
void music_lighting_init(uintptr_t b) { base=b; }
void music_lighting_created(void *p)
{
    pthread_mutex_lock(&mutex);
    for (unsigned i=0;i<MAX_LAYERS;++i) if (!layers[i].layer || layers[i].layer==p) {
        layers[i].layer=p; layers[i].previous=UINT64_MAX; layers[i].created=++created; break;
    }
    pthread_mutex_unlock(&mutex);
}
int music_lighting_levels_state(unsigned *enabled,unsigned *mode,unsigned *origin)
{
    *enabled=*mode=*origin=0;
    int found=0;uint64_t newest=0;
    pthread_mutex_lock(&mutex);
    for(unsigned i=0;i<MAX_LAYERS;++i){
        void *p=layers[i].layer;
        if(!p||layers[i].created<=newest||FIELD(p,0,uintptr_t)!=base+0x6e10a60)continue;
        void *properties=FIELD(p,0xe8,void*);if(!properties)continue;
        newest=layers[i].created;found=1;
        unsigned active=__atomic_load_n((unsigned char*)properties+0x2f8,__ATOMIC_ACQUIRE)&1;
        int type=__atomic_load_n((int*)((char*)properties+0x88),__ATOMIC_ACQUIRE);
        int reference=__atomic_load_n((int*)((char*)properties+0x228),__ATOMIC_ACQUIRE);
        /* Off may reset reference to -1. Never retain a prior enabled mode
         * on that transition, or on an invalid/destroyed native source. */
        *enabled=active&&type>=0&&type<=9&&reference>=0&&reference<16;
        *mode=type>=0&&type<=9?(unsigned)type:0;
        *origin=reference>=0&&reference<16?(unsigned)reference:0;
    }
    pthread_mutex_unlock(&mutex);
    return found;
}
void music_lighting_destroyed(void *p)
{
    /* Acquire before native destruction changes its vtable or frees fields.
     * Scheduling takes the native lifetime reference while holding this lock. */
    pthread_mutex_lock(&mutex);
    for (unsigned i=0;i<MAX_LAYERS;++i) if (layers[i].layer==p) {
        layers[i].layer=NULL; layers[i].previous=UINT64_MAX;
    }
    pthread_mutex_unlock(&mutex);
}
void music_lighting_tick(void)
{
    uint64_t current=music_published()^((uint64_t)!!music_pad_lighting_mode()<<63);
    for (unsigned i=0;i<MAX_LAYERS;++i) {
        pthread_mutex_lock(&mutex);
        void *p=layers[i].layer;
        if (!p || current==layers[i].previous || FIELD(p,0,uintptr_t)!=base+0x6e10a60) {
            pthread_mutex_unlock(&mutex); continue;
        }
        void *properties=FIELD(p,0xe8,void*);
        if (!properties || !(__atomic_load_n((unsigned char*)properties+0x2f8,__ATOMIC_ACQUIRE)&1)
            || __atomic_load_n((int*)((char*)properties+0x88),__ATOMIC_ACQUIRE)!=1) {
            pthread_mutex_unlock(&mutex); continue;
        }
        void *queue_owner=FIELD(p,0x40,void*);
        if (!queue_owner) { pthread_mutex_unlock(&mutex); continue; }
        void *token=(char*)p+0x40;
        struct {
            uintptr_t invoke,manage;
            void *reference,*object;
        } action={base+0x1538cd0,base+0x1538b10,token,p};
        _Static_assert(sizeof(action)==32,"Pinned native function payload");
        unsigned *references=(unsigned*)((char*)p+0x48);
        __atomic_fetch_add(references,1,__ATOMIC_ACQUIRE);
        layers[i].previous=current;
        /* The native reference now protects the object. Do not hold our
         * registry mutex while acquiring a native queue mutex. */
        pthread_mutex_unlock(&mutex);
        void *queue_mutex=(char*)queue_owner+0x220;
        FN(0xf40e50,void(*)(void*))(queue_mutex);
        int accepted=FN(0x2b59300,int(*)(void*,const void*,uintptr_t))
            ((char*)queue_owner+0x1d8,&action,(uintptr_t)token&~(uintptr_t)3)&255;
        FN(0xf40e90,void(*)(void*))(queue_mutex);
        if (!accepted) {
            pthread_mutex_lock(&mutex);
            if (layers[i].layer==p) layers[i].previous=UINT64_MAX;
            pthread_mutex_unlock(&mutex);
            __atomic_fetch_sub(references,1,__ATOMIC_RELEASE);
        }
        if (records++<32) fprintf(stderr,"MUSIC_LIGHTING %s\n",accepted ? "REFRESH_ALL_QUEUED" : "QUEUE_FULL_RETRY");
    }
}
