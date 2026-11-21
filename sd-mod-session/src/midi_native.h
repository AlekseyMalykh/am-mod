#ifndef AM_MIDI_NATIVE_H
#define AM_MIDI_NATIVE_H
#include <stdint.h>
/* Constructor-time install in the pinned clone, before music-owner dispatch. */
int am_midi_native_install(uintptr_t image);
/* Native music executor only, before the aggregate audio release. */
void am_midi_native_release_choice(void *track,unsigned source,unsigned packed_choice);
void am_midi_native_key_begin(void *owner,unsigned key,unsigned layer);
void am_midi_native_key_end(void);
void am_midi_native_key_release(void *owner,unsigned key,void *track);
#endif
