#ifndef MPC_BATCH_EVENTS_H
#define MPC_BATCH_EVENTS_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Opaque owned list of native event commands. Snapshot before copying any
 * sounds. Each command targets its original sequence's existing event list. */
void *batch_events_prepare(uintptr_t base,void *project,void *things,void *program,
                          const unsigned *sources,const unsigned *targets,unsigned count);
int batch_events_perform(void *commands);
int batch_events_undo(void *commands);
void batch_events_free(void *commands);
unsigned batch_events_count(void *commands);
#ifdef __cplusplus
}
#endif
#endif
