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
#ifdef __cplusplus
}
#endif
#endif
