#ifndef AM_SYNTH_SEQUENCE_H
#define AM_SYNTH_SEQUENCE_H
#include "synth.h"
#include "midi_event.h"
#ifdef __cplusplus
extern "C" {
#endif
#ifdef MPC_PAD_MIDI_RECORDING_TRIAL
enum am_synth_sequence_result {AM_SYNTH_SEQUENCE_INVALID=-2,AM_SYNTH_SEQUENCE_BUSY=-1,
    AM_SYNTH_SEQUENCE_IGNORED=0,AM_SYNTH_SEQUENCE_ACCEPTED=1};
/* Embed one token in the existing native/MIDI live_note. No second event pool. */
struct am_synth_sequence_voice {uint64_t epoch,transport,occurrence;struct am_synth_handle handle;unsigned releasing;};
/* One music-executor owner for the current single Synth engine. Zero-init;
 * never copy live state, serialize it, or call it from UI/audio concurrently.
 * This is an event sink, not a clock: native accepted/scheduled On and exact
 * scheduled Off call these functions. No duration timer, probability reroll,
 * ratchet expansion, selected-pad lookup or instrument inference occurs here.
 * A future adapter must first resolve an explicit validated Synth binding. */
struct am_synth_sequence {uint64_t epoch,transport,last_occurrence;};
/* New binding/project requires a monotonically increasing source epoch.
 * Native/runtime identities belong to the adapter, not to this value sink. */
int am_synth_sequence_configure(struct am_synth_sequence*,uint64_t epoch);
/* Capture epoch AND transport when accepting the native occurrence; do not
 * substitute current values when a delayed callback is delivered. */
int am_synth_sequence_on(struct am_synth_sequence*,struct am_synth_sequence_voice*,uint64_t epoch,uint64_t transport,uint64_t occurrence,
                         const struct am_midi_event*,int transpose);
int am_synth_sequence_off(struct am_synth_sequence*,struct am_synth_sequence_voice*,uint64_t epoch,uint64_t transport,uint64_t occurrence);
/* Retry an unaccepted Off on the same native executor; BUSY retains its token.
 * Stop/New/Load use stop/configure so silence never depends on a free slot. */
int am_synth_sequence_retry(struct am_synth_sequence*,struct am_synth_sequence_voice*);
int am_synth_sequence_stop(struct am_synth_sequence*);
#endif
#ifdef __cplusplus
}
#endif
#endif
