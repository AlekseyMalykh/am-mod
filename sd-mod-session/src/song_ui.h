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
int song_ui_pads_active(void);   /* Song or Song Export visible (pad colours) */
int song_ui_selected_sequence(void); /* pad-selected sequence on the Song page, -1 unknown */
/* Producer side (input thread). A claimed button down is remembered so its
 * up is claimed too, even if Shift was released first. */
int song_ui_owns(unsigned kind,unsigned code,int shift,int down);
/* Consumer side (UI thread): 1 = handled. */
int song_ui_control(void *event);
int song_ui_button(void *event,int shift);
/* Native button sink for gestures that replay stock presses (Song Export Shift+B2 Stems). */
void song_ui_set_forward(void(*forward)(void*));
/* Hardware Shift edge, delivered by the native button hook. No JUCE access
 * on the producer thread: the Song UI timer handles repainting. */
void song_ui_shift_event(int down);
/* Notify after the actual native Shift-UP has been processed, never before.
 * B2 replay is deferred to the subsequent UI tick. */
void song_ui_stems_shift_up_forwarded(void);
/* Native Song Export filename editor handoff, not the parent Export screen. */
int song_ui_stems_name_pending(void);
void *song_ui_stems_name_editor(void);
int song_ui_stems_name_active(void);
/* Stock-style footer on the full Song Export page while its name modal is open. */
int song_ui_stems_modal_visible(void);
void song_ui_stems_input_mode(int pads);
void song_ui_stems_name_opened(void);
/* Attach a guarded native modal paintOverChildren footer from TextEditor::paint. */
void song_ui_stems_name_modal_attach(void *editor);
void song_ui_stems_name_button(unsigned code,int down);
void song_ui_stems_name_closed(void);
#endif
