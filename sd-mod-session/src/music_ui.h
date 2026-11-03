#ifndef MPC_MUSIC_UI_H
#define MPC_MUSIC_UI_H
int music_ui_exit_saving(void);
#include <stdint.h>
void music_ui_init(uintptr_t base);
void music_ui_requested(unsigned code,unsigned down);
int music_ui_sequence_visible(void);
int music_ui_chords_available(void);
/* Last native Note Repeat rate seen, as a Chords rate index, or -1. */
int music_ui_repeat_rate(void);
int music_ui_shift_active(void);
int music_ui_repeat_active(void);
int music_ui_filter_spectrum_visible(void *sample);
int music_ui_waveform_tint(uint32_t *rgb);
/* UI thread only; actual event containers, including automation/muted events. */
unsigned music_ui_sequence_presence(unsigned bank);
int music_ui_sequence_selected(void);
int music_ui_install(void (*sample_paint)(void*,void*),void (*project_paint)(void*,void*),void (*tab_paint)(void*,void*));
void music_ui_tab(void *button,void *graphics);
void music_ui_sample(void *root,void *graphics);
void music_ui_project(void *root,void *graphics);
void music_ui_destroyed(void *root);
void music_ui_request_project(void *root);
int music_ui_tick(void);
int music_ui_active(void);
int music_ui_owns(unsigned cc);
int music_ui_button_owns(unsigned code);
int music_ui_control(void *event);
int music_ui_reset_knob(unsigned column,unsigned shifted);
int music_ui_button(void *event);
#endif
