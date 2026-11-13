#ifndef AM_SAMPLE_PLAYING_H
#define AM_SAMPLE_PLAYING_H
#include <stdint.h>
/* UI owner only. Native direct links and Mute Group peers in one visible bank. */
unsigned sample_playing_links(const void *program,unsigned source,unsigned bank);
unsigned sample_playing_group(const void *program,unsigned source,unsigned bank);
int sample_playing_install(uintptr_t image);
/* Audio owner, after live Program render: bounded numeric snapshot, no UI,
 * allocation or locks. Offset is not treated specially. */
void sample_playing_render(const void *program);
/* UI reads numeric snapshots only. Live Mixer builds publish on the audio
 * owner; legacy builds use the native PlayingPadsLighting owner. */
int sample_playing_active(const void *program,unsigned pad);
#endif
