#ifndef MPC_RESAMPLE_QUEUE_H
#define MPC_RESAMPLE_QUEUE_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
struct resample_queue;
/* Pinned existing Copy owner queue, context+48 and lifetime counter+50.
 * tick runs once per closure on its owner; return1 for another bounded step.
 * No new thread. User/context must live until destroy succeeds. */
struct resample_queue *resample_queue_create(uintptr_t base,void *context,
    int (*tick)(void*),void *user);
/* UI wake after enqueuing commands; duplicate wake coalesces without losing
 * work arriving during a running tick.0=queue rejected; caller must retry. */
int resample_queue_wake(struct resample_queue*);
/* Permanently prohibit new posts before native context destruction. Does not
 * dereference native memory or wait for a worker; queued closures remain owned
 * by the native destructor, and running tick may finish one bounded step. */
void resample_queue_stop(struct resample_queue*);
/*1=pending/running,0=idle,-1=posting failure,-2=stopped/idle. Thread-safe. */
int resample_queue_status(struct resample_queue*);
/* After UI callers joined; refuses while any queued/running closure exists. */
int resample_queue_destroy(struct resample_queue*);
#ifdef __cplusplus
}
#endif
#endif
