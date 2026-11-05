#ifndef MPC_REAR_AUDIO_H
#define MPC_REAR_AUDIO_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
enum { REAR_MONITOR, REAR_LEVEL, REAR_PAN, REAR_ENABLED, REAR_MODE, REAR_TARGET };
enum { REAR_STEREO, REAR_LEFT, REAR_RIGHT, REAR_DUAL };
enum { REAR_IDLE, REAR_PREPARING, REAR_WAITING, REAR_RECORDING, REAR_FINISHING, REAR_READY, REAR_ERROR };
uint64_t rear_settings_snapshot(void);
int rear_settings_valid(uint64_t value);
void rear_settings_restore(uint64_t value);
void rear_settings_reset(void);
unsigned rear_setting(unsigned channel, unsigned field);
int rear_set_setting(unsigned channel, unsigned field, unsigned value);
/* Realtime owner only. Input is raw Rear L/R; output is the monitor bus.
 * No file work, allocation, model calls or locks occur in this callback. */
void rear_audio_process(const float *left, const float *right, float *out_left,
                        float *out_right, unsigned frames, unsigned rate, int monitor);
void rear_audio_meters(float levels[2]);
/* Called off the audio thread. The parent directory is the native temporary
 * project audio directory. Files remain owned by the project after success. */
int rear_record_begin(const char *directory, unsigned channels, unsigned rate,
                      float threshold, uint64_t limit_frames);
void rear_record_stop(void);
void rear_record_cancel(void);
unsigned rear_record_state(void);
uint64_t rear_record_frames(void);
const char *rear_record_error(void);
const char *rear_record_file(unsigned channel);
void rear_record_release(int assigned);
#ifdef __cplusplus
}
#endif
#endif
