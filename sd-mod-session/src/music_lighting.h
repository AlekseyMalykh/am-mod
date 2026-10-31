#ifndef MPC_MUSIC_LIGHTING_H
#define MPC_MUSIC_LIGHTING_H
#include <stdint.h>
void music_lighting_init(uintptr_t base);
void music_lighting_created(void *layer);
void music_lighting_destroyed(void *layer);
void music_lighting_tick(void);
/* UI owner: live native state, independent of lighting refresh callbacks. */
int music_lighting_levels_state(unsigned *enabled,unsigned *mode,unsigned *origin);
#endif
