/* Sample 1.3.0.12 AC50SongSequenceController, NOT the generic SongMode.
 * 0x1578480 rebuilds its control-thread vector at +0x230, but the stock
 * loop reads only SequenceChain::Step.sequence, ignoring Step.repeats.
 * Expand there, before the unchanged native notification/cache publication.
 * No Song/XPJ data, audio-thread objects or transport indices are written.
 */
#include <stdlib.h>
static uintptr_t song_repeats_base;
static void *song_repeats_thunk;
struct song_repeats_resume { uintptr_t pc, argument; };

static struct song_repeats_resume song_repeats_prepare(void *controller)
{
    void *song=FIELD(controller,0x220,void*);
    uintptr_t first=FIELD(song,0x30,uintptr_t),last=FIELD(song,0x38,uintptr_t);
    struct song_repeats_resume fallback={song_repeats_base+0x1578664,(uintptr_t)song};
    /* Preserve native handling of unsupported/corrupt/oversized songs. The
     * UI's total-bar limit bounds a valid song to at most 0x3137 passes. */
    if(last<first || ((last-first)&7) || (last-first)/8>0x3137) return fallback;
    size_t count=0;
    for(uintptr_t p=first;p<last;p+=8){
        int repeats=FIELD((void*)p,4,int);
        if(repeats<1 || repeats>99 || count+(unsigned)repeats>0x3137) return fallback;
        count+=(unsigned)repeats;
    }
    int *data=FIELD(controller,0x230,int*);
    uintptr_t capacity=FIELD(controller,0x240,uintptr_t);
    size_t bytes=count*sizeof(int);
    if(capacity<(uintptr_t)data || capacity-(uintptr_t)data<bytes){
        /* Both the preload and the native default new/delete use the same
         * process libc heap. Allocate without throwing across the stock frame.
         * On OOM, return before publication: the last audio cache stays valid. */
        int *replacement=malloc(bytes);
        if(!replacement){
            struct song_repeats_resume fail={song_repeats_base+0x1578630,
                *(uintptr_t*)(song_repeats_base+0x7164398)};
            return fail;
        }
        if(data) ((void(*)(void*,size_t))(song_repeats_base+0xb51408))
            (data,capacity-(uintptr_t)data);
        data=replacement;
        FIELD(controller,0x230,int*)=data;
        FIELD(controller,0x240,int*)=data+count;
    }
    size_t index=0;
    for(uintptr_t p=first;p<last;p+=8){
        int sequence=FIELD((void*)p,0,int),repeats=FIELD((void*)p,4,int);
        for(int n=0;n<repeats;++n)data[index++]=sequence;
    }
    FIELD(controller,0x238,int*)=count?data+count:data;
    struct song_repeats_resume done={song_repeats_base+0x15784d0,0};
    return done;
}

static int song_repeats_install(uintptr_t image)
{
    if(song_repeats_thunk)return image==song_repeats_base;
    const uint32_t expected[]={0xa90f5bf5,0x9100c015,0xf94006a3,0xa9116bf9};
    if(memcmp((void*)(image+0x1578660),expected,sizeof expected))return 0;
    /* Sample's SequenceChain observer also ignores SetRepeats notifications.
     * Reuse its existing all-steps-changed handler (same this adjustment).
     * Otherwise K1/Undo would change the model but leave the cache stale. */
    uintptr_t *changed=(uintptr_t*)(image+0x6d8eb68+0x20);
    if(*changed!=image+0x1577e50)return 0;
    if(sysconf(_SC_PAGESIZE)!=4096)return 0;
    void *thunk=mmap(NULL,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if(thunk==MAP_FAILED)return 0;
    /* Borrow the native frame; preserve its original x21/x22 before calling C.
     * The two-word C result is returned in x0/x1 (AAPCS64). The native saved
     * signed LR and stack canary remain in the original frame. */
    const uint32_t body[]={0xd503245f,0xa90f5bf5,0xaa1403e0,0x580000b0,
        0xd63f0200,0xaa0003f0,0xaa0103e0,0xd61f0200};
    memcpy(thunk,body,sizeof body);
    FIELD(thunk,32,uintptr_t)=(uintptr_t)song_repeats_prepare;
    __builtin___clear_cache(thunk,(char*)thunk+40);
    if(mprotect(thunk,4096,PROT_READ|PROT_EXEC)){munmap(thunk,4096);return 0;}
    void *page=(void*)((image+0x1578660)&~(uintptr_t)4095);
    if(mprotect(page,4096,PROT_READ|PROT_WRITE)){munmap(thunk,4096);return 0;}
    void *vpage=(void*)((uintptr_t)changed&~(uintptr_t)4095);
    if(mprotect(vpage,4096,PROT_READ|PROT_WRITE)){
        if(mprotect(page,4096,PROT_READ|PROT_EXEC))_exit(121);
        munmap(thunk,4096);return 0;
    }
    song_repeats_base=image;
    song_repeats_thunk=thunk;
    const uint32_t jump[]={0x58000050,0xd61f0200};
    memcpy((void*)(image+0x1578660),jump,sizeof jump);
    FIELD((void*)image,0x1578668,uintptr_t)=(uintptr_t)thunk;
    *changed=image+0x1578a00;
    __builtin___clear_cache((char*)image+0x1578660,(char*)image+0x1578670);
    if(mprotect(page,4096,PROT_READ|PROT_EXEC))_exit(121);
    if(mprotect(vpage,4096,PROT_READ))_exit(121);
    return 1;
}
