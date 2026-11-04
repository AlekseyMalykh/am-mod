#ifndef AM_SAMPLE_PLAYING_H
#define AM_SAMPLE_PLAYING_H
#include <stdint.h>
int sample_playing_install(uintptr_t image);
/* UI reads numeric snapshots only. Native voice traversal stays on the same
 * lighting owner that calls the stock PlayingPadsLighting method. */
int sample_playing_active(const void *program,unsigned pad);
#endif
