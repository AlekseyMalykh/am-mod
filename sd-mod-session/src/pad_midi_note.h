#ifndef AM_PAD_MIDI_NOTE_H
#define AM_PAD_MIDI_NOTE_H
#include "midi_event.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Prototype value adapter; no hooks, project mutation, clock or audio I/O.
 * The caller explicitly owns the instrument binding. This codec must never
 * infer a Synth destination from channel, current screen or an empty pad.
 * Musical pitch is encoded as native Tune relative to MIDI note 60. */
#define AM_PAD_MIDI_BASE_NOTE 60u
typedef struct step_event (*am_pad_note_factory)(unsigned,int64_t,int64_t,
                                                unsigned,unsigned,float);
typedef void (*am_pad_note_choice_set)(void*,int,int,int);

/* Build a native input value for the existing batch_piano_input worker.
 * source_note is Program's native note, NOT a hardware pad ordinal.
 * velocity is 1..127, channel 0..15. Failure preserves *out.
 * Successful enqueue, recording and destination ownership remain separate. */
int am_pad_midi_note_make(unsigned source_note,unsigned pitch,unsigned velocity,
                         unsigned channel,am_pad_note_factory,
                         am_pad_note_choice_set,am_midi_choice_get,
                         struct step_event *out);

/* For an explicitly bound carrier only. Reject Slice/other modifiers rather
 * than silently converting a Chop/sample event into a Synth note.
 * Returns a value snapshot; no native pointer is retained. */
int am_pad_midi_note_read(const struct step_event*,am_midi_choice_get,
                         struct am_midi_event *out,unsigned *pitch);
#ifdef __cplusplus
}
#endif
#endif
