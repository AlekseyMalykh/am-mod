#ifndef MPC_STEP_NATIVE_H
#define MPC_STEP_NATIVE_H
#include <stdint.h>
/* Native MPC event variant. Only tag 3 is copied: its payload is inline data,
 * not the refcounted ownership used by variant 4. */
struct step_event { int64_t time; uint32_t type, reserved; unsigned char data[40]; };
void step_native_init(uintptr_t base);
void step_native_levels_processor(uintptr_t original);
int step_native_sample_chop(void);
void step_native_levels_state(unsigned enabled,unsigned mode,unsigned origin);
void step_native_native_mute(unsigned enabled);
/* visible:0 hidden,1 Sample,2 Seq with actual native Mute active. */
void step_native_playback_context(void *current_things,int transpose,int visible);
int step_native_install(void (*paint)(void *,void *));
void step_native_paint(void *screen,void *graphics);
void step_native_event_label(int pad,const struct step_event *,uint64_t *string);
void step_native_destroyed(void *screen);
void step_native_project_changed(void);
void step_native_clock(void *now_position);
unsigned step_native_transport_state(void); /* bit0 playing, bit1 valid */
void step_native_bank_created(void *owner);
void step_native_bank_destroyed(void *owner);
int step_native_tick(void);
/* UI thread: Tune pitches currently flashed from sequence playback. */
int step_native_playback_pitches(int *out,int max);
/* Last engine tick seen by the native transport timer (960 per quarter). */
int64_t step_native_clock_ticks(uint64_t *serial,unsigned *running);
int step_native_browser_bank(int bank); /* UI only; -1 reads, 0..7 selects bank. */
int step_native_active(void);
int step_native_custom(void);
int step_native_sound_layout(void); /* Ordinary Events pad layout, no modal/picker. */
int step_native_record_light(unsigned *value);
int step_native_button_light(unsigned code,unsigned *value);
int step_native_pad_route(uint64_t ticket); /* 0=native, 1=step/picker, 2=Shift shortcut, 3=Events Erase chord */
int step_native_shortcut(uint64_t ticket);
uint64_t step_native_ticket(void);
void step_native_requested(unsigned code,unsigned down);
int step_native_owns(unsigned cc);
int step_native_button_owns(unsigned code);
int step_native_control(void *event,uint64_t ticket);
int step_native_button(void *event,uint64_t ticket); /* 0=unclaimed, 1=consumed, 2=observed; forward native */
int step_native_button_at(void *event,uint64_t ticket,uint64_t when_ms);
void step_native_pad(unsigned pad,uint64_t ticket);
void step_native_pad_release(unsigned pad,uint64_t ticket);
void step_native_audition_created(void *native_audition);
void step_native_audition_bind(void *queues,void *current_things,void *sequencer_receiver);
int step_native_event_preview_enabled(void);
int step_native_preview_suppressed(void *track,const void *event,unsigned source_mask);
int step_native_chop_audition(void *program,unsigned source,unsigned slice,int down);
void step_native_queue_fault(void);
int step_native_light(unsigned pad,uint64_t *light);
#endif
