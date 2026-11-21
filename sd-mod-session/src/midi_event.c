#include "midi_event.h"
#include <math.h>
#include <string.h>

int am_midi_event_copy(const struct step_event *source,am_midi_choice_get get,unsigned operation,
                       struct am_midi_event *out)
{
    if(!source||!out||(source->type&255)!=3||operation>AM_MIDI_EVENT_NOTE_OFF)return 0;
    struct step_event value=*source;
    float velocity;uint16_t mask;int64_t duration;
    memcpy(&velocity,value.data,sizeof velocity);
    memcpy(&mask,value.data+4,sizeof mask);
    memcpy(&duration,value.data+24,sizeof duration);
    if(!isfinite(velocity)||velocity<0.f||velocity>1.f||value.data[32]>127
        ||value.data[34]>100||((mask&0x8001)&&!get))return 0;
    int tune=(mask&1)?get(value.data+4,0,0):0;
    int slice=(mask&0x8000)?get(value.data+4,15,0):0;
    if(tune< -120||tune>120||slice<0||slice>128
        ||((mask&0x8000)&&!slice))return 0;
    struct am_midi_event result={
        .tick=value.time,.duration=duration,.tune=(int16_t)tune,
        .slice=(uint16_t)slice,.source_note=value.data[32],
        .velocity=(uint8_t)(velocity*127.f+.5f),
        .off=(uint8_t)(operation==AM_MIDI_EVENT_NOTE_OFF),
        .muted=(uint8_t)!!(value.type&0x200),
        .probability=value.data[34],.repeats=(uint8_t)((value.data[36]&7)+1)
    };
    *out=result;return 1;
}

int am_midi_event_target(const struct am_midi_event *event,unsigned base,unsigned *note)
{
    if(!event||!note||base>127||event->off||event->muted
        ||event->tune< -120||event->tune>120||event->slice>128)return 0;
    int target=(int)base+event->tune+(event->slice?(int)event->slice-1:0);
    if(target<0||target>127)return 0;
    *note=(unsigned)target;return 1;
}

int am_midi_command_copy(const void *command,const struct am_midi_event *source,
                         am_midi_choice_get get,struct am_midi_event *out)
{
    if(!command||!source||!out)return 0;
    unsigned char value[44];memcpy(value,command,sizeof value);
    unsigned channel;memcpy(&channel,value+4,sizeof channel);
    if(channel>15||value[0]>127)return 0;
    /* Reuse the pinned event decoder instead of duplicating packed choices. */
    struct step_event event={0};event.type=3;
    memcpy(event.data,value+8,4);memcpy(event.data+4,value+12,18);
    event.data[32]=value[0];event.data[34]=100;
    struct am_midi_event result;
    if(!am_midi_event_copy(&event,get,AM_MIDI_EVENT_NOTE_ON,&result))return 0;
    result.tick=source->tick;result.duration=source->duration;
    result.source_note=source->source_note;result.probability=source->probability;
    result.repeats=source->repeats;result.muted=source->muted;
    *out=result;return 1;
}
