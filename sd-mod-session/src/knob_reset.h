#ifndef AM_KNOB_RESET_H
#define AM_KNOB_RESET_H
#include <stdint.h>
void knob_reset_init(uintptr_t base);
/* Numeric modifier snapshot only; does not consume or replay a bank button. */
void knob_reset_button(unsigned code,unsigned down);
uint64_t knob_reset_ticket(void);
int knob_reset_claim(unsigned cc);
/* UI owner only. Custom parameter owners get first refusal. */
void knob_reset_observe(void *slider);
void knob_reset_forget(void *screen);
int knob_reset_native(unsigned column,unsigned shifted);
int knob_reset_begin(uint64_t ticket,unsigned cc);
#endif
