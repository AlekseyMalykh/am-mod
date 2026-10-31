#ifndef MPC_PAD_SAMPLE_POOL_H
#define MPC_PAD_SAMPLE_POOL_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Native SamplePool operations, owner/command exclusion only. Register may
 * choose a unique name. These mutate the project library: use during the
 * future Apply transaction, never implicitly while rendering/Compare.
 * Caller retains its Sample reference throughout; registration is not Undo. */
int pad_sample_pool_contains(uintptr_t,void *pool,const uintptr_t sample[2]);
int pad_sample_pool_register(uintptr_t,void *pool,const uintptr_t sample[2]);
/* Rollback/Undo only for this operation's new Sample, never the source. */
int pad_sample_pool_remove(uintptr_t,void *pool,const uintptr_t sample[2]);
#ifdef __cplusplus
}
#endif
#endif
