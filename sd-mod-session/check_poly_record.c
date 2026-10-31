#define main old_fx_main
#include "check_fx_native.c"
#undef main
#include "src/step_native.h"
#include "src/poly_record.c"
struct tree {uint64_t pad,colour;void *parent,*left,*right;uint64_t size;};
static struct step_event committed[4];
static unsigned commit_count;
static struct step_event played[4];static unsigned play_count,release_count,fallback_count;
static unsigned released_choice[4];
static unsigned char fake_engine[0x800];static unsigned queued[4],queued_count;
static void capture_play(void *engine,int source,void *event,int flags){
    assert(engine==fake_engine&&source==0&&flags==0x7ffffffa&&play_count<4);
    played[play_count++]=*(struct step_event*)event;
}
static void capture_release(void *engine,int source,int note,int flags,int extra){
    assert(engine==fake_engine&&source==1&&note==0&&flags==0x7ffffffa&&!extra&&releasing);
    assert(release_count<4);released_choice[release_count++]=release_choice;
}
static int track_available(void *p){assert(p==(void*)0x4560);return 0;}
static int recording_enabled(void *p){assert(p==(void*)0x7890);return 1;}
static void *clip_recorder(void *service,void *track){assert(service==(void*)0x7891&&track==(void*)0x4560);return (void*)0x2340;}
static int recording_destination(void *p){assert(p==(void*)0x2340);return 7;}
static int midi_note(void *engine,int pad,int flags,void *track){assert(engine==fake_engine&&!pad&&flags==0x7ffffffa&&track==(void*)0x4560);return 36;}
static void capture_off_queue(void *recorder,int source,int destination,unsigned note){
    assert(recorder==(void*)0x2340&&source==1&&destination==7&&queued_count<4);
    queued[queued_count++]=note;
}
static void capture_fallback(void *mode,int pad,float velocity){(void)mode;(void)pad;(void)velocity;++fallback_count;}
static void redirect(uintptr_t offset,void *function){
    assert(!mprotect((void*)((base+offset)&~(uintptr_t)4095),4096,PROT_READ|PROT_WRITE|PROT_EXEC));
    uint32_t code[]={0x58000050,0xd61f0200};memcpy((void*)(base+offset),code,8);
    F((void*)base,offset+8,uintptr_t)=(uintptr_t)function;
    __builtin___clear_cache((char*)base+offset,(char*)base+offset+16);
}
static void capture_commit(void *recorder,void *program,int destination,void *event,
                           unsigned flag,uintptr_t a,uintptr_t b,uintptr_t c,unsigned d,int flags){
    (void)recorder;(void)program;(void)a;(void)b;(void)c;(void)d;(void)flags;
    assert(destination==7&&flag==1&&commit_count<4);
    committed[commit_count++]=*(struct step_event*)event;
}
static void pending(void *recorder,void *program,const struct step_event *event){
    void *node=FN(0xb51418,void*(*)(size_t))(0x68);memset(node,0,0x68);
    FN(0x1971e60,void(*)(void*,const void*))((char*)node+0x10,event);
    F(node,0x48,void*)=program;F(node,0x50,int)=7;
    F(node,0x58,int64_t)=F(node,0x60,int64_t)=100;
    FN(0xb536f8,void(*)(void*,void*))(node,(char*)recorder+0x1a0);
    ++F(recorder,0x1b0,uint64_t);
}
static void init_batch(unsigned char batch[0x60]){
    memset(batch,0,0x60);F(batch,0x28,void*)=F(batch,0x30,void*)=batch+0x18;
}
static uint64_t batch_size(unsigned char batch[0x60]){
    void *node=F(batch,0x28,void*);assert(node!=batch+0x18);
    return F(node,0x50,uint64_t); /* per-clip added-event multiset */
}
#ifndef MPC_POLY_TEST_MAIN
#define MPC_POLY_TEST_MAIN main
#endif
int MPC_POLY_TEST_MAIN(int argc,char **argv){
    assert(argc==2);load(argv[1]);
    struct sigaction sa={.sa_sigaction=fault,.sa_flags=SA_SIGINFO};
    sigaction(SIGSEGV,&sa,NULL);sigaction(SIGABRT,&sa,NULL);
    struct tree tree={0};tree.left=tree.right=&tree.colour;
    struct step_event a=FN(0x27e54d0,struct step_event(*)(unsigned,int64_t,int64_t,unsigned,unsigned,float))(36,480,240,100,0,.8f);
    struct step_event b=a;
    FN(0x27e4400,void(*)(void*,int,int,int))((char*)&a+0x14,0,0,-3);
    FN(0x27e4400,void(*)(void*,int,int,int))((char*)&b+0x14,0,0,4);
    FN(0x1574460,void(*)(void*,const void*))(&tree,&a);
    void *end=&tree.colour;
    void *old=FN(0x27cf8d0,void*(*)(void*,const void*))(&tree,&b);
    assert(old!=end); /* Negative control: a different pitch is overwritten. */
    unsigned char old_batch[0x60];init_batch(old_batch);
    FN(0x2137ed0,void(*)(void*,void*,const void*))(old_batch,&tree,&a);
    FN(0x2137ed0,void(*)(void*,void*,const void*))(old_batch,&tree,&b);
    assert(batch_size(old_batch)==1); /* Undo aggregation independently loses a chord. */
    assert(poly_record_install(base));
    unsigned char new_batch[0x60];init_batch(new_batch);
    FN(0x2137ed0,void(*)(void*,void*,const void*))(new_batch,&tree,&a);
    FN(0x2137ed0,void(*)(void*,void*,const void*))(new_batch,&tree,&b);
    assert(batch_size(new_batch)==2);
    uint32_t branch=F((void*)base,0x213aadc,uint32_t);
    uintptr_t relay=base+0x213aadc+((int32_t)(branch<<6)>>4);
    void *(*lookup)(void*,const void*)=(void*)relay;
    assert(lookup(&tree,&b)==end && lookup(&tree,&a)==old);
    FN(0x1574460,void(*)(void*,const void*))(&tree,&b);
    assert(tree.size==2 && lookup(&tree,&b)!=old && lookup(&tree,&b)!=end);
    struct step_event c=b;
    F(&c,0x10,float)=.3f;F(&c,0x28,int64_t)=960;
    assert(lookup(&tree,&c)==lookup(&tree,&b)); /* Same pitch: replace velocity/duration. */
    c.time++;assert(lookup(&tree,&c)==end);
    c=b;F(&c,0x30,unsigned char)=37;assert(lookup(&tree,&c)==end);
    assert(poly_record_install(base));
    /* Run the real native pending-note scan, duration calculation, unlink and
     * deallocation. Only final project commit is captured in this fixture. */
    unsigned char recorder[0x1c0]={0};void *header=recorder+0x1a0;
    F(header,0,void*)=F(header,8,void*)=header;void *program=&tree;
    F(&a,0x28,int64_t)=F(&b,0x28,int64_t)=0;
    pending(recorder,program,&a);pending(recorder,program,&b);
    uint32_t jump[]={0x58000050,0xd61f0200};
    assert(!mprotect((void*)(base+0x213a000),4096,PROT_READ|PROT_WRITE|PROT_EXEC));
    memcpy((void*)(base+0x213a940),jump,8);F((void*)base,0x213a948,uintptr_t)=(uintptr_t)capture_commit;
    __builtin___clear_cache((char*)base+0x213a940,(char*)base+0x213a950);
    unsigned high=F(&b,0x16,unsigned char),low=F(&a,0x16,unsigned char);
    finish_note(recorder,program,7,CHOICE_MARK|(high<<8)|36,0,500,0,0,0);
    assert(commit_count==1&&same_choice(&committed[0],&b)&&F(&committed[0],0x28,int64_t)==400);
    assert(F(recorder,0x1b0,uint64_t)==1);
    finish_note(recorder,program,7,CHOICE_MARK|(high<<8)|36,0,550,0,0,0);
    assert(commit_count==1); /* Duplicate/stale release cannot consume low. */
    finish_note(recorder,program,7,CHOICE_MARK|(low<<8)|36,0,800,0,0,0);
    assert(commit_count==2&&same_choice(&committed[1],&a)&&F(&committed[1],0x28,int64_t)==700);
    assert(F(header,0,void*)==header&&F(header,8,void*)==header&&!F(recorder,0x1b0,uint64_t));
    /* Exercise the actual replacement pad callback and native 16-Levels
     * modifier builder. Capture only the final Engine calls. */
    redirect(0x211b090,capture_play);redirect(0x2118930,capture_release);redirect(0x29c3f80,track_available);
    redirect(0x2133800,recording_enabled);redirect(0x20f8da0,clip_recorder);redirect(0x20e2290,recording_destination);
    redirect(0x2114fc0,midi_note);redirect(0x20e4da0,capture_off_queue);
    unsigned char mode[0x80]={0},props[0x340]={0},gate[0x60]={0},things[0x150]={0},input[0xe0]={0};
    F(mode,0x48,void*)=gate;F(mode,0x50,void*)=props;F(mode,0x58,void*)=things;F(mode,0x68,void*)=input;
    F(things,0x140,void*)=(void*)0x4560;F(input,0xd8,void*)=fake_engine;
    F(fake_engine,0x768,void*)=(void*)0x7890;F(fake_engine,0x778,void*)=(void*)0x7891;
    F(props,0x88,int)=1;F(props,0x228,int)=3;atomic_store(&context,0x10001);
    input_pad(mode,0,.8f);input_pad(mode,7,.6f);
    assert(play_count==2&&same_choice(&played[0],&a)&&same_choice(&played[1],&b));
    assert(F(&played[0],0x10,float)==.8f&&F(&played[1],0x10,float)==.6f);
    input_pad(mode,7,0);input_pad(mode,0,0);input_pad(mode,0,0);
    assert(queued_count==2&&queued[0]==(CHOICE_MARK|(high<<8)|36)&&queued[1]==(CHOICE_MARK|(low<<8)|36));
    assert(release_count==1&&released_choice[0]==low); /* Shared source closes once, on last physical release. */
    uintptr_t saved_original=input_original;input_original=(uintptr_t)capture_fallback;
    atomic_store(&context,0x20000);input_pad(mode,3,.7f);input_original=saved_original;
    assert(fallback_count==1&&play_count==2); /* Mono retains its native route. */
    puts("PASS native pitch overwrite and independent reverse-order NoteOff durations; duplicate release ignored; native pending list unlink/free executed. Live recording requires VM test.");
    return 0;
}
