#ifndef AM_MIDI_UI_H
#define AM_MIDI_UI_H
#include <stdint.h>
void am_midi_ui_init(uintptr_t);
void am_midi_ui_paint(void*,void*);
void am_midi_ui_destroyed(void*);
void am_midi_ui_tick(void*); /* UI owner: native root, NULL when hidden */
int am_midi_ui_active(void);
int am_midi_ui_owns(unsigned code,int button);
int am_midi_ui_control(void*,unsigned shift);
int am_midi_ui_button(void*);
/* Control-surface thread: a physical pad (absolute 0..127) was struck. Only
 * atomics cross threads; the UI owner applies it in am_midi_ui_tick. */
void am_midi_ui_pad_hit(unsigned pad);
#endif
