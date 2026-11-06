#ifndef MPC_NOTE_NATIVE_H
#define MPC_NOTE_NATIVE_H
#include <stdint.h>
#include <stddef.h>
/* UI-thread calls, except active() and destroyed(). No audio or file writes. */
void note_native_init(uintptr_t base);
int note_native_footer(void *component, void *graphics);
int note_native_tick(void);
/* UI timer only: Mix Shift replaces K2 alone (Volume/Pan stay shown). */
void note_native_mix_shift_tick(void);
int note_native_active(void);
int note_native_control(void *event);
int note_native_reset_knob(unsigned column,unsigned shifted);
void note_native_button_requested(unsigned code,unsigned down);
int note_native_owns(unsigned cc);
void note_native_destroyed(void *component);
void note_native_observe_root(void *component);
int note_native_current(char *sample_name,size_t size);
int note_native_pad_index(unsigned *pad);
void *note_native_context(void); /* Borrowed CurrentThings, UI Timer only. */
/* UI owner: 0 unavailable, 1 already Poly, 2 queued by native Play command. */
int note_native_request_poly(void);
int note_native_sample_visible(void); /* Whole Sample screen, including Trim. */
void note_native_repaint_sample(void); /* UI owner only. */
void note_native_sample_overlay(void *screen,void *g);
int note_native_button(void *event); /* UI owner: Mix K2 colour focus; Shift restores native Kit Volume. */
#endif
