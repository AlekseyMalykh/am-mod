#ifndef AM_MIXER_METERS_H
#define AM_MIXER_METERS_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define MIXER_METER_TRACKS 8u
#define MIXER_METER_DIRECT 8u
#define MIXER_METER_MASTER 9u
#define MIXER_METER_BUSES 10u
#define MIXER_METER_QUEUE 4u
struct mixer_meter_input {
    const float *channel[2];
    unsigned channels; /* 1/2; 0 means this tap is unavailable (not silence) */
    unsigned clear; /* Native clear flag: ignore stale backing-store PCM. */
};
struct mixer_meter_snapshot {
    uint64_t epoch,frame_end;
    uint32_t valid_mask,fault_mask,clip_mask;
    float peak[MIXER_METER_BUSES][2]; /* linear PCM peak, not fader gain */
};
struct mixer_meters;
/* Create/destroy off RT. Destroy only after producer and consumer are joined.
 * Exactly one audio producer, one UI consumer. No global or UI/native pointers. */
struct mixer_meters *mixer_meters_create(double rate);
void mixer_meters_destroy(struct mixer_meters*);
/* Audio-owner call with aligned bus blocks and monotonically increasing
 * nonzero project/session epoch. Borrowed PCM is scanned now, never retained.
 * No allocation, lock, IO, retry loop or reader wait. -1=invalid invocation,
 * 0=queue full (telemetry still accumulated), 1=snapshot published. */
int mixer_meters_push(struct mixer_meters*,uint64_t epoch,unsigned frames,
                      const struct mixer_meter_input[MIXER_METER_BUSES]);
/* UI owner: drain at most four snapshots, return latest matching epoch.
 * Missing epoch returns 0 and leaves out untouched. UI must invalidate its
 * cached display on epoch change and expire stale frame counters itself. */
int mixer_meters_take(struct mixer_meters*,uint64_t epoch,struct mixer_meter_snapshot*out);
float mixer_meter_dbfs(float linear_peak);
size_t mixer_meters_bytes(void);
#ifdef __cplusplus
}
#endif
#endif
