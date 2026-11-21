#define main unused_fx_probe
#include "check_fx_native.c"
#undef main
#include "src/midi_event.h"

int main(int argc,char **argv)
{
    assert(argc==2);struct sigaction sa={.sa_sigaction=fault,.sa_flags=SA_SIGINFO};
    sigaction(SIGSEGV,&sa,NULL);sigaction(SIGILL,&sa,NULL);sigaction(SIGABRT,&sa,NULL);
    load(argv[1]);phase="MIDI snapshot through real native event factory/modifiers";
    struct step_event (*make)(unsigned,int64_t,int64_t,unsigned,unsigned,float)=
        FN(0x27e54d0,struct step_event(*)(unsigned,int64_t,int64_t,unsigned,unsigned,float));
    void (*set)(void*,int,int,int)=FN(0x27e4400,void(*)(void*,int,int,int));
    am_midi_choice_get get=FN(0x27e4230,am_midi_choice_get);
    struct step_event event=make(50,960,240,100,0,.8f);
    struct am_midi_event decoded;unsigned note=255;
    assert(am_midi_event_copy(&event,get,AM_MIDI_EVENT_NOTE_ON,&decoded));
    assert(decoded.tick==960&&decoded.duration==240&&decoded.source_note==50);
    assert(!decoded.tune&&!decoded.slice&&decoded.velocity==102&&decoded.probability==100&&decoded.repeats==1);
    assert(am_midi_event_target(&decoded,67,&note)&&note==67);
    /* Approved Chop layout, decoded with the firmware's own packed getter.
     * The MIDI boundary must reject overflow, never wrap slice 62 to note 0. */
    for(unsigned slice=1;slice<=128;++slice){
        struct step_event chopped=make(50,960,240,100,0,.8f);
        set(chopped.data+4,15,0,(int)slice);
        assert(am_midi_event_copy(&chopped,get,AM_MIDI_EVENT_NOTE_ON,&decoded));
        assert(decoded.slice==slice&&decoded.tune==0);
        for(unsigned base=0;base<=127;++base){
            note=255;unsigned expected=base+slice-1;
            int accepted=am_midi_event_target(&decoded,base,&note);
            assert(accepted==(expected<=127));
            assert(note==(expected<=127?expected:255));
        }
    }
    set(event.data+4,0,0,-12);set(event.data+4,15,0,3);
    assert(am_midi_event_copy(&event,get,AM_MIDI_EVENT_NOTE_ON,&decoded)&&decoded.tune==-12&&decoded.slice==3);
    assert(am_midi_event_target(&decoded,67,&note)&&note==57);
    event.data[34]=17;event.data[36]=(event.data[36]&~7u)|3u;
    assert(am_midi_event_copy(&event,get,AM_MIDI_EVENT_NOTE_ON,&decoded)&&decoded.probability==17&&decoded.repeats==4);
    event.type|=0x100;assert(am_midi_event_copy(&event,get,AM_MIDI_EVENT_NOTE_ON,&decoded)&&!decoded.off);
    assert(am_midi_event_target(&decoded,67,&note)&&note==57);
    assert(am_midi_event_copy(&event,get,AM_MIDI_EVENT_NOTE_OFF,&decoded)&&decoded.off);
    note=255;assert(!am_midi_event_target(&decoded,67,&note)&&note==255);
    event.type&=~0x100u;event.type|=0x200;assert(am_midi_event_copy(&event,get,AM_MIDI_EVENT_NOTE_ON,&decoded)&&decoded.muted);
    assert(!am_midi_event_target(&decoded,67,&note));event.type&=~0x200u;
    set(event.data+4,0,0,0);set(event.data+4,15,0,2);
    assert(am_midi_event_copy(&event,get,AM_MIDI_EVENT_NOTE_ON,&decoded));
    assert(!am_midi_event_target(&decoded,127,&note));
    set(event.data+4,0,0,-120);set(event.data+4,15,0,128);
    assert(am_midi_event_copy(&event,get,AM_MIDI_EVENT_NOTE_ON,&decoded)&&decoded.tune==-120&&decoded.slice==128);
    assert(am_midi_event_target(&decoded,67,&note)&&note==74);
    struct am_midi_event before=decoded;float invalid=NAN;memcpy(event.data,&invalid,4);
    assert(!am_midi_event_copy(&event,get,AM_MIDI_EVENT_NOTE_ON,&decoded)&&!memcmp(&before,&decoded,sizeof decoded));
    event.type=4;assert(!am_midi_event_copy(&event,get,AM_MIDI_EVENT_NOTE_ON,&decoded));
    puts("PASS native MIDI event snapshot: factory, packed Tune+Slice, velocity, saved probability/ratchet, release/mute and range guards");
    return 0;
}
