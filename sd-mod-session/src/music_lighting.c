#define _GNU_SOURCE
#include "music_lighting.h"
#include "music_settings.h"
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

/* Request through the exact native lighting queue used by the layer's own
 * constructor at 0x153ad90..0x153adf4. UI never calls RefreshAll or the final
 * MIDI sender directly. The native +0x48 reference counter keeps the object
 * alive until callback 0x1538cd0 executes and releases that reference.
 * The destructor at 0x1547d44 waits for the same queued references. */
static uintptr_t base;
static pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;
#define MAX_LAYERS 8
static struct { void *layer; uint64_t previous,created,native_state,generation; } layers[MAX_LAYERS];
static uint64_t created;
static unsigned records;
#define FIELD(p,off,type) (*(type *)((char *)(p)+(off)))
#define FN(off,type) ((type)(base+(off)))
void music_lighting_init(uintptr_t b) { base=b; }
void music_lighting_created(void *p)
{
    pthread_mutex_lock(&mutex);
    for (unsigned i=0;i<MAX_LAYERS;++i) if (!layers[i].layer || layers[i].layer==p) {
        layers[i].layer=p; layers[i].previous=UINT64_MAX; layers[i].native_state=UINT64_MAX; layers[i].generation=UINT64_MAX; layers[i].created=++created;
        if(records<32)fprintf(stderr,"MUSIC_LIGHTING LAYER_CREATED slot=%u vptr_ok=%d\n",i,FIELD(p,0,uintptr_t)==base+0x6e10a60);
        break;
    }
    pthread_mutex_unlock(&mutex);
}
int music_lighting_levels_state(unsigned *enabled,unsigned *mode,unsigned *origin)
{
    *enabled=*mode=*origin=0;
    int found=0;uint64_t newest=0;unsigned best=0;
    pthread_mutex_lock(&mutex);
    /* Several layer instances can exist (one per screen that built pad
     * lighting). r27q-fix4 device log: in Chop the newest instance stayed
     * 0/1/3 (off) while the pads showed native Chop focus 0xff7700. Prefer
     * the newest ENABLED instance; fall back to the newest one. */
    for(unsigned i=0;i<MAX_LAYERS;++i){
        void *p=layers[i].layer;
        if(!p||FIELD(p,0,uintptr_t)!=base+0x6e10a60)continue;
        void *properties=FIELD(p,0xe8,void*);if(!properties)continue;
        unsigned active=__atomic_load_n((unsigned char*)properties+0x2f8,__ATOMIC_ACQUIRE)&1;
        int type=__atomic_load_n((int*)((char*)properties+0x88),__ATOMIC_ACQUIRE);
        int reference=__atomic_load_n((int*)((char*)properties+0x228),__ATOMIC_ACQUIRE);
        /* Off may reset reference to -1. Never retain a prior enabled mode
         * on that transition, or on an invalid/destroyed native source. */
        unsigned on=active&&type>=0&&type<=9&&reference>=0&&reference<16;
        if(found&&(on<best||(on==best&&layers[i].created<=newest)))continue;
        newest=layers[i].created;best=on;found=1;
        *enabled=on;
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
static _Atomic unsigned refresh_requested;
static uint64_t settle_at;
static uint64_t lighting_now_ms(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000u+(uint64_t)t.tv_nsec/1000000u;}
void music_lighting_request_refresh(void){__atomic_store_n(&refresh_requested,1,__ATOMIC_RELEASE);}
void music_lighting_tick(void)
{
    /* A native mode transition (Sample -> Chop while a voice still plays)
     * can paint before its new layer has settled. One follow-up RefreshAll
     * 300 ms after any queued transition re-emits the settled state. */
    uint64_t now_tick=lighting_now_ms();
    if(settle_at&&now_tick>=settle_at){settle_at=0;__atomic_store_n(&refresh_requested,1,__ATOMIC_RELEASE);}
    if(__atomic_exchange_n(&refresh_requested,0,__ATOMIC_ACQ_REL)){
        pthread_mutex_lock(&mutex);
        for(unsigned i=0;i<MAX_LAYERS;++i)if(layers[i].layer)layers[i].native_state=UINT64_MAX-1;
        pthread_mutex_unlock(&mutex);
        if (records<32) fprintf(stderr,"MUSIC_LIGHTING REFRESH_REQUESTED\n");
    }
    uint64_t current=music_published()^((uint64_t)!!music_pad_lighting_mode()<<63)^music_mod_settings_snapshot();
    uint64_t generation=music_generation();
    for (unsigned i=0;i<MAX_LAYERS;++i) {
        pthread_mutex_lock(&mutex);
        void *p=layers[i].layer;
        if (!p || FIELD(p,0,uintptr_t)!=base+0x6e10a60) {
            pthread_mutex_unlock(&mutex); continue;
        }
        void *properties=FIELD(p,0xe8,void*);
        if(!properties){pthread_mutex_unlock(&mutex);continue;}
        unsigned enabled=__atomic_load_n((unsigned char*)properties+0x2f8,__ATOMIC_ACQUIRE)&1;
        int mode=__atomic_load_n((int*)((char*)properties+0x88),__ATOMIC_ACQUIRE);
        int origin=__atomic_load_n((int*)((char*)properties+0x228),__ATOMIC_ACQUIRE);
        uint64_t state=enabled|((uint64_t)(unsigned)(mode+1)<<1)|((uint64_t)(unsigned)(origin+1)<<33);
        /* r27q-fix5 device log: every RefreshAll queued while Chop (mode 8)
         * is on is answered with all 16 pads = 000000 (lines 261-263, 272-274,
         * 283-287), and the pads stay dark until a pad press. The native
         * Chop entry already emits focus ff7700 and slices 000033 itself.
         * Never refresh inside Chop; leaving Chop still refreshes. */
        if(enabled&&mode==8){layers[i].native_state=state;layers[i].previous=current;layers[i].generation=generation;
            pthread_mutex_unlock(&mutex);continue;}
        /* A Chop/16 Levels transition needs native RefreshAll, not a resend
         * of original_lights cached under the previous lighting layer. */
        if(state==layers[i].native_state&&(!enabled||mode!=1||(current==layers[i].previous&&generation==layers[i].generation))){
            pthread_mutex_unlock(&mutex);continue;
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
        if(layers[i].native_state!=UINT64_MAX-1&&state!=layers[i].native_state)settle_at=now_tick+300;
        layers[i].previous=current;layers[i].native_state=state;layers[i].generation=generation;
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
            if (layers[i].layer==p){layers[i].previous=UINT64_MAX;layers[i].native_state=UINT64_MAX;}
            pthread_mutex_unlock(&mutex);
            __atomic_fetch_sub(references,1,__ATOMIC_RELEASE);
        }
        if (records++<32) fprintf(stderr,"MUSIC_LIGHTING %s\n",accepted ? "REFRESH_ALL_QUEUED" : "QUEUE_FULL_RETRY");
    }
}
