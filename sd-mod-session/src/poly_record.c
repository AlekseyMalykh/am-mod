#define _GNU_SOURCE
#include "poly_record.h"
#include "step_native.h"
#ifdef MPC_CHORDS
#include "chords.h"
#endif
#include <stdio.h>
#include <stdatomic.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <pthread.h>
#define F(p,o,T) (*(T*)((char*)(p)+(o)))
static uintptr_t image;
static uintptr_t input_original;
static _Atomic uint64_t context;
/* Lab debugger reads counters without logging or allocating in the audio thread. */
static _Atomic unsigned input_stats[4];
#define CHOICE_MARK 0x4d000000u
static _Thread_local unsigned releasing,release_choice;
static _Thread_local struct {void *mode;uint64_t context;unsigned held;unsigned char choice[16];} keys;
#ifdef MPC_CHORDS
static _Thread_local struct {void *mode;uint64_t context;struct chord_voices voices;} chord_keys;
/* Arpeggiator handoff. Guarded by arp_mutex; native pointers are only used
 * while the same generation is current (cleared when Chords closes). */
static pthread_mutex_t arp_mutex=PTHREAD_MUTEX_INITIALIZER;
static struct {void *engine,*track;unsigned note;int count;int notes[16];float velocity;} arp;
static void arp_add(void *engine,void *track,unsigned note,const int *pitches,int n,float velocity){
    pthread_mutex_lock(&arp_mutex);
    if(arp.engine!=engine||arp.track!=track||arp.note!=note)arp.count=0;
    arp.engine=engine;arp.track=track;arp.note=note;arp.velocity=velocity;
    for(int i=0;i<n;++i){
        int dup=0;
        for(int j=0;j<arp.count;++j)dup|=arp.notes[j]==pitches[i];
        if(!dup&&arp.count<16)arp.notes[arp.count++]=pitches[i];
    }
    pthread_mutex_unlock(&arp_mutex);
}
static void arp_remove(const int *pitches,int n){
    pthread_mutex_lock(&arp_mutex);
    for(int i=0;i<n;++i){
        for(int j=0;j<arp.count;++j){
            if(arp.notes[j]!=pitches[i])continue;
            for(int k=j+1;k<arp.count;++k)arp.notes[k-1]=arp.notes[k];
            --arp.count;
            break;
        }
    }
    pthread_mutex_unlock(&arp_mutex);
}
static void chord_record_off(void *engine,void *track,unsigned note,unsigned choice);
static void arp_send(int pitch,int on,float velocity){
    struct step_event event=((struct step_event(*)(unsigned,int64_t,int64_t,unsigned,unsigned,float))(image+0x27e54d0))(arp.note,0,0,100,0,on?velocity:1.f);
    ((void(*)(void*,int,int,int))(image+0x27e4400))((char*)&event+0x14,0,0,pitch);
    if(on)((void(*)(void*,int,void*,int))(image+0x211b090))(arp.engine,0,&event,0x7ffffffa);
    else chord_record_off(arp.engine,arp.track,arp.note,F(&event,0x16,unsigned char));
}
int poly_record_arp_notes(int *notes,int max,float *velocity){
    pthread_mutex_lock(&arp_mutex);
    int n=arp.engine&&chords_active()?arp.count:0;
    if(n>max)n=max;
    for(int i=0;i<n;++i)notes[i]=arp.notes[i];
    if(velocity)*velocity=arp.velocity;
    pthread_mutex_unlock(&arp_mutex);return n;
}
int poly_record_arp_emit(int pitch,int on,float velocity){
    pthread_mutex_lock(&arp_mutex);
    int ok=image&&arp.engine&&arp.track&&(chords_active()||!on);
    if(ok)arp_send(pitch,on,velocity);
    pthread_mutex_unlock(&arp_mutex);return ok;
}
static void chord_record_off(void *engine,void *track,unsigned note,unsigned choice){
    if(((int(*)(void*))(image+0x2133800))(F(engine,0x768,void*))&1){
        void *recorder=((void*(*)(void*,void*))(image+0x20f8da0))(F(engine,0x778,void*),track);
        int destination=recorder?((int(*)(void*))(image+0x20e2290))(recorder):-1;
        int midi=((int(*)(void*,int,int,void*))(image+0x2114fc0))(engine,(int)note,0x7ffffffa,track);
        if(destination>=0&&midi>=0&&midi<128)
            ((void(*)(void*,int,int,unsigned))(image+0x20e4da0))(recorder,1,destination,CHOICE_MARK|(choice<<8)|(unsigned)midi);
    }
}
static int chord_pad(void*mode,int pad,float velocity,uint64_t snapshot){
    int enabled=chords_active();
    if(!enabled&&!chord_keys.voices.held)return 0;
    if(chord_keys.mode!=mode||chord_keys.context!=snapshot){
        memset(&chord_keys.voices,0,sizeof(chord_keys.voices));
        chord_keys.mode=mode;chord_keys.context=snapshot;
    }
    if(pad<0||pad>=8)return enabled;
    if(!enabled&&velocity>0)return 1; /* Finish held chord keys before a new route. */
    void *track=F(F(mode,0x58,void*),0x140,void*);
    if(!track||(((int(*)(void*))(image+0x29c3f80))(track)&1))return 1;
    struct chord_config config;chords_get(&config);
    int arpeggiated=chords_arp_pattern(&config)!=ARP_OFF;
    if(!arpeggiated){pthread_mutex_lock(&arp_mutex);arp.count=0;pthread_mutex_unlock(&arp_mutex);}
    int pitches[CHORD_NOTES];unsigned before=chord_keys.voices.held;
    int count=chords_transition(&chord_keys.voices,&config,(unsigned)pad,velocity>0,pitches);
    void *engine=F(F(mode,0x68,void*),0xd8,void*);
    unsigned note=F(F(mode,0x50,void*),0x158,unsigned);
    if(arpeggiated){
        /* Arpeggiator (or Note Repeat): the UI timer sounds these one step at a time. */
        /* A fresh gesture starts an empty pool: notes of a key-up that never
         * arrived are not mixed into the new arpeggio. */
        if(velocity>0&&!before){pthread_mutex_lock(&arp_mutex);arp.count=0;pthread_mutex_unlock(&arp_mutex);}
        if(velocity>0)arp_add(engine,track,note,pitches,count,velocity);else arp_remove(pitches,count);
        chords_roots(chord_keys.voices.held,velocity>0?pad:-1);
        return 1;
    }
    for(int i=0;i<count;++i){
        struct step_event event=((struct step_event(*)(unsigned,int64_t,int64_t,unsigned,unsigned,float))(image+0x27e54d0))(note,0,0,100,0,velocity>0?velocity:1.f);
        ((void(*)(void*,int,int,int))(image+0x27e4400))((char*)&event+0x14,0,0,pitches[i]);
        if(velocity>0)((void(*)(void*,int,void*,int))(image+0x211b090))(engine,0,&event,0x7ffffffa);
        else chord_record_off(engine,track,note,F(&event,0x16,unsigned char));
    }
    if(before&&!chord_keys.voices.held){
        ++releasing;((void(*)(void*,int,int,int,int))(image+0x2118930))(engine,1,note,0x7ffffffa,0);--releasing;
    }
    chords_roots(chord_keys.voices.held,velocity>0?pad:-1);
    return 1;
}
#endif
void poly_record_context(void *things){
    static void *previous;
    static unsigned previous_note=~0u,previous_poly=~0u,generation;
    void *program=things?F(things,0xb8,void*):NULL;
    void *instrument=things?F(things,0x668,void*):NULL;
    int pad=program&&instrument?((int(*)(void*,void*))(image+0x286bc20))(program,instrument):-1;
    /* SixteenLevelProperties+158 is the pad index, before Engine translates
     * it through Program+12a8 into a MIDI/sound note. */
    unsigned note=pad>=0&&pad<128?(unsigned)pad:~0u;
    unsigned poly=instrument&&F(instrument,0x1690,int)==0&&note<128;
    if(previous!=instrument||previous_note!=note||previous_poly!=poly){
        previous=instrument;previous_note=note;previous_poly=poly;++generation;
        atomic_store_explicit(&context,((uint64_t)generation<<16)|((uint64_t)(note&255)<<8)|poly,memory_order_release);
    }
}
static void input_pad(void *mode,int pad,float velocity){
    atomic_fetch_add_explicit(&input_stats[0],1,memory_order_relaxed);
    void *properties=F(mode,0x50,void*);
    uint64_t snapshot=atomic_load_explicit(&context,memory_order_acquire);
#ifdef MPC_CHORDS
    if(chords_active()&&(!(snapshot&1)||!properties||F(properties,0x88,int)!=1||
        F(properties,0x158,unsigned)!=((snapshot>>8)&255)))return;
    /* A note held before Chop opened still belongs to normal 16 Levels. */
    int old_key=pad>=0&&pad<16&&keys.mode==mode&&keys.context==snapshot&&(keys.held&(1u<<pad));
    if(old_key&&chords_active()&&velocity>0)return;
    if(!old_key&&chord_pad(mode,pad,velocity,snapshot))return;
#endif
    if(!(snapshot&1)||pad<0||pad>=16||!properties||F(properties,0x88,int)!=1||
        F(properties,0x158,unsigned)!=((snapshot>>8)&255)||
        (F(F(mode,0x48,void*),0x50,unsigned char)&1)){
        atomic_fetch_add_explicit(&input_stats[1],1,memory_order_relaxed);
        ((void(*)(void*,int,float))input_original)(mode,pad,velocity);return;
    }
    void *track=F(F(mode,0x58,void*),0x140,void*);
    if(!track||(((int(*)(void*))(image+0x29c3f80))(track)&1))return;
    if(keys.mode!=mode||keys.context!=snapshot){keys.mode=mode;keys.context=snapshot;keys.held=0;}
    void *engine=F(F(mode,0x68,void*),0xd8,void*);
    unsigned note=F(properties,0x158,unsigned);
    if(velocity>0){
        atomic_fetch_add_explicit(&input_stats[2],1,memory_order_relaxed);
        struct step_event event=((struct step_event(*)(unsigned,int64_t,int64_t,unsigned,unsigned,float))(image+0x27e54d0))(note,0,0,100,0,velocity);
        ((void(*)(void*,int,void*))(image+0x21472f0))(properties,pad,&event);
        keys.choice[pad]=F(&event,0x16,unsigned char);keys.held|=1u<<pad;
        ((void(*)(void*,int,void*,int))(image+0x211b090))(engine,0,&event,0x7ffffffa);
    }else if(keys.held&(1u<<pad)){
        atomic_fetch_add_explicit(&input_stats[3],1,memory_order_relaxed);
        keys.held&=~(1u<<pad);release_choice=keys.choice[pad];
        /* Engine's held-note map has one entry per source sound, so its first
         * NoteOff removes the route needed by later chord releases. Resolve
         * the current native clip recorder for every physical release. */
        if(((int(*)(void*))(image+0x2133800))(F(engine,0x768,void*))&1){
            void *recorder=((void*(*)(void*,void*))(image+0x20f8da0))(F(engine,0x778,void*),track);
            int destination=recorder?((int(*)(void*))(image+0x20e2290))(recorder):-1;
            int midi=((int(*)(void*,int,int,void*))(image+0x2114fc0))(engine,(int)note,0x7ffffffa,track);
            if(destination>=0&&midi>=0&&midi<128)
                ((void(*)(void*,int,int,unsigned))(image+0x20e4da0))(recorder,1,destination,CHOICE_MARK|(release_choice<<8)|(unsigned)midi);
        }
        if(!keys.held){
            ++releasing;
            ((void(*)(void*,int,int,int,int))(image+0x2118930))(engine,1,note,0x7ffffffa,0);
            --releasing;
        }
    }
}
/* PadControls dispatches physical releases through vtable+20, not through
 * the +18 velocity callback with zero. Stock SixteenLevels leaves +20 empty.
 * Only keys actually owned by our Poly/Chords path need a matching NoteOff. */
