#ifndef AM_MIXER_INPUT_H
#define AM_MIXER_INPUT_H
#include "mixer_control.h"
#ifdef __cplusplus
extern "C" {
#endif
/* UI FIFO consumer only. Producers publish numeric edges/time/ticket. No native
 * object or callback pointer is retained here. Native button IDs are pinned to
 * 1.3.0.12; see check_mixer_input_native.c and docs/MIXER_INPUT.md. */
int mixer_input_button(struct mixer_control*,unsigned code,int down,uint64_t ticket);
enum mixer_entry_action {
    MIXER_ENTRY_PASS,       /* original physical event */
    MIXER_ENTRY_CONSUME,    /* retain/drop, never send an unmatched native edge */
    MIXER_ENTRY_OPEN,       /* consumed short release; open Overview */
    MIXER_ENTRY_LONG_DOWN,  /* tick: replay captured native Shift+pad9 down */
    MIXER_ENTRY_LONG_PAIR,  /* long release before tick: replay captured down/up */
    MIXER_ENTRY_NATIVE_UP,  /* pair a previously replayed native down */
    MIXER_ENTRY_CANCELLED   /* no native down was sent; retain ownership of up */
};
struct mixer_entry {
    uint64_t started_ms,ticket;
    unsigned hold_ms,held,armed,native_down;
};
/* One gesture owner/timer. Native stock opens Fader on down, not on a long
 * timeout. 500ms is an AM initial policy, not a claimed firmware constant. */
#define MIXER_ENTRY_HOLD_MS 500u
void mixer_entry_init(struct mixer_entry*,unsigned hold_ms);
enum mixer_entry_action mixer_entry_pad(struct mixer_entry*,unsigned pad,int down,
    uint64_t when_ms,uint64_t ticket,int shift,int audio_ready,int blocked);
enum mixer_entry_action mixer_entry_tick(struct mixer_entry*,uint64_t when_ms,
    uint64_t ticket,int audio_ready,int blocked);
/* Invalidates a pending short/long decision but preserves paired native up. */
void mixer_entry_cancel(struct mixer_entry*);
#ifdef __cplusplus
}
#endif
#endif
