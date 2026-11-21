#ifndef AM_MIDI_VOICE_H
#define AM_MIDI_VOICE_H
#include "midi_event.h"
#include "midi_tools.h"
#include "midi_route.h"

#define AM_MIDI_VOICES 256u
struct am_midi_packet { unsigned port;struct am_midi_message message; };
/* Queue acceptance only. The music owner must not perform device I/O here. */
typedef int (*am_midi_queue)(void*,const struct am_midi_packet*);
struct am_midi_voice {
    uint64_t id;
    uint32_t scope;
    uint8_t port,channel,note,released,kind;
};
struct am_midi_voices {
    struct am_midi_voice voices[AM_MIDI_VOICES];
    uint16_t gates[2][16][128];
    uint16_t cc_gates[2][16][128];
    uint32_t sustain_pending,panic_pending;
};
/* Single music owner. IDs identify occurrences, not just source note numbers.
 * A scope identifies a live input owner, allowing its release without stopping
 * sequence voices. Channel is 1..16; port is USB or External.
 * Return 1 accepted, 0 ignored, -1 invalid/capacity/backpressure.
 * Only adapters already owning the musical route call these entrypoints. */
int am_midi_voice_on(struct am_midi_voices*,uint64_t id,uint32_t scope,
                     unsigned port,unsigned channel,unsigned base,
                     const struct am_midi_event*,am_midi_queue,void*);
int am_midi_voice_target_on(struct am_midi_voices*,uint64_t id,uint32_t scope,
                           const struct am_midi_target*,am_midi_queue,void*);
int am_midi_voice_off(struct am_midi_voices*,uint64_t id,am_midi_queue,void*);
int am_midi_voice_pressure(struct am_midi_voices*,uint64_t id,unsigned value,
                           am_midi_queue,void*);
/* Failed releases stay owned and can be retried without losing destinations. */
int am_midi_voices_retry(struct am_midi_voices*,am_midi_queue,void*);
int am_midi_voices_release_scope(struct am_midi_voices*,uint32_t,am_midi_queue,void*);
int am_midi_voices_panic(struct am_midi_voices*,am_midi_queue,void*);
#endif
