#include "midi_context.h"
#include <string.h>

static _Thread_local struct am_midi_context current;
int am_midi_context_begin(const void *track,const struct step_event *event,
                          am_midi_choice_get get,struct am_midi_context_frame *frame)
{
    if(!frame)return 0;
    frame->previous=current;memset(&current,0,sizeof current);
    struct am_midi_event copied;
    if(!track||!am_midi_event_copy(event,get,AM_MIDI_EVENT_NOTE_ON,&copied))return 0;
    const unsigned char *program;
    memcpy(&program,(const unsigned char*)track+0xbb8,sizeof program);
    if(!program)return 0;
    int sound,note;
    memcpy(&sound,program+0x14a8+copied.source_note*4,sizeof sound);
    if(sound<0||sound>=128)return 0;
    memcpy(&note,program+0x12a8+(unsigned)sound*4,sizeof note);
    if(note!=(int)copied.source_note)return 0;
    current.event=copied;current.track_id=(uintptr_t)track;
    current.sound=(unsigned)sound;current.channel=(event->type>>12)&15;
    current.valid=1;return 1;
}
void am_midi_context_end(const struct am_midi_context_frame *frame)
{if(frame)current=frame->previous;}
int am_midi_context_snapshot(struct am_midi_context *out)
{if(!out||!current.valid)return 0;*out=current;return 1;}
