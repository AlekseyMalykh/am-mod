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
/* Create and owner_tick on one non-audio owner. Native engine/Program pointers
 * serve as opaque identities. The automatic runtime uses the phone UI owner for
 * its AM model and DSP preparation, with native reads confined to audio. */
struct mixer_session*mixer_session_create(uint64_t epoch,uint64_t part,double rate,unsigned capacity,
    void*engine,void*program,const struct mixer_live_ops*,const struct fx_backend*);
/* UI owner can retain this borrowed controller until session destruction. */
struct mixer_control*mixer_session_control(struct mixer_session*);
/* Owner submits edits -> audio accepts complete revision -> owner atomically
 * updates the archive -> UI receives confirmed snapshot. tick never waits. */
void mixer_session_owner_tick(struct mixer_session*);
void mixer_session_ui_tick(struct mixer_session*);
struct mixer_live*mixer_session_live(struct mixer_session*);
/* Explicitly stop audio/native callbacks and join UI before destroy. This API
 * doesn't attach native globals automatically and cannot extend Program life. */
void mixer_session_destroy(struct mixer_session*);
/* 0 boot/pending,1 ready,-1 project changed/read-only,-2 publish/prepare failure. */
int mixer_session_status(const struct mixer_session*);
#ifdef __cplusplus
}
#endif
#endif
