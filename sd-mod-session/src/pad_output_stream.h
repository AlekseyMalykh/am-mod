#ifndef MPC_PAD_OUTPUT_STREAM_H
#define MPC_PAD_OUTPUT_STREAM_H
#include "pad_render.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Immutable completed stereo PCM with explicit owner. retain returns1 only
 * after taking a reference; release drops it. Both must be nonthrowing.
 * Caller keeps its reference until open returns. No PCM duplication. */
struct pad_output_reference {
    void *owner;
    const float *pcm;
    uint64_t frames;
    double rate;
    int (*retain)(void*);
    void (*release)(void*);
};
/* Native owned ProcessingStream pair {deleter,reader}. Pinned1.3.0.12 ABI.
 * Runs off the realtime callback; output[0](output[1]) releases its owner.
 * Read uses native requested-frame/owned-block ABI; release each completed
 * block before requesting another one. This creates no SamplePool entry,
 * pad assignment, file or Undo command. */
enum pad_render_status pad_output_stream_open(uintptr_t base,
    const struct pad_output_reference*,uintptr_t output[2]);
#ifdef __cplusplus
}
#endif
#endif
