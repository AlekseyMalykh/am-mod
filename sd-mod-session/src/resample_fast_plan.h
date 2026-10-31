#ifndef MPC_RESAMPLE_FAST_PLAN_H
#define MPC_RESAMPLE_FAST_PLAN_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Planning only: no project, sequence, sample, file or audio-engine mutation.
 * processed_frames is a separate duration estimate after pitch/warp. A raw
 * source floor must never be replaced by the current sequence's length. */
struct resample_fast_plan {
    uint64_t minimum_frames,minimum_ticks,render_bars,render_ticks,render_frames;
};
int resample_fast_plan_make(uint64_t source_frames,unsigned source_rate,
    uint64_t processed_frames,unsigned processed_rate,unsigned output_rate,
    unsigned bpm_milli,unsigned ppqn,unsigned beats_per_bar,
    uint64_t native_tick_limit,struct resample_fast_plan*);
enum { RFT_MORE,RFT_QUIET,RFT_NEEDS_MORE_ROOM,RFT_INVALID };
struct resample_fast_tail {
    uint64_t minimum,hold,budget,scanned,quiet;
    float threshold;
    unsigned state;
};
int resample_fast_tail_init(struct resample_fast_tail*,uint64_t minimum,
    uint64_t quiet_hold,uint64_t budget,float threshold);
/* Offline worker only. Silence must persist AFTER the complete source floor.
 * Exhausting the render budget is not successful tail completion. */
unsigned resample_fast_tail_scan(struct resample_fast_tail*,const float*left,
    const float*right,uint64_t frames);
#ifdef __cplusplus
}
#endif
#endif
