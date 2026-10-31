#ifndef MPC_PAD_ASSIGNMENT_H
#define MPC_PAD_ASSIGNMENT_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Pinned1.3.0.12 AssignSampleCommand builder, not a complete Apply adapter.
 * Native model owner only, under its ordinary project/command exclusion.
 * Sample MUST already be registered in this project's SamplePool; the native
 * command resolves its name at perform/undo/redo. Caller must keep project,
 * properties, pool and command services alive through history lifetime.
 * Layer0..7; this prototype deliberately forbids assigning all layers.
 * output is a libstdc++ shared_ptr ABI pair, never an owned stream pair.
 * No dispatch, pool insertion, parameter reset, file write or queue wait. */
int pad_assignment_prepare(uintptr_t base,void *properties,void *program,
    void *command_service,void *pool,const uintptr_t sample[2],
    unsigned target,unsigned layer,uintptr_t output[2]);
/* Drops only caller's reference. A CommandManager may retain its own copy. */
void pad_assignment_release(uintptr_t base,uintptr_t command[2]);
#ifdef __cplusplus
}
#endif
#endif
