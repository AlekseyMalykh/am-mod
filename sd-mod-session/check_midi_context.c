#include "src/midi_context.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
static unsigned char track[0xbc0],program[0x16a8];
static void bind(unsigned pad,unsigned note)
{int p=(int)pad,n=(int)note;memcpy(program+0x12a8+pad*4,&n,4);memcpy(program+0x14a8+note*4,&p,4);}
static struct step_event make(unsigned note)
{struct step_event e={0};e.type=3|(2u<<12);e.time=960;float v=.8f;memcpy(e.data,&v,4);e.data[32]=(unsigned char)note;e.data[34]=100;return e;}
static void *other_thread(void *unused)
{(void)unused;struct am_midi_context v;assert(!am_midi_context_snapshot(&v));return NULL;}
int main(void)
{
    const void *p=program;memcpy(track+0xbb8,&p,sizeof p);bind(14,50);bind(127,127);
    struct step_event a=make(50),b=make(127);struct am_midi_context result;
    struct am_midi_context_frame outer,inner,rejected;
    assert(!am_midi_context_snapshot(&result));
    assert(am_midi_context_begin(track,&a,NULL,&outer));
    assert(am_midi_context_snapshot(&result)&&result.sound==14&&result.channel==2&&result.event.source_note==50&&result.track_id==(uintptr_t)track);
    pthread_t thread;assert(!pthread_create(&thread,NULL,other_thread,NULL));assert(!pthread_join(thread,NULL));
    assert(am_midi_context_begin(track,&b,NULL,&inner));
    assert(am_midi_context_snapshot(&result)&&result.sound==127);
    struct step_event controller={0};controller.type=1;
    assert(!am_midi_context_begin(track,&controller,NULL,&rejected));
    assert(!am_midi_context_snapshot(&result));
    am_midi_context_end(&rejected);assert(am_midi_context_snapshot(&result)&&result.sound==127);
    am_midi_context_end(&inner);assert(am_midi_context_snapshot(&result)&&result.sound==14);
    am_midi_context_end(&outer);assert(!am_midi_context_snapshot(&result));
    int invalid=128;memcpy(program+0x14a8+50*4,&invalid,4);
    assert(!am_midi_context_begin(track,&a,NULL,&outer));am_midi_context_end(&outer);
    bind(14,50);invalid=49;memcpy(program+0x12a8+14*4,&invalid,4);
    assert(!am_midi_context_begin(track,&a,NULL,&outer));am_midi_context_end(&outer);
    assert(!am_midi_context_snapshot(&result));
    puts("PASS MIDI Sound context: reciprocal pad lookup, source channel, nested restoration, rejected-event isolation and thread isolation");return 0;
}
