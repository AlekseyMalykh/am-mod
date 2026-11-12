#ifndef AM_MIXER_LIVE_H
#define AM_MIXER_LIVE_H
#include "mixer_route_native.h"
#ifdef __cplusplus
extern "C" {
#endif
struct mixer_live;
struct mixer_live_ops {
    struct mixer_route_ops route;
    void(*engine)(void*,void*,unsigned);
    void*(*final_output)(void*);
};
/* One engine audio owner / one control producer / one UI meter consumer.
 * Binding pointers are identity comparisons only; native owns their lifetime.
 * Destroy after stopped/joined audio or native_take_retired() ownership return;
 * the UI consumer must also stop accessing the retired session. */
struct mixer_live*mixer_live_prepare(const struct mixer_project*,uint64_t part,double rate,
    unsigned capacity,uint64_t epoch,void*engine,void*program,const struct mixer_live_ops*,
    const struct fx_backend*,enum mixer_bus_result*);
void mixer_live_destroy(struct mixer_live*);
/* Offline owner only (export worker): bind engine/Program identities, and
 * read a Track/Direct bus of the last applied block. */
int mixer_live_render_offline(struct mixer_live*,void*,void*,unsigned,uintptr_t);
void mixer_live_rebind(struct mixer_live*,void*engine,void*program);
int mixer_live_bus_view(const struct mixer_live*,unsigned bus,struct mixer_pcm_view*);
/* Control producer, complete absolute snapshot; OK queues ownership internally.
 * Busy returns STATE, so caller can coalesce edits and retry from latest model. */
enum mixer_bus_result mixer_live_flush_fx(struct mixer_live*,const struct mixer_project*,uint64_t part);
enum mixer_bus_result mixer_live_publish(struct mixer_live*,const struct mixer_project*,uint64_t part,uint64_t revision);
void mixer_live_collect(struct mixer_live*);
uint64_t mixer_live_applied_revision(const struct mixer_live*);
int mixer_live_fx_monitor(struct mixer_live*,unsigned channel,unsigned slot,struct mixer_fx_monitor*);
int mixer_live_take_meter(struct mixer_live*,struct mixer_meter_snapshot*);
/* Native dispatch: engine executes exactly once, Program capture only inside
 * this engine scope, global Master FX/gain after native downstream mixing. */
int mixer_live_render(struct mixer_live*,void*engine,void*clock,unsigned frames);
void mixer_live_program(const struct mixer_route_ops*,void*program,void*clock,unsigned frames);
/* Diagnostic counters: engine blocks, blocks with Program matched, Program calls,
 * scoped calls, matched calls, calls from another thread during an engine block,
 * scoped calls for a different Program object. */
void mixer_live_diag(uint64_t out[7]);
/* Diagnostic: bound Program and up to four other Program objects seen in scope. */
void mixer_live_diag_objects(const struct mixer_live*,uintptr_t out[5]);
/* Experimental pinned installer. Call pre-main/stopped only. No automatic
 * binding or activation. One control owner can attach/detach while audio runs;
 * ownership transfers on success, and retirement never waits on audio. */
int mixer_live_native_install(uintptr_t image);
const struct mixer_live_ops*mixer_live_native_ops(void);
uintptr_t mixer_live_native_image(void);
int mixer_live_native_attach(struct mixer_live*);
struct mixer_live*mixer_live_native_take_retired(void);
/* A copied observation from the pinned live render callsite. Identities are
 * opaque off audio. They must never be dereferenced by the UI/control owner. */
struct mixer_native_binding {
    uint64_t epoch;
    void *engine, *program;
    double rate;
    unsigned capacity;
    unsigned clock_valid;
    unsigned char position[88]; /* Actual native clock, copied on audio owner. */
};
/*1=snapshot,0=unavailable,-1=busy. Busy preserves the previous UI binding. */
int mixer_live_native_binding(struct mixer_native_binding*);
/* Audio only, inside the matching real live caller. No UI reads or fabricated
 * stopped/offline clock. Block identity changes even on loop/seek at same frame. */
int mixer_live_native_position(uint64_t epoch,void *position88,uint64_t *block);
/* Revoke immediately on Load/New. The current callback may complete, and
 * take_retired still governs destruction. New observations use a new epoch. */
void mixer_live_native_invalidate(void);
int mixer_live_native_attach_binding(struct mixer_live*,const struct mixer_native_binding*);
#ifdef __cplusplus
}
#endif
#endif
