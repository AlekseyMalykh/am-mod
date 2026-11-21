#define main unused_fx_probe
#include "check_fx_native.c"
#undef main
#include "src/midi_event.h"

static unsigned char emitted[32];static int emitted_route,calls;static void *emitted_manager;
static unsigned released_count;static unsigned char released[3][96];
static void capture(void *manager,int route,const void *message)
{memcpy(emitted,message,32);emitted_route=route;emitted_manager=manager;++calls;}
static void release_capture(void *engine,uint64_t time,void *track,unsigned note,
                            unsigned tag,unsigned channel,unsigned kind,unsigned scheduled)
{
    (void)time;(void)tag;(void)kind;assert(scheduled==1&&released_count<3);
    const unsigned char *first;memcpy(&first,(char*)engine+0x6c8,sizeof first);
    assert(first&&first[64]==note);unsigned saved_channel;uintptr_t saved_track;
    memcpy(&saved_channel,first+76,4);memcpy(&saved_track,first+80,8);
    assert(saved_channel==channel&&saved_track==(uintptr_t)track);
    memcpy(released[released_count++],first,96);
}
static void replace(unsigned offset,uintptr_t fn)
{
    uint32_t jump[]={0x58000050,0xd61f0200};
    memcpy((void*)(base+offset),jump,8);memcpy((void*)(base+offset+8),&fn,8);
    __builtin___clear_cache((char*)base+offset,(char*)base+offset+16);
}
int main(int argc,char **argv)
{
    assert(argc==2);struct sigaction sa={.sa_sigaction=fault,.sa_flags=SA_SIGINFO};
    sigaction(SIGSEGV,&sa,NULL);sigaction(SIGILL,&sa,NULL);sigaction(SIGABRT,&sa,NULL);
    load(argv[1]);phase="native MIDI backend to queue boundary";
    /* Only the final queue insertion is replaced. Message construction, timing,
     * channel encoding, velocity conversion and JUCE copying execute in ELF. */
    replace(0x1e2ac50,(uintptr_t)capture);
    uintptr_t vtable[4]={0,0,0,base+0x28a8020};
    unsigned char engine[96]={0},clock[32]={0};float rate=48000;uint64_t origin=1234000;
    memcpy(engine+80,&rate,4);memcpy(clock+16,&origin,8);
    void *manager=(void*)0x12345000;
    uintptr_t backend[4]={(uintptr_t)vtable,(uintptr_t)engine,(uintptr_t)clock,(uintptr_t)manager};
    void (*on)(void*,uint64_t,int,const void*)=FN(0x28a8d00,void(*)(void*,uint64_t,int,const void*));
    void (*off)(void*,uint64_t,int,unsigned,unsigned)=FN(0x28a8eb0,void(*)(void*,uint64_t,int,unsigned,unsigned));
    void (*pressure)(void*,uint64_t,int,unsigned,unsigned,float)=FN(0x28a8410,void(*)(void*,uint64_t,int,unsigned,unsigned,float));
    struct step_event (*make)(unsigned,int64_t,int64_t,unsigned,unsigned,float)=FN(0x27e54d0,struct step_event(*)(unsigned,int64_t,int64_t,unsigned,unsigned,float));
    void (*set)(void*,int,int,int)=FN(0x27e4400,void(*)(void*,int,int,int));
    am_midi_choice_get get=FN(0x27e4230,am_midi_choice_get);
    struct step_event event=make(50,960,240,17,0,.8f);
    set(event.data+4,0,0,-12);set(event.data+4,15,0,3);
    unsigned char command[44]={0};command[0]=50;
    unsigned channel=2;memcpy(command+4,&channel,4);memcpy(command+8,event.data,4);
    memcpy(command+12,event.data+4,18);
    struct am_midi_event source,accepted;unsigned target;
    assert(am_midi_event_copy(&event,get,AM_MIDI_EVENT_NOTE_ON,&source));
    assert(am_midi_command_copy(command,&source,get,&accepted));
    assert(accepted.tune==-12&&accepted.slice==3&&accepted.probability==17&&accepted.tick==960&&accepted.duration==240);
    assert(am_midi_event_target(&accepted,67,&target)&&target==57);
    on(backend,96,321,command);
    assert(calls==1&&emitted_manager==manager&&emitted_route==321);
    assert(emitted[0]==0x92&&emitted[1]==50&&emitted[2]==101);
    double timestamp;unsigned milliseconds;int offset,size;
    memcpy(&timestamp,emitted+8,8);memcpy(&size,emitted+16,4);
    memcpy(&milliseconds,emitted+24,4);memcpy(&offset,emitted+28,4);
    assert(size==3&&offset==96&&timestamp==0.0&&milliseconds==1234002);
    /* Stock ignores the packed Tune and Slice. Substituting only the accepted
     * command's key preserves native route, timing and audio event bytes. */
    struct step_event before=event;command[0]=(unsigned char)target;
    on(backend,96,321,command);
    assert(calls==2&&emitted[0]==0x92&&emitted[1]==57&&emitted_route==321);
    assert(!memcmp(&before,&event,sizeof event));
    off(backend,128,321,2,target);
    assert(calls==3&&emitted[0]==0x82&&emitted[1]==57&&emitted[2]==0);
    memcpy(&offset,emitted+28,4);assert(offset==128);
    pressure(backend,144,321,2,target,.5f);
    assert(calls==4&&emitted[0]==0xa2&&emitted[1]==57&&emitted[2]==64&&emitted_route==321);
    memcpy(&offset,emitted+28,4);assert(offset==144);
    struct am_midi_event previous=accepted;channel=16;memcpy(command+4,&channel,4);
    assert(!am_midi_command_copy(command,&source,get,&accepted)&&!memcmp(&previous,&accepted,sizeof accepted));
    phase="real native scheduled release heap";
    replace(0x2118460,(uintptr_t)release_capture);
    unsigned char scheduler[0x800]={0};
    void (*push)(void*,const void*)=FN(0x212bc10,void(*)(void*,const void*));
    void (*due)(void*,uint64_t,uint64_t)=FN(0x21185a0,void(*)(void*,uint64_t,uint64_t));
    const uint64_t ends[3]={960,720,840};
    for(unsigned i=0;i<3;++i){
        unsigned char entry[96]={0};
        struct step_event queued=make(50,480,240,100,0,.8f);
        set(queued.data+4,0,0,(int)i);set(queued.data+4,15,0,(int)i+1);
        memcpy(entry,&queued,sizeof queued);memcpy(entry+56,ends+i,8);
        entry[64]=50;unsigned input_kind=3,saved_channel=2;
        memcpy(entry+72,&input_kind,4);memcpy(entry+76,&saved_channel,4);
        uintptr_t track=0x12345000;memcpy(entry+80,&track,8);entry[88]=1;
        push(scheduler+0x6c8,entry);
    }
    due(scheduler,719,0);assert(released_count==0);
    /* This native entry is called at the exact next scheduled tick. It is not
     * a catch-up polling API for arbitrary wall-clock deadlines. */
    due(scheduler,720,96);assert(released_count==1);
    due(scheduler,840,96);assert(released_count==2);
    due(scheduler,960,96);assert(released_count==3);
    const unsigned order[3]={1,2,0};
    for(unsigned i=0;i<3;++i){
        struct step_event queued;memcpy(&queued,released[i],sizeof queued);
        assert(am_midi_event_copy(&queued,get,AM_MIDI_EVENT_NOTE_ON,&accepted));
        assert(accepted.tune==(int)order[i]&&accepted.slice==order[i]+1&&accepted.tick==480&&accepted.duration==240);
        uint64_t end;memcpy(&end,released[i]+56,8);assert(end==ends[order[i]]);
    }
    void *storage;memcpy(&storage,scheduler+0x6c8,sizeof storage);free(storage);
    puts("PASS real native MIDI backend: accepted Tune/Slice snapshot, Note On/Off, Pressure, channel, route, velocity and timestamp preserved at queue insertion");
    puts("PASS real native release heap: reordered overlapping events retain Tune/Slice, timing, track, note and channel at scheduled Note Off");
    return 0;
}
