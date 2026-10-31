#ifndef MPC_PAD_RESAMPLE_COMMIT_H
#define MPC_PAD_RESAMPLE_COMMIT_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
enum pad_commit_state { PAD_COMMIT_READY, PAD_COMMIT_APPLIED,
    PAD_COMMIT_UNDONE, PAD_COMMIT_RECOVERY_REQUIRED };
enum pad_commit_operation { PAD_COMMIT_APPLY, PAD_COMMIT_UNDO, PAD_COMMIT_RECOVER };
struct pad_commit_context {
    uintptr_t base;
    void *properties,*program,*service,*pool,*user;
    /* Required, synchronous model-owner check. Host must validate current
     * project/source identity and unbaked source settings under exclusion.
     * No mutation/reentrancy. These borrowed services outlive Undo history. */
    int (*validate)(void*,enum pad_commit_operation);
    /* Optional paired lease for user/services. One retain per commit object,
     * released only after its native references/history/recovery are gone. */
    int (*retain)(void*);
    void (*release)(void*);
};
struct pad_resample_commit;
/* Model owner only, inside one future native history command, never UI/audio.
 * Preparation retains result and all target layers; it does NOT register or
 * assign. The generated Sample must not already belong to the pool. Target
 * instrument must exist. Deliberately layer0, no full Instrument reset.
 * Occupied target needs explicit permission. Source/model guard is required.
 * This core is NOT itself a native CommandManager command. */
struct pad_resample_commit *pad_commit_prepare(const struct pad_commit_context*,
    const uintptr_t result[2],unsigned target,int overwrite_confirmed);
/* 1=completed, 0=refused/rolled back. A recovery-required state retains all
 * references and MUST block host teardown/history eviction until recovered.
 * apply also performs redo after Undo, resolving a fresh unique name. */
int pad_commit_apply(struct pad_resample_commit*);
int pad_commit_undo(struct pad_resample_commit*);
/* Finish rollback AFTER host recovery restored the exact original layers.
 * Does not replay native Undo (its cached instrument is cleared on first use).
 * Same owner/exclusion and valid original library entries; no force teardown. */
int pad_commit_recover(struct pad_resample_commit*);
enum pad_commit_state pad_commit_status(const struct pad_resample_commit*);
/* Never silently Undo on history eviction. Applied result stays in pool;
 * undone/prepared result is unregistered. Returns0 in recovery-required. */
int pad_commit_destroy(struct pad_resample_commit*);
#ifdef __cplusplus
}
#endif
#endif
