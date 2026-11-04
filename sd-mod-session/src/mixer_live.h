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
/* Control producer, complete absolute snapshot; OK queues ownership internally.
 * Busy returns STATE, so caller can coalesce edits and retry from latest model. */
enum mixer_bus_result mixer_live_publish(struct mixer_live*,const struct mixer_project*,uint64_t part,uint64_t revision);
void mixer_live_collect(struct mixer_live*);
uint64_t mixer_live_applied_revision(const struct mixer_live*);
int mixer_live_take_meter(struct mixer_live*,struct mixer_meter_snapshot*);
/* Native dispatch: engine executes exactly once, Program capture only inside
 * this engine scope, global Master FX/gain after native downstream mixing. */
int mixer_live_render(struct mixer_live*,void*engine,void*clock,unsigned frames);
void mixer_live_program(const struct mixer_route_ops*,void*program,void*clock,unsigned frames);
/* Experimental pinned installer. Call pre-main/stopped only. No automatic
 * binding or activation. One control owner can attach/detach while audio runs;
 * ownership transfers on success, and retirement never waits on audio. */
int mixer_live_native_install(uintptr_t image);
const struct mixer_live_ops*mixer_live_native_ops(void);
int mixer_live_native_attach(struct mixer_live*);
struct mixer_live*mixer_live_native_take_retired(void);
#ifdef __cplusplus
}
#endif
#endif
