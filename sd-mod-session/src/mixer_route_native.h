#ifndef AM_MIXER_ROUTE_NATIVE_H
#define AM_MIXER_ROUTE_NATIVE_H
#include "mixer_buses.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Experimental, explicitly scoped adapter for the pinned 1.3.0.12
 * DrumProgram separate-buffer renderer. This module installs NO hooks.
 * Only an isolated harness currently connects the two call sites. */
struct mixer_route_ops {
    void *(*buffer)(void*,unsigned long);
    unsigned long (*buffer_count)(void*);
    void *(*resolve)(void*,const void*);
    int (*sum)(void*,void*,void*,const void*,unsigned);
    void (*stage)(void*,void*,unsigned,unsigned);
    void (*render)(void*,void*,unsigned);
};
enum mixer_route_result { MIXER_ROUTE_APPLIED, MIXER_ROUTE_PASSTHROUGH,
    MIXER_ROUTE_RECOVERED, MIXER_ROUTE_INVALID };
struct mixer_route_native;
/* Prepare/destroy off RT, with audio and readers stopped. Each instance and
 * its graph have one audio owner. Frozen snapshot; no live reconfiguration. */
struct mixer_route_native *mixer_route_prepare(const struct mixer_project*,
    uint64_t part_id,double rate,unsigned capacity,const struct fx_backend*,
    const struct mixer_route_ops*,enum mixer_bus_result*);
void mixer_route_destroy(struct mixer_route_native*);
/* Native object ownership/pointers must be valid for the entire callback.
 * Unsupported/shared/physical-output blocks run entirely through native code.
 * Failure after capture restores the native shadow sum and disables this
 * adapter until it is destroyed/reprepared. No allocations, locks or IO. */
unsigned mixer_route_last_reject(void); /* diagnostic, 0 = last block captured */
unsigned mixer_route_last_recover(void); /* diagnostic, source line of the last recovery */
enum mixer_route_result mixer_route_render(struct mixer_route_native*,
    void *program,void *clock,unsigned frames);
/* Only these specific call sites may use these dispatchers:
 * 286bf44 -> sum; 286bfa4 -> stage1. All unrelated calls forward through ops.
 * ops must remain stable; do not intercept the sum/stage function entries. */
int mixer_route_sum(const struct mixer_route_ops*,void *instrument,void *source,
    void *destination,const void *route,unsigned frames);
void mixer_route_stage(const struct mixer_route_ops*,void *program_processor,
    void *buffer,unsigned frames,unsigned stage);
/* Same-owner diagnostic views only after APPLIED. The Master view is before
 * native program FX/gain, NOT a final output tap. Use the explicit Track/Direct
 * meter publication below; do not feed this diagnostic Master to the UI. */
int mixer_route_view(const struct mixer_route_native*,unsigned bus,struct mixer_pcm_view*);
/* Audio owner, once after each render and before the next one. Publishes real
 * post-gate Track0..7/Direct PCM. Global Master is UNAVAILABLE: the core Master
 * precedes native Program/general mixing and cannot represent final output.
 * Passthrough/recovered blocks publish unavailable taps, never stale peaks.
 * Same return convention as mixer_meters_push; duplicate block push is -1. */
int mixer_route_meters_push(struct mixer_route_native*,struct mixer_meters*,uint64_t epoch);
/* Off RT, before rendering: install an audio-only block-boundary callback.
 * It runs after complete preflight and before graph begin, with no allocation. */
void mixer_route_before_block(struct mixer_route_native*,void(*)(void*),void*);
/* Borrowed graph handle only for control publish/collect/revision or audio
 * readiness; lifetime is the adapter's, never destroy it separately. */
struct mixer_buses *mixer_route_buses(struct mixer_route_native*);
#ifdef __cplusplus
}
#endif
#endif
