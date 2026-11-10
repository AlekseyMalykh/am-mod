#ifndef AM_MIXER_BUSES_H
#define AM_MIXER_BUSES_H
#include "mixer_model.h"
#include "mixer_meters.h"
#ifdef __cplusplus
extern "C" {
#endif
enum mixer_bus_result { MIXER_BUS_OK, MIXER_BUS_INVALID, MIXER_BUS_RESOURCE,
    MIXER_BUS_FX_ERROR, MIXER_BUS_UNSUPPORTED, MIXER_BUS_STATE, MIXER_BUS_AUDIO_ERROR };
struct mixer_buses;
struct mixer_bus_update;
#define MIXER_FX_MONITOR_FRAMES 4096u
struct mixer_fx_monitor {
    unsigned channel,slot;
    float input_peak,output_peak,key_peak,gain_reduction_db;
    float input_pcm[MIXER_FX_MONITOR_FRAMES];
    float pcm[MIXER_FX_MONITOR_FRAMES];
};
/* Audio owner only. The final Master graph may borrow the Track graph's
 * pre-FX detector levels for this block. */
void mixer_buses_key_source(struct mixer_buses*,const struct mixer_buses*);
void mixer_buses_monitor(struct mixer_buses*,unsigned channel,unsigned slot);
int mixer_buses_take_monitor(struct mixer_buses*,struct mixer_fx_monitor*);
struct mixer_pcm_view {const float *channel[2];unsigned frames;};
/* Prepare immutable routing/mix/FX snapshot off RT. part_id chooses an
 * optional Scene. No native source, project, sequencer or UI is changed.
 * Track sends feed the destination before its FX. Independent FX
 * instances belong to this graph; chain bypass leaves native config intact. */
struct mixer_buses *mixer_buses_prepare(const struct mixer_project*,uint64_t part_id,
    double rate,unsigned max_frames,const struct fx_backend*,enum mixer_bus_result*error);
/* Stop/join audio and readers before destroy. Backend destructors run here. */
void mixer_buses_destroy(struct mixer_buses*);
/* Single audio owner: begin -> any number of accumulate -> finish. Input
 * is post-individual-Pad-processing PCM; mono duplicates to L/R. Sources may
 * overlap, including several voices of one Pad. Inputs are never mutated.
 * Adapter must suppress the old summing path for exactly those sources. */
enum mixer_bus_result mixer_buses_begin(struct mixer_buses*,unsigned frames);
enum mixer_bus_result mixer_buses_accumulate(struct mixer_buses*,unsigned pad,
    const float *left,const float *right,unsigned channels,unsigned clear);
enum mixer_bus_result mixer_buses_finish(struct mixer_buses*,struct mixer_meters*,uint64_t epoch);
/* Read on the same owner before next begin/destroy. Index 0..7 Track,
 * 8 Master Direct (post Solo), 9 final Master. No unfinished/error output. */
int mixer_buses_view(const struct mixer_buses*,unsigned bus,struct mixer_pcm_view*);
uint32_t mixer_buses_return_peaks(const struct mixer_buses*,float out[MIXER_TRACKS]);
size_t mixer_buses_pcm_bytes(const struct mixer_buses*);
/* One control producer prepares complete updates off RT and publishes at most
 * one pending update. Ownership transfers only on OK. Revisions increase;
 * publish never changes audio immediately. No native/UI pointers in packets. */
struct mixer_bus_update *mixer_buses_update_prepare(const struct mixer_project*,uint64_t part_id,
    double rate,uint64_t revision,const struct fx_backend*,enum mixer_bus_result*);
/* Same single producer, using effective FX configs from its last successful
 * publication (or initial preparation). Unchanged chains need no new factory
 * instance. Publication order must remain the same as this configuration base. */
struct mixer_bus_update *mixer_buses_update_prepare_delta(const struct mixer_project*,uint64_t part_id,
    double rate,uint64_t revision,const struct fx_backend*,const struct fx_chain_config previous[9],enum mixer_bus_result*);
void mixer_buses_update_destroy(struct mixer_bus_update*);
enum mixer_bus_result mixer_buses_publish(struct mixer_buses*,struct mixer_bus_update*);
/* Control owner: frees retired packets/processors, never called from audio. */
void mixer_buses_collect(struct mixer_buses*);
uint64_t mixer_buses_applied_revision(const struct mixer_buses*);
int mixer_buses_ready_audio(const struct mixer_buses*);
/* Audio owner: an update starts only at begin(). Gains, pan, gates and source
 * routing ramp for5ms; changed FX crossfade for5ms. Unchanged FX retain state.
 * Applied revision advances only after a successful completed transition. */
#ifdef __cplusplus
}
#endif
#endif
