#ifndef MPC_RESAMPLE_FAULT_H
#define MPC_RESAMPLE_FAULT_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Diagnostic only: record fault registers to inherited stderr, then restore
 * and deliver the original SIGSEGV action. No recovery/model/file API calls. */
int resample_fault_install(uintptr_t application_base);
void resample_fault_stage(unsigned stage);
#ifdef __cplusplus
}
#endif
#endif
