#ifndef AM_MIDI_WIRE_H
#define AM_MIDI_WIRE_H
#include "midi_voice.h"
#define AM_MIDI_WIRE_BYTES 25u
/* Value-only envelope through native MidiBuffer, which copies musical bytes
 * and discards short-message padding. Decoded before the actual JUCE output;
 * it must never reach a cable, the recorder, or a saved project. */
struct am_midi_wire {uint64_t id;unsigned sound,layer;int delta;};
uint64_t am_midi_wire_id(void);
/* record is a borrowed 32-byte queue record. storage remains valid through
 * the synchronous native queue insertion, which makes its own JUCE copy. */
int am_midi_wire_wrap(void *record,unsigned char storage[AM_MIDI_WIRE_BYTES],const struct am_midi_wire*);
int am_midi_wire_read(const void *message,struct am_midi_wire*,struct am_midi_message *original);
void am_midi_wire_begin(const struct am_midi_wire*,struct am_midi_wire *previous);
void am_midi_wire_end(const struct am_midi_wire *previous);
int am_midi_wire_current(struct am_midi_wire*);
int am_midi_wire_send(struct am_midi_voices*,const struct am_midi_wire*,unsigned port,const struct am_midi_message*,am_midi_queue,void*);
#endif
