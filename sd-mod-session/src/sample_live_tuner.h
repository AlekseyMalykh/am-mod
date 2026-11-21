#ifndef AM_SAMPLE_LIVE_TUNER_H
#define AM_SAMPLE_LIVE_TUNER_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* UI owner: NULL program disables capture; no native pointer is dereferenced. */
void sample_live_tuner_target(uintptr_t program,unsigned pad);
/* Live engine clock, including blocks where the selected Sound is silent. */
void sample_live_tuner_clock(unsigned frames,unsigned rate);
/* Audio owner, one live engine only. NULL channels mean a silent buffer.
 * Bounded PCM copies only: no allocation, locks, waits or pitch analysis. */
void sample_live_tuner_push(uintptr_t program,unsigned pad,const float *left,
                            const float *right,unsigned frames,unsigned rate);
/* UI: only fresh pitch of the currently requested Sound is returned. */
int sample_live_tuner_info(float *hz,unsigned *pad);
#ifdef __cplusplus
}
#endif
#endif
