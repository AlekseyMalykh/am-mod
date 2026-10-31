#ifndef MPC_RESAMPLE_RUNTIME_H
#define MPC_RESAMPLE_RUNTIME_H
#include "resample_panel.h"
#include "resample_context.h"
#ifdef __cplusplus
extern "C" {
#endif
struct resample_runtime;
/* UI host supplies lifecycle tracker and the live CopyContext queue handle.
 * Creation/open only retain the tracker; all model capture, native factory
 * calls and worker teardown execute later on existing owner queue.
 * Host must stop new UI callers before destroy. Context lifecycle/queue must
 * remain valid until owner close acknowledges. No detached background task. */
struct resample_runtime *resample_runtime_create(uintptr_t base,void *copy_context,
    struct resample_context*);
/* UI owner APIs. open requests async owner capture, no model access on UI. */
int resample_runtime_open(struct resample_runtime*);
void resample_runtime_tick(struct resample_runtime*);
int resample_runtime_button(struct resample_runtime*,unsigned code,int down);
int resample_runtime_knob(struct resample_runtime*,unsigned knob,int delta);
void resample_runtime_close(struct resample_runtime*);
/*1=closed and owner cleanup acknowledged,0=still in use. */
int resample_runtime_closed(struct resample_runtime*);
int resample_runtime_destroy(struct resample_runtime*);
/* UI copies state; never retain internal flow/worker pointers across calls. */
enum resample_page resample_runtime_page(struct resample_runtime*);
void resample_runtime_paint(struct resample_runtime*,const struct resample_painter*);
/* First limited host supports EQ; edits are UI config only until Render. */
int resample_runtime_set_fx(struct resample_runtime*,const struct fx_chain_config*);
int resample_runtime_applied(struct resample_runtime*);
/* UI polls these before choosing native handoff/editor. Seq is the existing
 * device record/resample path; callback must be supplied by actual UI host. */
int resample_runtime_take_sequence(struct resample_runtime*);
int resample_runtime_take_fx_request(struct resample_runtime*,unsigned *slot);
/* Physical MIDI-style controls: K1..3=10..12 absolute0..127, encoder64
 * signed-relative. Main volume13 is deliberately not owned. */
int resample_runtime_control(struct resample_runtime*,unsigned cc,unsigned value);
int resample_runtime_fx_snapshot(struct resample_runtime*,struct fx_chain_config*);
int resample_runtime_select_fx(struct resample_runtime*,unsigned slot);
unsigned resample_runtime_fx_slot(struct resample_runtime*);
double resample_runtime_rate(struct resample_runtime*);
/* UI-owner snapshot, including the bounded native RAM-copy stage. No worker
 * model pointers escape; caller may use it to draw progress while closing. */
struct resample_runtime_progress {
    enum resample_page page;
    uint64_t source_done,source_total,done,total,ram_done,ram_total;
};
void resample_runtime_progress(struct resample_runtime*,struct resample_runtime_progress*);
#ifdef __cplusplus
}
#endif
#endif
