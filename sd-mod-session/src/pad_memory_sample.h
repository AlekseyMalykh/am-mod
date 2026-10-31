#ifndef MPC_PAD_MEMORY_SAMPLE_H
#define MPC_PAD_MEMORY_SAMPLE_H
#include "pad_output_stream.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Actual Sample ctor services from the pinned native model owner. Borrowed;
 * their project/application lifetime MUST cover the resulting Sample. */
struct pad_memory_services {void *queue_context,*storage,*resources;};
/* Owner only, pinned actual Sample ctor fields: +48 queue,+160 storage,
 * +168 resources. Borrowed values; retain Sample AND application services. */
int pad_memory_services_from_sample(uintptr_t base,const uintptr_t sample[2],
    struct pad_memory_services*);
struct pad_memory_job;
/* Owner worker only. Pins immutable completed output, creates real Sample,
 * metadata and MemorySample, reserves native float-vector storage. No WAV,
 * SamplePool entry, pad assignment or thread creation. Both original output
 * and new native PCM count against the given budget (0 ->64MiB). Source PCM
 * must be the completed renderer output, whose source buffer is released. */
struct pad_memory_job *pad_memory_begin(uintptr_t base,const struct pad_memory_services*,
    const struct pad_output_reference*,const char *name,size_t pcm_budget,enum pad_render_status*);
/* Copies at most4096 stereo frames, with cancellation and finite checks. */
enum pad_render_status pad_memory_step(struct pad_memory_job*,size_t frames);
/* Worker owner only. Cancellation is not an audio-thread operation. */
void pad_memory_cancel(struct pad_memory_job*);
void pad_memory_progress(const struct pad_memory_job*,uint64_t *done,uint64_t *total);
/* Completed result only. Transfers a native shared_ptr Sample pair once. */
int pad_memory_take(struct pad_memory_job*,uintptr_t sample[2]);
void pad_memory_destroy(struct pad_memory_job*);
#ifdef __cplusplus
}
#endif
#endif
