#include "pad_midi_note.h"
#include <string.h>

int am_pad_midi_note_read(const struct step_event *event,am_midi_choice_get get,
                         struct am_midi_event *out,unsigned *pitch)
{
    if(!event||!out||!pitch)return 0;
    uint16_t mask;memcpy(&mask,event->data+4,sizeof mask);
    if(mask&~1u)return 0;
    struct am_midi_event decoded;
    if(!am_midi_event_copy(event,get,AM_MIDI_EVENT_NOTE_ON,&decoded)
       ||decoded.tick<0||decoded.duration<0||!decoded.velocity||decoded.muted)return 0;
    unsigned note;
    if(!am_midi_event_target(&decoded,AM_PAD_MIDI_BASE_NOTE,&note))return 0;
    *out=decoded;*pitch=note;return 1;
}

int am_pad_midi_note_make(unsigned source_note,unsigned pitch,unsigned velocity,
                         unsigned channel,am_pad_note_factory make,
                         am_pad_note_choice_set set,am_midi_choice_get get,
                         struct step_event *out)
{
    if(source_note>127||pitch>127||!velocity||velocity>127||channel>15
       ||!make||!set||!get||!out)return 0;
    struct step_event event=make(source_note,0,0,100,channel,velocity/127.f);
    set(event.data+4,0,0,(int)pitch-(int)AM_PAD_MIDI_BASE_NOTE);
    struct am_midi_event decoded;unsigned verified;
    if(!am_pad_midi_note_read(&event,get,&decoded,&verified)
       ||verified!=pitch||decoded.source_note!=source_note||decoded.velocity!=velocity
       ||decoded.tick||decoded.duration||((event.type>>12)&15u)!=channel)return 0;
    *out=event;return 1;
}
