#ifndef AM_MIDI_EVENT_H
#define AM_MIDI_EVENT_H
#include <stdint.h>
#include "step_native.h"

/* A value snapshot of persisted musical data. No native owner crosses threads.
 * Probability and repeat count describe the source; scheduling must consume
 * native decisions, never roll probability or expand ratchets a second time. */
struct am_midi_event {
    int64_t tick, duration;
    int16_t tune;
    uint16_t slice; /* 0: absent; native slice 1..128 */
    uint8_t source_note, velocity, off, muted, probability, repeats;
};
typedef int (*am_midi_choice_get)(const void*,int,int);
enum { AM_MIDI_EVENT_NOTE_ON, AM_MIDI_EVENT_NOTE_OFF };
/* Only inline native tag-3 events are accepted. Failure preserves *out. */
/* The operation comes from the emitting boundary. In stored events bit 8 is
 * also used by selection, so it cannot classify a MIDI release by itself. */
int am_midi_event_copy(const struct step_event*,am_midi_choice_get,unsigned operation,
                       struct am_midi_event *out);
/* The accepted 44-byte Note command at the native MIDI backend. Its choices
 * have already passed native scheduling/filtering. No probability is rerolled.
 * source supplies persisted time/metadata; final velocity and choices come
 * from command. Neither pointer is retained. */
int am_midi_command_copy(const void *command,const struct am_midi_event *source,
                         am_midi_choice_get,struct am_midi_event *out);
/* Note-on target only. Releases must use their captured voice destination. */
int am_midi_event_target(const struct am_midi_event*,unsigned base,unsigned *note);
#endif
