#ifndef MPC_SONG_UI_H
#define MPC_SONG_UI_H
#include <stdint.h>
/* Song Mode additions over the stock SongScreen (pinned 1.3.0.12):
 * K1 repeats of the selected step, Shift+B3 replace its sequence with the
 * pad-selected one, Shift+B2 duplicate it, Shift+-/+ move it.
 * Every edit goes through the stock undoable Song step commands. */
void song_ui_init(uintptr_t base);
int song_ui_install(void);      /* SongScreen vtable slots; constructor time only */
void song_ui_tick(void);        /* UI Timer */
int song_ui_active(void);       /* SongScreen visible */
int song_ui_selected_sequence(void); /* pad-selected sequence on the Song page, -1 unknown */
/* Producer side (input thread). A claimed button down is remembered so its
 * up is claimed too, even if Shift was released first. */
int song_ui_owns(unsigned kind,unsigned code,int shift,int down);
/* Consumer side (UI thread): 1 = handled. */
int song_ui_control(void *event);
int song_ui_button(void *event);
#endif
