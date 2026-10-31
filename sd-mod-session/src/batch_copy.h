#ifndef MPC_BATCH_COPY_H
#define MPC_BATCH_COPY_H
#include <stdint.h>
#define BATCH_MAX_ITEMS 16u
enum { BATCH_JOB_IDLE, BATCH_JOB_PREPARE, BATCH_JOB_COPY, BATCH_JOB_ROLLBACK,
       BATCH_JOB_DONE, BATCH_JOB_FAILED };
struct batch_copy_progress {unsigned phase,done,total;};
#ifdef __cplusplus
extern "C" {
#endif
void batch_copy_init(uintptr_t base);
int batch_copy_submit(uintptr_t base,void *context,const unsigned *sources,const unsigned *destinations,unsigned count);
/* One owned job on the same native ThreadQueues worker used by stock Copy.
 * UI reads only atomic progress; acknowledge completion before another job. */
int batch_copy_begin(uintptr_t base,void *context,const unsigned *sources,const unsigned *destinations,unsigned count);
int batch_copy_begin_events(uintptr_t base,void *context,const unsigned *sources,const unsigned *destinations,unsigned count,int events);
int batch_copy_submit_events(uintptr_t base,void *context,const unsigned *sources,const unsigned *destinations,unsigned count,int events);
void batch_copy_poll(struct batch_copy_progress *result);
int batch_copy_release(void);
/* Close native Copy mode on its own worker, as stock1517dc0 does. UI must
 * wait for DONE before releasing its pad capture. No command is performed. */
enum {BATCH_EXIT_IDLE,BATCH_EXIT_PENDING,BATCH_EXIT_DONE,BATCH_EXIT_FAILED};
void batch_copy_hold_native_exit(int hold);
int batch_copy_native_release_exit(void *mode,int reason,int id);
int batch_copy_exit_begin(uintptr_t base,void *context);
unsigned batch_copy_exit_poll(void);
void batch_copy_exit_reset(void);
enum {BATCH_PREVIEW_IDLE,BATCH_PREVIEW_QUEUED,BATCH_PREVIEW_PLAYING,BATCH_PREVIEW_RECORDING,BATCH_PREVIEW_FAILED};
int batch_copy_preview(uintptr_t base,void *context,unsigned source,int down);
unsigned batch_copy_preview_status(void);
/* Event editor uses the ordinary Sample/16 Levels input on the native worker.
 * The complete 56-byte event is copied before enqueue; no borrowed UI data. */
int batch_event_preview(uintptr_t base,void *context,void *program,const void *event,unsigned key,unsigned mode);
int batch_event_preview_stop(uintptr_t base,void *context,int cancel_pending);
int batch_event_preview_override(void *properties,void *generated_event);
unsigned batch_event_preview_status(void);
unsigned batch_event_preview_pending(void);
/* Chops are independent native Extract results, not pad Trim aliases.
 * slices[i] is a zero-based source slice index, copied before dispatch. */
int batch_chop_begin(uintptr_t base,void *context,void *services,unsigned source,
                     const unsigned *slices,const unsigned *destinations,unsigned count);
int batch_chop_submit(uintptr_t base,void *context,void *services,unsigned source,
                      const unsigned *slices,const unsigned *destinations,unsigned count);
/* One independent native mono Sample, channel0 Left/1 Right. Same-pad
 * replacement is supported; Copy/Assign/conversion form one Undo command. */
int batch_mono_begin(uintptr_t base,void *context,void *services,unsigned source,unsigned target,unsigned channel);
int batch_mono_submit(uintptr_t base,void *context,void *services,unsigned source,unsigned target,unsigned channel);
void *batch_copy_source(void *program,int source);
#ifdef __cplusplus
}
#endif
#endif