static void input_release(void *mode,int pad){
    int held=pad>=0&&pad<16&&keys.mode==mode&&(keys.held&(1u<<pad));
#ifdef MPC_CHORDS
    held|=pad>=0&&pad<8&&chord_keys.mode==mode&&(chord_keys.voices.held&(1u<<pad));
#endif
    if(held)input_pad(mode,pad,0.f);
    else ((void(*)(void*,int))(image+0x1545920))(mode,pad);
}
/* This marker exists only in the native record-queue NoteOff integer. It
 * never enters the audio event, a pending event, the project or saved XPJ. */
static void enqueue_off(void *recorder,int source,int destination,unsigned note){
    if(!releasing)((void(*)(void*,int,int,unsigned))(image+0x20e4da0))(recorder,source,destination,note);
}
static void finish_note(void *recorder,void *program,int destination,unsigned note,
                        uintptr_t a,uintptr_t b,uintptr_t c,unsigned d,int flags){
    if((note&0xffff0000u)==CHOICE_MARK){
        unsigned choice=(note>>8)&255;note&=255;
        void *header=(char*)recorder+0x1a0,*found=NULL;
        unsigned limit=4096;
        for(void *node=F(header,0,void*);node!=header&&limit--;node=F(node,0,void*)){
            if(F(node,0x48,void*)==program&&F(node,0x50,int)==destination&&
               F(node,0x18,unsigned char)==3&&F(node,0x40,unsigned char)==note&&
               F(node,0x41,unsigned char)==0&&F(node,0x26,unsigned char)==choice){found=node;break;}
        }
        if(!found)return; /* Never release a different pitch on a stale key-up. */
        if(found!=F(header,0,void*)){
            void *previous=F(found,8,void*),*next=F(found,0,void*),*first=F(header,0,void*);
            F(previous,0,void*)=next;F(next,8,void*)=previous;
            F(found,0,void*)=first;F(found,8,void*)=header;
            F(first,8,void*)=found;F(header,0,void*)=found;
        }
    }
    ((void(*)(void*,void*,int,unsigned,uintptr_t,uintptr_t,uintptr_t,unsigned,int))(image+0x213b130))
        (recorder,program,destination,note,a,b,c,d,flags);
}
static int install_input(void){
    const uint32_t expected[]={0xd503233f,0xa9bd7bfd,0x910003fd,0xa90153f3};
    void *entry=(void*)(image+0x1545050);
    if(memcmp(entry,expected,sizeof(expected)))return 0;
    void *code=mmap(NULL,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if(code==MAP_FAILED)return 0;
    uint32_t trampoline[]={0xd503245f,expected[0],expected[1],expected[2],expected[3],0x58000070,0xd61f0200,0xd503201f};
    memcpy(code,trampoline,sizeof(trampoline));F(code,32,uintptr_t)=image+0x1545060;
    __builtin___clear_cache(code,(char*)code+40);
    if(mprotect(code,4096,PROT_READ|PROT_EXEC)){munmap(code,4096);return 0;}
    void *page=(void*)((uintptr_t)entry&~(uintptr_t)4095);
    if(mprotect(page,4096,PROT_READ|PROT_WRITE)){munmap(code,4096);return 0;}
    input_original=(uintptr_t)code;
    const uint32_t jump[]={0x58000050,0xd61f0200};memcpy(entry,jump,8);F(entry,8,uintptr_t)=(uintptr_t)input_pad;
    __builtin___clear_cache(entry,(char*)entry+16);
    if(mprotect(page,4096,PROT_READ|PROT_EXEC))_exit(121);
    return 1;
}
/* Only the SequenceRecorder's overwrite lookup is changed. Native ordering,
 * note storage, undo, velocity and duration remain native. Two simultaneous
 * pitches use the same sound note (+30), but distinct modifier data (+14). */
static int same_choice(const void *a,const void *b){
    return F(a,0x31,unsigned char)==F(b,0x31,unsigned char)&&
        !memcmp((const char*)a+0x14,(const char*)b+0x14,20);
}
static void *find_choice(void *tree,const void *event,int selected){
    void *end=(char*)tree+8;
    void *node=((void*(*)(void*,const void*))(image+(selected?0x27cfcd0:0x27cf8d0)))(tree,event);
    if(node==end||F(event,8,unsigned char)!=3)return node;
    for(;node!=end;node=((void*(*)(void*))(image+0xb51738))(node)){
        const void *candidate=(const char*)node+0x20;
        if(F(candidate,0,int64_t)!=F(event,0,int64_t))break;
        if(F(candidate,8,unsigned char)==3&&
           F(candidate,0x30,unsigned char)==F(event,0x30,unsigned char)&&
           (!selected||!((F(candidate,8,unsigned)^F(event,8,unsigned))&0x100u))&&
           same_choice(candidate,event))return node;
    }
    return end;
}
static void *find_recorded(void *tree,const void *event){return find_choice(tree,event,0);}
static void *find_added(void *tree,const void *event){return find_choice(tree,event,1);}
int poly_record_install(uintptr_t base){
    if(image)return image==base;
    if(!base||sysconf(_SC_PAGESIZE)!=4096)return 0;
    /* Pinned SixteenLevels virtual release; never replace the shared empty
     * native function itself, which belongs to other modes too. */
    uintptr_t *release_slot=(uintptr_t*)(base+0x6d8dc48);
    if(*release_slot!=base+0x1545920)return 0;
    const uintptr_t sites[]={0x213aadc,0x213a5e0,0x21188e0,0x20df4f8,0x2137f58,0x2137c44};
    const uintptr_t targets[]={0x27cf8d0,0x27cf8d0,0x20e4da0,0x213b130,0x27cfcd0,0x27cf8d0};
    for(unsigned i=0;i<6;++i){
        uint32_t before=0x94000000u|((uint32_t)((targets[i]-sites[i])/4)&0x3ffffffu);
        if(F((void*)base,sites[i],uint32_t)!=before)return 0;
    }
    uintptr_t site=base+sites[0];
    void *relay=MAP_FAILED;
    for(uintptr_t off=0x7600000;off<0x9600000;off+=0x10000){
        uintptr_t address=base+off;
        if(address-site>=0x8000000)break;
        relay=mmap((void*)address,4096,PROT_READ|PROT_WRITE,
            MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
        if(relay!=MAP_FAILED)break;
    }
    if(relay==MAP_FAILED)return 0;
    uint32_t code[]={0xd503245f,0x58000070,0xd61f0200,0xd503201f};
    for(unsigned i=0;i<6;++i){
        memcpy((char*)relay+i*32,code,sizeof(code));
        F(relay,i*32+16,uintptr_t)=i==2?(uintptr_t)enqueue_off:i==3?(uintptr_t)finish_note:i==4?(uintptr_t)find_added:(uintptr_t)find_recorded;
    }
    __builtin___clear_cache(relay,(char*)relay+192);
    if(mprotect(relay,4096,PROT_READ|PROT_EXEC)){munmap(relay,4096);return 0;}
    image=base;
    for(unsigned i=0;i<6;++i){
        site=base+sites[i];void *page=(void*)(site&~(uintptr_t)4095);
        if(mprotect(page,4096,PROT_READ|PROT_WRITE))_exit(121);
        F((void*)site,0,uint32_t)=0x94000000u|((uint32_t)(((uintptr_t)relay+i*32-site)/4)&0x3ffffffu);
        __builtin___clear_cache((char*)site,(char*)site+4);
        if(mprotect(page,4096,PROT_READ|PROT_EXEC))_exit(121);
    }
    if(!install_input())_exit(121);
    void *release_page=(void*)((uintptr_t)release_slot&~(uintptr_t)4095);
    if(mprotect(release_page,4096,PROT_READ|PROT_WRITE))_exit(121);
    *release_slot=(uintptr_t)input_release;
    if(mprotect(release_page,4096,PROT_READ))_exit(121);
    fprintf(stderr,"POLY_RECORD PITCH_AWARE_OVERWRITE_INSTALLED HELD_PAD_INPUT PHYSICAL_RELEASE_BOUND\n");
    return 1;
}
