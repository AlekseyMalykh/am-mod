#ifndef AM_MIXER_CONTROL_H
#define AM_MIXER_CONTROL_H
#include "mixer_view.h"
#ifdef __cplusplus
extern "C" {
#endif
enum mixer_action {MIXER_B1,MIXER_B2,MIXER_B3,MIXER_PAD_FX,MIXER_KNOB,
    MIXER_SELECT,MIXER_NATIVE_MODE,MIXER_SHIFT};
struct mixer_control;
/* Transient state belongs exclusively to the UI owner. Input producers must
 * enqueue numeric events/tickets; they must never call this API concurrently.
 * Confirmed model and live audio lifetime are owned separately by session. */
struct mixer_control_ops {
    void*user;
    /* Complete desired snapshot. Nonzero means queued; revision must increase.
     * Submit runs only on control owner, never audio or input producer. */
    int(*submit)(void*,const struct mixer_project*,uint64_t expected,uint64_t*revision);
};
struct mixer_control*mixer_control_create(const struct mixer_control_ops*,double rate);
void mixer_control_destroy(struct mixer_control*);
/* Confirmed snapshot from the project/audio owner. Load/New/source epoch change
 * invalidates input ticket and pickup; stale acknowledgements are ignored. */
int mixer_control_confirm(struct mixer_control*,const struct mixer_project*,uint64_t revision,uint64_t source_epoch);
int mixer_control_open(struct mixer_control*,int audio_ready,int modal_blocked);
void mixer_control_close(struct mixer_control*);
int mixer_control_active(const struct mixer_control*);
uint64_t mixer_control_ticket(const struct mixer_control*);
/* 1 consumed,0 forward native. Late ups for owned downs are consumed even
 * after close/ticket change. Native mode closes before returning0. Shift is
 * observed but always forwarded as the native modifier. */
int mixer_control_button(struct mixer_control*,enum mixer_action,int down,uint64_t ticket);
int mixer_control_pad(struct mixer_control*,unsigned pad,int down,uint64_t ticket);
int mixer_control_cc(struct mixer_control*,unsigned cc,unsigned value,uint64_t ticket);
void mixer_control_tick(struct mixer_control*);
/* Native owner supplies actual occupied sounds, not table assignments. */
void mixer_control_presence(struct mixer_control*,uint64_t low,uint64_t high,unsigned bank);
int mixer_control_led(const struct mixer_control*,unsigned pad,uint32_t*rgb);
void mixer_control_meter(struct mixer_control*,const struct mixer_meter_snapshot*,uint64_t now_ms);
void mixer_control_view(struct mixer_control*,uint64_t now_ms,struct mixer_view_state*);
const char*mixer_control_message(const struct mixer_control*);
float mixer_fader_db(unsigned value);
#ifdef __cplusplus
}
#endif
#endif
