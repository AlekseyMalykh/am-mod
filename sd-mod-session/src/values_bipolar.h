#ifndef VALUES_BIPOLAR_H
#define VALUES_BIPOLAR_H
#include <stdint.h>
/* Centre-based K bars for symmetric ranges. Optional; 0 if not applied. */
int values_bipolar_install(uintptr_t base);
int values_shift_pairs_install(uintptr_t base);
int values_loop_lock_install(uintptr_t base);
#endif
