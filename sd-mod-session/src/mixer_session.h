#ifndef AM_MIXER_SESSION_H
#define AM_MIXER_SESSION_H
#include "mixer_live.h"
#include "mixer_control.h"
#ifdef __cplusplus
extern "C" {
#endif
struct mixer_session;
/* Native phone UI thread only. Pass NULL on the same thread before destroying
 * a bound session. The caller retains ownership and drives owner_tick. */
int mixer_phone_bind_ui(struct mixer_session*);
/* UI owner: Output of absolute sound 0..127 (0 Master, 1..8 Track A-H), -1 = Mixer audio not ready. */
int mixer_phone_pad_output(unsigned sound);
int mixer_phone_set_pad_output(unsigned sound,unsigned output);
/* Create and owner_tick on one non-audio owner. Native engine/Program pointers
 * serve as opaque identities. The automatic runtime uses the phone UI owner for
 * its AM model and DSP preparation. Engine/Program reads stay on audio;
 * independently validated application FX formats are borrowed on the owner. */
struct mixer_session*mixer_session_create(uint64_t epoch,uint64_t part,double rate,unsigned capacity,
    void*engine,void*program,const struct mixer_live_ops*,const struct fx_backend*);
/* UI owner can retain this borrowed controller until session destruction. */
struct mixer_control*mixer_session_control(struct mixer_session*);
/* Control owner, before attachment. Rechecked after the audio revision's
 * acquire load, so an FX clock failure cannot acknowledge a dry fallback. */
void mixer_session_health(struct mixer_session*,int(*healthy)(void*),void*);
/* Owner submits edits -> audio accepts complete revision -> owner atomically
 * updates the archive -> UI receives confirmed snapshot. tick never waits. */
void mixer_session_owner_tick(struct mixer_session*);
void mixer_session_ui_tick(struct mixer_session*);
struct mixer_live*mixer_session_live(struct mixer_session*);
/* Explicitly stop audio/native callbacks and join UI before destroy. This API
 * doesn't attach native globals automatically and cannot extend Program life. */
void mixer_session_destroy(struct mixer_session*);
/* 0 boot/pending,1 ready,-1 project changed/read-only,-2 DSP health failure.
 * Rejected edit preparation leaves the existing session and saved mix active. */
int mixer_session_status(const struct mixer_session*);
#ifdef __cplusplus
}
#endif
#endif
