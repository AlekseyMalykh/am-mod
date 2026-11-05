#ifndef MPC_TAKEOVER_NATIVE_H
#define MPC_TAKEOVER_NATIVE_H
#include <stdint.h>
int takeover_native_check(uintptr_t base);
void takeover_native_context_changed(void);
void takeover_native_forward(void *event,void(*receive)(void*));
#endif
