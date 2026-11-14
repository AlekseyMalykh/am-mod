#ifndef AM_INPUT_FX_H
#define AM_INPUT_FX_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* UI owner prepares and retires complete independent processors. */
void input_fx_ui_tick(uintptr_t image);
/* The common selected-input render callback, before native monitor/record.
 * Preserves native channel count; stereo effects are downmixed for mono. */
int input_fx_audio(void *buffer,unsigned rate);
struct mixer_fx_monitor;
int input_fx_monitor(unsigned slot,struct mixer_fx_monitor *out);
const char *input_fx_status(void);
void input_fx_flush(void);
#ifdef __cplusplus
}
#endif
#endif
