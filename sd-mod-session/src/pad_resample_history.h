#ifndef MPC_PAD_RESAMPLE_HISTORY_H
#define MPC_PAD_RESAMPLE_HISTORY_H
#include "pad_resample_commit.h"
#ifdef __cplusplus
extern "C" {
#endif
/* One native history entry: register + assign + Undo pool cleanup.
 * Native model owner/exclusion only. Same pinned Command base as Batch Copy.
 * Required hold callback takes ownership of a quarantined command if native
 * history drops its LAST reference while rollback still needs recovery.
 * Host MUST preserve context/services and call finish_recovery only after
 * restoring the original model. Callback must not throw/reenter. */
int pad_history_prepare(const struct pad_commit_context*,const uintptr_t sample[2],
    unsigned target,int overwrite,void (*hold)(void *user,void *quarantined),
    uintptr_t command[2]);
/* Only for a quarantined pointer handed to hold; success destroys it. */
int pad_history_finish_recovery(void *quarantined);
/* Intrusive quarantine bookkeeping, only after hold callback receives object.
 * Link belongs to the still-live History allocation; no extra allocation. */
void **pad_history_recovery_next(void *quarantined);
void *pad_history_recovery_user(void *quarantined);
#ifdef __cplusplus
}
#endif
#endif
