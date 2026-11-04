#ifndef AM_MIXER_RUNTIME_H
#define AM_MIXER_RUNTIME_H
#ifdef __cplusplus
extern "C" {
#endif
/* The phone UI Timer owns discovery, preparation, edits and retirement.
 * The native render publishes opaque identities and owns Engine/Program/clock
 * reads. Application FX formats are borrowed on the UI owner while alive. */
void mixer_runtime_ui_tick(void);
void mixer_runtime_ui_stop(void);
#ifdef __cplusplus
}
#endif
#endif
