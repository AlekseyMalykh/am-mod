#ifndef MPC_RESAMPLE_WORKER_H
#define MPC_RESAMPLE_WORKER_H
#include "resample_flow.h"
#include "pad_sample_source.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Existing non-real-time worker pumps this object: no new thread. UI can
 * call send/poll only. Native adapters execute on the owning worker.
 * FX-only prototype: pad Tune/envelope/filter/velocity curve are not baked. */
struct resample_worker_backend {
    void *user;
    int (*open)(void*,unsigned source,struct pad_sample_selection*);
    /* Optional native mix adapter: must acknowledge a safe source switch.
     * While rendered=1 PCM is borrowed and retained by the worker. Returning
     *0 must leave the previous routing intact. rendered=0 drops that borrow. */
    int (*compare)(void*,unsigned source,const float*,uint64_t,double,int rendered);
    /* Optional native undo transaction. MUST revalidate project/source/target
     * identity, refuse unexpected target changes, and require the explicit
     * overwrite_confirmed permission for an occupied target. Returning1
     * means completed with its own Sample ownership. No raw PCM retained. */
    int (*apply)(void*,const struct resample_spec*,const float*,uint64_t,double);
    /* compare/apply may return2 for PENDING instead of blocking on a native
     * queue. poll must then be present and nonblocking:0 failed,1 done,
     *2 pending. Operation1=compare,2=restore Original,3=apply transaction. */
    int (*poll)(void*,unsigned operation);
    struct fx_backend fx;
    /* Optional pre-commit cancellation, owner only.1 acknowledges that a
     * later failed Apply poll is cancellation,0 means commit may still win. */
    int (*cancel_apply)(void*);
};
struct resample_worker;
struct resample_worker_event {struct resample_update update;char error[96];};
struct resample_worker *resample_worker_create(const struct resample_worker_backend*);
/* send copies values into a bounded queue. Sequence and FX editor commands
 * belong to the UI host; this worker rejects them. False never enqueues. */
int resample_worker_send(void*,const struct resample_command*);
/* Worker owner only; at most one command and4096 source/output frames.
 * IO may block: this MUST NOT be called from the real-time audio callback. */
void resample_worker_step(struct resample_worker*);
/* Owner only, for rescheduling on the existing native queue. */
int resample_worker_work_pending(struct resample_worker*);
/* UI reader only; coalesced numeric progress/terminal snapshot with serial.
 * update.error is rebound to caller-owned event.error on a successful poll. */
int resample_worker_poll(struct resample_worker*,uint64_t *after,struct resample_worker_event*);
/* Owner after all send/poll callers joined. Returns0 and retains all memory
 * if a mix adapter failed to restore Original; caller must retry cleanup. */
int resample_worker_destroy(struct resample_worker*);
#ifdef __cplusplus
}
#endif
#endif
