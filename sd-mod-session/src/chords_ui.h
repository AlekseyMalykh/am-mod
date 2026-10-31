#ifndef MPC_CHORDS_UI_H
#define MPC_CHORDS_UI_H
#include <stdint.h>
void chords_ui_init(uintptr_t base);
/* Poll actual modal ownership; background observers/entry readiness do not block. */
void chords_ui_poll(int naming_or_steps);
void chords_ui_tick(int available);
int chords_ui_button(void *event);
int chords_ui_button_owns(unsigned code);
void chords_ui_button_input(unsigned code,int down);
int chords_ui_control(void *event);
int chords_ui_owns(unsigned cc);
int chords_ui_paint(void *root,void *graphics);
int chords_ui_button_light(unsigned code,unsigned *value);
#endif
