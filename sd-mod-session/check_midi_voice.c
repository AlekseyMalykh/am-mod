#include "src/midi_voice.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static struct am_midi_packet sent[1024];static unsigned count,blocked;
static int queue(void *p,const struct am_midi_packet *packet)
{(void)p;if(blocked)return 0;assert(count<1024);sent[count++]=*packet;return 1;}
int main(void)
{
    struct am_midi_voices state={0};struct am_midi_event event={.velocity=100,.probability=100,.repeats=1};
    assert(am_midi_voice_on(&state,1,10,1,3,67,&event,queue,NULL)==1);
    event.tune=4;assert(am_midi_voice_on(&state,2,10,1,3,67,&event,queue,NULL)==1);
    assert(count==2&&sent[0].message.data[0]==0x92&&sent[0].message.data[1]==67&&sent[1].message.data[1]==71);
    assert(am_midi_voice_off(&state,1,queue,NULL)==1&&sent[2].message.data[1]==67);
    assert(am_midi_voice_pressure(&state,2,80,queue,NULL)==1&&sent[3].message.data[0]==0xa2&&sent[3].message.data[1]==71);
    assert(am_midi_voice_off(&state,2,queue,NULL)==1&&sent[4].message.data[1]==71);
    assert(am_midi_voice_off(&state,2,queue,NULL)==0);
    /* Shared note: one wire On, one final Off, any source release order. */
    event.tune=0;count=0;
    assert(am_midi_voice_on(&state,3,10,1,3,67,&event,queue,NULL)==1);
    assert(am_midi_voice_on(&state,4,20,1,3,67,&event,queue,NULL)==1&&count==1);
    assert(am_midi_voices_release_scope(&state,10,queue,NULL)==1&&count==1);
    assert(am_midi_voice_pressure(&state,4,65,queue,NULL)==1);
    assert(am_midi_voice_off(&state,4,queue,NULL)==1&&count==3);
    /* Queue refusal: releases remain owned and retry their original target. */
    assert(am_midi_voice_on(&state,5,10,2,16,60,&event,queue,NULL)==1);
    blocked=1;assert(am_midi_voice_off(&state,5,queue,NULL)==-1);
    assert(am_midi_voice_pressure(&state,5,80,queue,NULL)==0);
    assert(am_midi_voice_on(&state,500,10,2,16,60,&event,queue,NULL)==-1);
    blocked=0;assert(am_midi_voices_retry(&state,queue,NULL)==1);
    assert(sent[count-1].port==2&&sent[count-1].message.data[0]==0x8f&&sent[count-1].message.data[1]==60);
    blocked=1;assert(am_midi_voice_on(&state,6,10,1,3,67,&event,queue,NULL)==-1);
    blocked=0;assert(am_midi_voice_off(&state,6,queue,NULL)==0);
    count=0;for(unsigned i=0;i<AM_MIDI_VOICES;++i)assert(am_midi_voice_on(&state,100+i,10,1,3,67,&event,queue,NULL)==1);
    assert(count==1&&am_midi_voice_on(&state,10000,10,1,3,67,&event,queue,NULL)==-1);
    blocked=1;assert(am_midi_voices_panic(&state,queue,NULL)==-1);
    assert(am_midi_voice_on(&state,10000,10,1,3,68,&event,queue,NULL)==-1);
    blocked=0;assert(am_midi_voices_retry(&state,queue,NULL)==1);
    assert(count==4&&sent[1].message.data[0]==0x82&&sent[2].message.data[1]==64&&sent[3].message.data[1]==123);
    for(unsigned i=0;i<AM_MIDI_VOICES;++i)assert(!state.voices[i].id);
    /* A15 base 67/channel 3: three Chop slices may overlap. A new route for
     * subsequent notes must not redirect pressure or Off of an existing slice. */
    count=0;event.tune=0;
    for(unsigned slice=1;slice<=3;++slice){
        event.slice=(uint16_t)slice;
        assert(am_midi_voice_on(&state,2000+slice,15,1,3,67,&event,queue,NULL)==1);
        assert(sent[slice-1].message.data[1]==66+slice);
    }
    event.slice=1;
    assert(am_midi_voice_on(&state,3000,16,2,8,40,&event,queue,NULL)==1);
    assert(am_midi_voice_pressure(&state,2002,70,queue,NULL)==1);
    assert(sent[count-1].port==1&&sent[count-1].message.data[0]==0xa2&&sent[count-1].message.data[1]==68);
    assert(am_midi_voice_off(&state,2002,queue,NULL)==1);
    assert(sent[count-1].message.data[0]==0x82&&sent[count-1].message.data[1]==68);
    unsigned before_release=count;
    assert(am_midi_voices_release_scope(&state,15,queue,NULL)==1&&count==before_release+2);
    assert(sent[before_release].message.data[1]==67&&sent[before_release+1].message.data[1]==69);
    assert(am_midi_voice_off(&state,3000,queue,NULL)==1);
    assert(sent[count-1].port==2&&sent[count-1].message.data[0]==0x87&&sent[count-1].message.data[1]==40);
    puts("PASS MIDI occurrence voices: independent pitches/releases, shared gates, captured pressure, scoped release, capacity, queue retry and Panic");
    return 0;
}
