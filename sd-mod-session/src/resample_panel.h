#ifndef MPC_RESAMPLE_PANEL_H
#define MPC_RESAMPLE_PANEL_H
#include "resample_worker.h"
#include "resample_queue.h"
#include "resample_view.h"
#ifdef __cplusplus
extern "C" {
#endif
/* UI owner bridge to already initialized worker/native queue. Borrows both;
 * actual device captures/constructs them on its owner before opening panel.
 * No DSP, model reads or model writes on UI. External native hooks must
 * honor button return values, consume paired pad/knob events while active,
 * forward Shift BOTH edges, and keep Play/Stop on their stock path. */
struct resample_panel {
    struct resample_flow flow;
    struct resample_worker *worker;
    struct resample_queue *queue;
    uint64_t after;
    unsigned sequence_requested,fx_request,fx_slot;
};
void resample_panel_init(struct resample_panel*,struct resample_worker*,struct resample_queue*);
int resample_panel_open(struct resample_panel*,unsigned pad,const char *sample,
    const char *sequence,int available,const uint64_t occupied[2]);
/* UI timer, including after Cancel until queue drains; retries queue-full
 * posts without duplicating copied commands. */
void resample_panel_tick(struct resample_panel*);
int resample_panel_button(struct resample_panel*,unsigned code,int down);
int resample_panel_knob(struct resample_panel*,unsigned knob,int delta);
void resample_panel_close(struct resample_panel*);
/* Host may detach only after closed + all queued closures/commands drained.
 * Owner still destroys worker/adapter on its queue; this does not free them. */
int resample_panel_can_detach(struct resample_panel*);
void resample_panel_paint(const struct resample_panel*,const struct resample_painter*);
#ifdef __cplusplus
}
#endif
#endif
