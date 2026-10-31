#ifndef MPC_PAD_EQ_NATIVE_H
#define MPC_PAD_EQ_NATIVE_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Pre-main only; pins the exact four relocated instructions of the common
 * Instrument render method. Clone process RAM, never firmware files. */
int pad_eq_native_install(uintptr_t base);
/* Model-owner only. Program has native string identity atdc8. Export clones
 * retain that identity while owning independent Instrument/render buffers. */
void pad_eq_native_program(void*program);
void pad_eq_native_clear(void);
struct pad_eq_native_stats {uint64_t calls,configured,matched,processed;};
void pad_eq_native_stats_read(struct pad_eq_native_stats*);
/* Control/UI thread only; audio callback only increments lock-free counters. */
void pad_eq_native_report(void);
#ifdef __cplusplus
}
#endif
#endif
