#ifndef MPC_MUSIC_LIGHTING_H
#define MPC_MUSIC_LIGHTING_H
#include <stdint.h>
void music_lighting_init(uintptr_t base);
void music_lighting_created(void *layer);
void music_lighting_destroyed(void *layer);
void music_lighting_tick(void);
/* Ask the native layer for a full RefreshAll on the next tick, e.g. after a
 * mod workflow released the pads or a native transition settled. */
void music_lighting_request_refresh(void);
/* UI owner: live native state, independent of lighting refresh callbacks. */
int music_lighting_levels_state(unsigned *enabled,unsigned *mode,unsigned *origin);
#endif
