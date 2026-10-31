#ifndef MPC_RESAMPLE_CONTEXT_H
#define MPC_RESAMPLE_CONTEXT_H
#include "resample_native.h"
#ifdef __cplusplus
extern "C" {
#endif
struct resample_context;
struct resample_selection {
    unsigned pad;
    char sample[128];
    uint64_t occupied[2];
};
/* Pinned AC50CopyMode base subobject, +48 from complete derived object.
 * Lifecycle owner must call changed BEFORE project replacement, and stop
 * BEFORE native context destructor. These do not wait on UI/native queue.
 * The tracking object has its own refcount, never context+50 queue counter.
 * It does NOT extend native service/model lifetime: stock history must be
 * cleared while its native dependencies are still alive. */
struct resample_context *resample_context_create(uintptr_t base,void *copy_context);
int resample_context_retain(struct resample_context*);
void resample_context_changed(struct resample_context*);
void resample_context_stop(struct resample_context*);
void resample_context_release(struct resample_context*);
/* Intrusive runtime-owned lifetime hook. stop() may only set numeric queue
 * state, never wait for UI/worker or invoke native model functions. remove
 * joins any stop invocation before the node/user can be destroyed. */
struct resample_context_hook {
    void (*stop)(void*);
    void *user;
    struct resample_context_hook *next;
};
int resample_context_add_hook(struct resample_context*,struct resample_context_hook*);
void resample_context_remove_hook(struct resample_context*,struct resample_context_hook*);
/* Existing native queue owner only, under normal model exclusion. Captures
 * CurrentThings program/selected instrument, ProjectProperties/pool, manager,
 * command queue service, real Sample services and occupied pads. No mutation.
 * Output config owns ONE reference: release it once via commit.release(user).
 * External hooks still must ensure queue/context lifetime during capture.
 * Input button/paint code must never read these model pointers from UI. */
int resample_context_capture(struct resample_context*,struct resample_native_config*,
    struct resample_selection*);
/* Numeric, thread-safe last capture failure; returned string has static life.
 * No file name or borrowed native pointer crosses to UI. */
const char *resample_context_error(struct resample_context*);
/* Recovery remains owned after native history drops it. No automatic model
 * restoration; caller must restore exact original state on model owner first.
 * Count is thread-safe, attempt is native-owner only. A nonzero count blocks
 * a clean in-process unload, but MUST NOT wait inside context destructor. */
unsigned resample_context_recovery_count(struct resample_context*);
int resample_context_finish_recovery(struct resample_context*);
#ifdef __cplusplus
}
#endif
#endif
