#ifndef VOICE_GUARD_H
#define VOICE_GUARD_H
#include <stdint.h>
/* Serialise native release-all-voices with the live audio render. */
int voice_guard_install(uintptr_t base);
void voice_guard_lock(void);
void voice_guard_unlock(void);
void voice_guard_stats(unsigned out[3]);
#endif
