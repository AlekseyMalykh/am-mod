#ifndef AM_SYNTH_ACCEPT_NATIVE_H
#define AM_SYNTH_ACCEPT_NATIVE_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Post-common-filter native instrument command boundary. The original virtual
 * backend is always called verbatim; an accepted Synth layer does not change
 * existing MIDI output routing. Callbacks must not throw or retain arguments.
 * Read-only 44-byte command, frame offset and output lifetime are borrowed
 * for this callback only. Does not open or require an external MIDI port. */
typedef void*(*am_synth_accept_tap)(void*,uint64_t,int,void*,const void*);
typedef void(*am_synth_accept_end)(void*cookie,int failed);
int am_synth_accept_native_install(uintptr_t,am_synth_accept_tap,am_synth_accept_end);
/* Stack-owned scope around the real native heap drain. Only the music
 * executor borrows it; no allocation, runtime pointers or IDs are saved. */
#define AM_SYNTH_DRAIN_OCCURRENCES 256u
struct am_synth_drain_scope {void*engine;uint64_t offset;uint64_t ids[AM_SYNTH_DRAIN_OCCURRENCES];struct am_synth_drain_scope*previous;};
typedef void(*am_synth_drain_fn)(void*,uint64_t,unsigned);
void am_synth_native_drain_scope(void*,uint64_t,unsigned,am_synth_drain_fn,
                                void(*begin)(struct am_synth_drain_scope*),void(*finish)(struct am_synth_drain_scope*,int));
#ifdef __cplusplus
}
#endif
#endif
