#ifndef MPC_WORKFLOW_UI_H
#define MPC_WORKFLOW_UI_H
#include <stdint.h>
void workflow_tail_tick(int available);
void workflow_tail_preview(unsigned pad,int chop);
void workflow_init(uintptr_t base);
void workflow_registry_bind(void *services);
void workflow_files_constructed(void *files);
void *workflow_project_services(void);
int workflow_jobs_busy(void); /* UI thread; protects an in-flight copy/mono job. */
void workflow_master_volume_bind(void *parameter,void *control);
float workflow_main_output_gain(void);
int workflow_preview_prepare(void *browsing,int mode);
/* Native SampleFileAudition worker payload, before its own private-program
 * play routine. Returns0 to cancel a queued stale play; caller releases
 * the native lifetime reference instead of entering that worker. */
int workflow_preview_worker(void *payload);
void workflow_preview_played(void *payload);
int workflow_install(void (*files)(void*,void*),void (*copy)(void*,void*));
void workflow_files_paint(void*,void*);
void workflow_copy_paint(void*,void*);
void workflow_sample_paint(void*,void*);
int workflow_mono_open(void *sample_root);
void workflow_destroyed(void*);
void workflow_copy_context(void*,void*);
void workflow_copy_context_dead(void*);
int workflow_event_preview(void *program,const void *event,unsigned key,unsigned mode);
void workflow_event_preview_stop(int cancel_pending);
unsigned workflow_event_preview_pending(void);
int workflow_tick(void);
int workflow_startup_loading(void); /* UI-only; false while a native modal is active. */
int workflow_active(void);
/* Atomic ownership:0 off,1 sample browser,2 native Copy Pad. RGB must not inspect Components. */
int workflow_native_pad_colours(void);
int workflow_native_occupied(void); /* atomic16-bit mask, -1 unavailable */
int workflow_native_selected(void); /* UI-published absolute browser destination, -1 unavailable */
/* UI thread: actual browser/Copy/Batch/Mono ownership, excluding the
 * ordinary Sample host observed for the optional Chop actions. */
int workflow_blocks_sample_controls(void);
int workflow_owns(unsigned kind,unsigned code);
uint64_t workflow_ticket(void);
int workflow_control(void*,uint64_t);
int workflow_button(void*,uint64_t);
void workflow_pad(unsigned,int,uint64_t,uint64_t);
int workflow_light(unsigned,uint64_t*);
uint64_t workflow_palette_generation(void);
void workflow_fault(void);
#endif
