#ifndef MPC_PAD_RENDER_H
#define MPC_PAD_RENDER_H
#include "fx_chain.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Offline audio preparation only. Not connected to Program, SamplePool,
 * sequencer, live preview, file storage, or pad assignment. */
#define PAD_RENDER_MAX_PCM_BYTES (64u*1024u*1024u)
#define PAD_RENDER_BLOCK_FRAMES 256u
#define PAD_RENDER_MAX_STEP_FRAMES 4096u
enum pad_render_status {
    PAD_RENDER_WORKING, PAD_RENDER_DONE, PAD_RENDER_CANCELLED,
    PAD_RENDER_INVALID, PAD_RENDER_LIMIT, PAD_RENDER_RESOURCE, PAD_RENDER_AUDIO_ERROR,
    PAD_RENDER_FX_ERROR
};
struct pad_render_source {
    const float *pcm;        /* interleaved, normalized float, mono or stereo */
    uint64_t frames;
    unsigned channels;
    double rate;
};
/* Owned, stable source stream. read fills exactly count stereo frames at
 * absolute source offset, returns1 on success and0 on any short/error read.
 * Transfer ownership only when create_stream succeeds. close runs once on
 * the owner after capture, failure, cancellation or destroy. No UI/audio
 * callback use: native source reads may perform IO. */
struct pad_render_stream {
    void *user;
    uint64_t frames;
    double rate;
    int (*read)(void *,uint64_t first,size_t count,float *stereo);
    void (*close)(void *);
};
struct pad_render_request {
    uint64_t first,end;      /* end exclusive, on the original source */
    unsigned repeats;       /* explicit finite repeat count1..64; no auto Loop */
    uint64_t tail_frames;    /* silence fed through FX after the final repeat */
    size_t pcm_budget;      /* 0 ->64MiB; smaller budgets allowed, never larger */
    struct fx_chain_config chain;
};
struct pad_render_progress {
    enum pad_render_status status;
    uint64_t done,total;
    double peak;
    uint64_t over_unity_frames; /* report headroom; no silent normalization */
    uint64_t source_done,source_total;
};
struct pad_render_job;
/* Owner thread: copies the selected source region and FX settings. Caller
 * keeps source stable during create only; no borrowed audio after return.
 * May allocate; must not run on the device real-time audio callback. */
struct pad_render_job *pad_render_create(const struct pad_render_source *,
    const struct pad_render_request *,const struct fx_backend *,enum pad_render_status *error);
/* Same PCM budget, no additional full-size intermediate capture buffer.
 * Source capture is performed in bounded steps before rendering. */
struct pad_render_job *pad_render_create_stream(const struct pad_render_stream *,
    const struct pad_render_request *,const struct fx_backend *,enum pad_render_status *error);
/* Same owner: bounded work, never more than4096 frames. Fresh FX histories
 * per job. For rerender, caller supplies the retained original source,
 * not a previous output. */
enum pad_render_status pad_render_step(struct pad_render_job *,size_t frame_budget);
void pad_render_progress(const struct pad_render_job *,struct pad_render_progress *);
/* Only callable cross-thread function. Progress/output are owner-only too.
 * The owner observes cancel between blocks. Stop/join all users before
 * destroy. No global cancel shared between jobs. */
void pad_render_cancel(struct pad_render_job *);
/* Read-only output exists only after DONE, stereo at the original rate.
 * Owned by the job; valid until destroy, no native assignment is implied. */
const float *pad_render_output(const struct pad_render_job *,uint64_t *frames,double *rate);
void pad_render_destroy(struct pad_render_job *);
#ifdef __cplusplus
}
#endif
#endif
