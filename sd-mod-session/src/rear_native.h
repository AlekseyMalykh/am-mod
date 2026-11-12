#ifndef MPC_REAR_NATIVE_H
#define MPC_REAR_NATIVE_H
#include "rear_audio.h"
#ifdef __cplusplus
extern "C" {
#endif
int rear_native_install(uintptr_t base);
typedef void *(*rear_render_fn)(void *,void *,void *);
void *rear_native_process(void *thread,void *position,void *buffer,rear_render_fn original);
void rear_native_bind(void *copy_context, void *services);
void rear_native_dead(void *copy_context);
void rear_native_project_changed(void);
void rear_native_tick(int record_screen_visible);
int rear_native_start(unsigned overwrite_mask);
int rear_native_busy(void);
int rear_native_source(void);
int rear_native_monitor(void);
void rear_native_monitor_on(void);
void rear_native_stop(void);
unsigned rear_native_occupied(void);
unsigned rear_native_bank_occupied(unsigned bank);
unsigned rear_native_status(void);
const char *rear_native_message(void);
void rear_native_clear(void);
int rear_ui_install(uintptr_t base);
int rear_ui_tick(void);
int rear_ui_input_config_visible(void);
int rear_ui_owns(unsigned kind, unsigned code);
int rear_ui_button(void *event);
int rear_ui_control(void *event);
int rear_ui_control_at(void *event,unsigned shift);
int rear_ui_reset_knob(unsigned column,unsigned shift);
void rear_ui_pad(unsigned pad, int down);
int rear_ui_pad_owns(void);
int rear_ui_light(unsigned pad, uint64_t *colour);
void rear_ui_fault(void);
#ifdef __cplusplus
}
#endif
#endif
