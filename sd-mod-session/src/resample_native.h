#ifndef MPC_RESAMPLE_NATIVE_H
#define MPC_RESAMPLE_NATIVE_H
#include "resample_worker.h"
#include "pad_memory_sample.h"
#include "pad_resample_history.h"
#ifdef __cplusplus
extern "C" {
#endif
struct resample_native_config {
    struct pad_commit_context commit;
    /* Optional expected services; all-zero discovers them from real source.
     * If provided, all three must match the source Sample's actual services. */
    struct pad_memory_services memory;
    void *manager;
    void (*hold_recovery)(void*,void*);
    struct fx_backend fx;
};
struct resample_native;
/* Owning non-realtime queue only. Required paired commit context lease must
 * cover services/guard through native Undo history and any recovery handoff.
 * Host validate checks current project/source/unbaked settings and target
 * expectation; actual UI-context capture remains the host's responsibility. */
struct resample_native *resample_native_create(const struct resample_native_config*);
void resample_native_backend(struct resample_native*,struct resample_worker_backend*);
/* Refuses during borrowed PCM/native preparation. Destroy worker first after
 * its cancellation/Apply acknowledgement, then this adapter. History retains
 * its OWN context lease independently of either object. */
int resample_native_destroy(struct resample_native*);
/* UI-readable coherent numeric snapshot. Join readers before destroy. */
void resample_native_progress(struct resample_native*,uint64_t *done,uint64_t *total);
#ifdef __cplusplus
}
#endif
#endif
