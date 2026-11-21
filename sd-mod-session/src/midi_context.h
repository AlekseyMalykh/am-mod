#ifndef AM_MIDI_CONTEXT_H
#define AM_MIDI_CONTEXT_H
#include "midi_event.h"

/* Music-owner context only. Opaque track identity is never dereferenced by a
 * transport thread. No reference to the currently selected UI Sound is used. */
struct am_midi_context {
    struct am_midi_event event;
    uintptr_t track_id;
    unsigned sound,channel;
    int valid;
};
struct am_midi_context_frame { struct am_midi_context previous; };
/* Pin: native 1.3.0 track->Program +0xbb8, Program note/pad reciprocal tables
 * +0x12a8/+0x14a8. Caller owns these native objects for the duration of entry.
 * Every begin, including rejected events, must be paired with end. */
int am_midi_context_begin(const void *track,const struct step_event*,
                          am_midi_choice_get,struct am_midi_context_frame*);
void am_midi_context_end(const struct am_midi_context_frame*);
int am_midi_context_snapshot(struct am_midi_context*);
#endif
