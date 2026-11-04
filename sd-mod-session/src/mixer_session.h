#ifndef AM_MIXER_SESSION_H
#define AM_MIXER_SESSION_H
#include "mixer_live.h"
#include "mixer_control.h"
#ifdef __cplusplus
extern "C" {
#endif
struct mixer_session;
/* Create and owner_tick on the native model/control owner, with valid borrowed
 * engine/Program. Native object destruction must invalidate/retire this session
 * first. No UI/paint/native model pointers are read from the audio callback. */
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
