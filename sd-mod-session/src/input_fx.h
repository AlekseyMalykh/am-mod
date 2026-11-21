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
/* Input routing: 0 Clean (source straight to pads), 1 Input track (FX + fader), 2 Input track with the synth as its input. */
unsigned input_fx_mode(void);
void input_fx_set_mode(unsigned mode);
/* Linear peak of the selected input (L/R), for the Mixer Input strip. */
void input_fx_peak(float*left,float*right);
/* Tuner: detected pitch in Hz of the selected input; returns 0 when there is none. Polling keeps the detector running. */
int input_fx_pitch(float*hz);
#ifdef __cplusplus
}
#endif
#endif
