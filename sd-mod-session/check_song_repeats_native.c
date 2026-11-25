/* Actual Sample list builder -> native notification -> deferred control task
 * -> native vector cache copy -> native audio queue/swap -> native list lookup.
 * The controller/queues/weak owners/executor are fixtures; no app or device. */
#define main unused_fx_main
#include "check_fx_native.c"
#undef main
#include <pthread.h>
#include <stdatomic.h>
#include <time.h>
#define FIELD(p,o,t) (*(t*)((char*)(p)+(o)))
static int fail_allocation;
static void *repeat_malloc(size_t n){return fail_allocation?NULL:malloc(n);}
#define malloc repeat_malloc
#include "src/song_repeats.inc"
#undef malloc
static _Alignas(16) unsigned char controller[0x280],song[0x80],queues[0x300],slots[8][0xc0];
static _Alignas(16) unsigned char cache_owner[0x20],source_owner[0x20],task[0x100];
static int task_pending;
static uint64_t reader;
static atomic_int running;
static int transition_count,requested_sequence;
static void flush_position_fixture(void*p,int64_t tick,int mode){(void)p;(void)tick;(void)mode;}
static void transition_fixture(void*p){++transition_count;requested_sequence=FIELD(p,0x5e0,int);}
static void patch_fixture(uintptr_t offset,void*callback){
    void*page=(void*)((base+offset)&~(uintptr_t)4095);
    assert(!mprotect(page,4096,PROT_READ|PROT_WRITE));
    const uint32_t jump[]={0x58000050,0xd61f0200};
    memcpy((void*)(base+offset),jump,8);FIELD((void*)base,offset+8,void*)=callback;
    __builtin___clear_cache((char*)base+offset,(char*)base+offset+16);
    assert(!mprotect(page,4096,PROT_READ|PROT_EXEC));
}
static void check_native_next(const int*sequence,size_t count){
    _Alignas(8) unsigned char sequencer[0x800]={0},timeline[0x200]={0};
    uintptr_t timeline_vt[4]={0,0,0,base+0x1445640};
    FIELD(timeline,0,void*)=timeline_vt;FIELD(timeline,0x1f0,unsigned char)=1;
    FIELD(sequencer,0x790,void*)=timeline;FIELD(sequencer,0x418,void*)=controller+0x30;
    FIELD(controller,0x30,uintptr_t)=base+0x6d8ead8;
    // Real end-of-sequence handler, real Sample virtual list getter. Voice
    // flush and downstream DSP/sequence activation are explicit test sinks.
    for(size_t n=1;n<=count;++n){
        FIELD(sequencer,0x390,int)=(int)n-1;transition_count=0;
        FIELD(sequencer,0x5f8,unsigned char)=0; // The activation sink consumed the previous pending change.
        FN(0x2120590,void(*)(void*))(sequencer);
        assert(transition_count==1&&requested_sequence==(n<count?sequence[n]:-1));
    }
    FIELD(timeline,0x1f0,unsigned char)=0;transition_count=0;
    FN(0x2120590,void(*)(void*))(sequencer);assert(!transition_count);
}
static int defer(void *self,void *action,uintptr_t key){
    (void)self;(void)key;assert(!task_pending);
    /* This native closure advertises trivial relocation (manager operation 2).
     * Transfer it as the executor does, leaving an empty source function. */
    assert(((int(*)(int,void*,void*))FIELD(action,8,uintptr_t))(2,(char*)action+16,NULL));
    memcpy(task,action,48);
    FIELD(action,0,uintptr_t)=base+0xb57a7c;
    FIELD(action,8,uintptr_t)=base+0x15265b0;
    task_pending=1;return 1;
}
static void run_audio(void){
    while(reader!=__atomic_load_n((uint64_t*)(queues+0x1f8),__ATOMIC_ACQUIRE))
        FN(0x2b59200,void(*)(void*,uint64_t*))(queues+0x1d8,&reader);
}
static void reclaim(void){FN(0x2b58c70,void(*)(void*))(queues+0x1d8);}
static void *audio_worker(void *unused){
    (void)unused;struct timespec pause={0,1000000};
    while(atomic_load(&running)){run_audio();nanosleep(&pause,NULL);}return NULL;
}
static void publish(void){
    phase="native source notification";
    run_audio();assert(task_pending);reclaim();
    pthread_t worker;atomic_store(&running,1);assert(!pthread_create(&worker,NULL,audio_worker,NULL));
    phase="native vector copy/control task";
    ((void(*)(void*))FIELD(task,0,uintptr_t))(task+16);
    atomic_store(&running,0);assert(!pthread_join(worker,NULL));
    run_audio();
    ((int(*)(int,void*,void*))FIELD(task,8,uintptr_t))(1,task+16,NULL);
    memset(task,0,sizeof task);task_pending=0;reclaim();
}
static void rebuild(void){FN(0x1578480,void(*)(void*))(controller);}
static void expect(const int *sequence,size_t count){
    assert(FN(0x1577ea0,unsigned(*)(void*))(controller+0x30)==count);
    for(size_t n=0;n<count;++n)assert(FN(0x1577f50,int(*)(void*,unsigned))(controller+0x30,(unsigned)n)==sequence[n]);
    assert(FN(0x1577f50,int(*)(void*,unsigned))(controller+0x30,(unsigned)count)==-1);
    if(count)check_native_next(sequence,count);
}
static void steps(int (*rows)[2],size_t count){
    FIELD(song,0x30,void*)=rows;FIELD(song,0x38,void*)=rows+count;
    FIELD(controller,0x220,void*)=song;
}
int main(int argc,char **argv){
    assert(argc==2);setbuf(stdout,NULL);
    struct sigaction sa={.sa_sigaction=fault,.sa_flags=SA_SIGINFO};
    sigaction(SIGSEGV,&sa,NULL);sigaction(SIGABRT,&sa,NULL);sigaction(SIGILL,&sa,NULL);
    load(argv[1]);
    patch_fixture(0x2119550,flush_position_fixture);patch_fixture(0x2120260,transition_fixture);
    /* Isolated ELF loader does not run the executable's COPY relocations. */
    static unsigned char multi_threaded=0;FIELD((void*)base,0x7165240,void*)=&multi_threaded;
    /* Native cache layout from AC50SongSequenceController's constructor. */
    void *cache=controller+0x38;
    FIELD(cache,0,uintptr_t)=base+0x6d8e9e8;
    FIELD(cache,8,void*)=queues+0x1d8;
    FIELD(cache,0x10,void*)=queues+8;
    FIELD(cache,0x28,void*)=controller+0xf8;
    FIELD(cache,0x30,void*)=cache_owner;
    FIELD(cache_owner,8,unsigned)=1;FIELD(cache_owner,12,unsigned)=2;
    FIELD(source_owner,8,unsigned)=1;FIELD(source_owner,12,unsigned)=2;
    FIELD(controller,0x248,void*)=controller+0x248;FIELD(controller,0x250,void*)=source_owner;
    FIELD(cache,0x88,void*)=controller+0x230;
    FIELD(controller,0x140,uintptr_t)=base+0x6d8eb68;
    /* Actual SequenceChain notifier and observer registration. */
    FIELD(song,0x10,void*)=song+0x18;FIELD(song,0x18,void*)=song+0x10;
    FN(0x1577680,void(*)(void*,void*,int))(song+8,controller+0x140,1);
    FIELD(queues,0x1d8,uint64_t)=8;FIELD(queues,0x208,void*)=slots;
    for(unsigned n=0;n<8;++n){FIELD(slots[n],0,uintptr_t)=base+0xb57a7c;FIELD(slots[n],8,uintptr_t)=base+0x15265b0;}
    FN(0xf406b0,void(*)(void*))(queues+0x220);
    uintptr_t executor_vt[3]={0,0,(uintptr_t)defer};FIELD(queues,0x30,void*)=executor_vt;
    pthread_key_t tls;assert(!pthread_key_create(&tls,NULL));FIELD(queues,0x1b0,pthread_key_t)=tls;
    int rows[][2]={{4,2},{9,3},{2,1}},old[]={4,9,2},repeated[]={4,4,9,9,9,2};
    phase="stock Repeats reproduction";steps(rows,3);rebuild();publish();expect(old,3);
    puts("REPRODUCED stock Sample ignores 2/3/1 repeats: 4,9,2");
    phase="install builder expansion";assert(song_repeats_install(base));assert(song_repeats_install(base));
    phase="expanded native playback list";rebuild();publish();expect(repeated,6);
    assert(rows[0][1]==2&&rows[1][1]==3&&rows[2][1]==1);
    puts("FIXED native audio list: 4,4,9,9,9,2,END");
    phase="real SetRepeats command Apply and Undo";
    _Alignas(8) unsigned char command[0x80]={0};
    FIELD(command,88,void*)=song;FIELD(command,96,long)=0;
    FIELD(command,104,int)=1;FIELD(command,108,int)=2;
    FN(0x19a01c0,int(*)(void*))(command);
    publish();int edited[]={4,9,9,9,2};expect(edited,5);
    FN(0x19a01f0,int(*)(void*))(command);publish();expect(repeated,6);
    phase="adjacent identical sequences";rows[1][0]=4;rebuild();publish();int adjacent[]={4,4,4,4,4,2};expect(adjacent,6);
    phase="replacement/load song";int replacement[][2]={{12,3}};steps(replacement,1);rebuild();publish();int loaded[]={12,12,12};expect(loaded,3);
    phase="allocation failure preserves audio list";replacement[0][1]=99;fail_allocation=1;rebuild();fail_allocation=0;
    assert(!task_pending);assert(reader==FIELD(queues,0x1f8,uint64_t));expect(loaded,3);
    phase="retry after OOM";rebuild();publish();int many[99];for(unsigned n=0;n<99;++n)many[n]=12;expect(many,99);
    phase="empty and null song";steps(rows,0);rebuild();publish();expect(NULL,0);
    FIELD(controller,0x220,void*)=NULL;rebuild();publish();expect(NULL,0);
    assert(FIELD(cache_owner,8,unsigned)==1&&FIELD(source_owner,8,unsigned)==1);
    assert(FIELD(cache_owner,12,unsigned)==2&&FIELD(source_owner,12,unsigned)==2);
    FN(0x15778e0,void(*)(void*,void*))(song+8,controller+0x140);
    free(FIELD(controller,0x230,void*));free(FIELD(cache,0x98,void*));
    assert(!pthread_key_delete(tls));assert(!pthread_mutex_destroy((pthread_mutex_t*)(queues+0x220)));
    puts("PASS native Repeats: native builder/SequenceChain observer/notification/deferred copy/audio swap/list lookup/end-of-sequence chooser; real command Apply/Undo, identical adjacent rows, replacement, empty, null, OOM. DSP/voice activation sinks; no app build or hardware.");
}
