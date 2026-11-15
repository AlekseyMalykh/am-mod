#ifndef MPC_RESAMPLE_UI_H
#define MPC_RESAMPLE_UI_H
#include <stdint.h>
#include "resample_view.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Installed only in a separately compiled experimental host. */
void resample_ui_init(uintptr_t base);
int resample_ui_shutdown_busy(void); /* UI thread, no cancellation or mutation. */
void resample_ui_bind(void *copy_context);
void resample_ui_context_dead(void *copy_context);
void resample_ui_project_changing(void);
void resample_ui_destroyed(void *sample_screen);
/* All remaining pointer operations are UI owner calls except requested,
 * owns and claim_pad, which only read/write numeric atomic producer state. */
void resample_ui_requested(unsigned code,int down);
int resample_ui_owns(unsigned kind,unsigned code);
int resample_ui_claim_pad(unsigned pad,int down,int blocked);
uint64_t resample_ui_ticket(void);
void resample_ui_pad(unsigned pad,int down,uint64_t ticket);
int resample_ui_button(void *event,uint64_t ticket);
int resample_ui_control(void *event,uint64_t ticket);
int resample_ui_tick(int blocked); /* Keep UI timer while active OR entry-ready. */
int resample_ui_active(void);
void resample_ui_paint(void *sample_screen,void *graphics);
/* Sample tuner probe: UI thread starts it, the native model queue reads the selected pad's PCM. state: 0 none, 1 working, 2 pitched, 3 no pitch. */
void sample_tuner_request(void);
void sample_tuner_poll(void);
unsigned sample_tuner_info(float *hz,unsigned *pad,char *name,unsigned size);
void resample_ui_paint_overlay(void *graphics); /* Sequence window: draws the dialog while active. */
/* Same painter contract as the ordinary runtime view; useful for offline QA. */
void resample_ui_draw(const struct resample_painter*);
int resample_ui_take_sequence(void);
void resample_ui_transport(unsigned running,unsigned valid);
int resample_ui_take_stop(void *native_event);
void resample_ui_fault(void);
void resample_ui_prime_eq(void);
int resample_ui_light(unsigned pad,uint64_t *colour);
uint64_t resample_ui_palette_generation(void);
#ifdef __cplusplus
}
#endif
#endif
