#ifndef MPC_WORKFLOW_UI_H
#define MPC_WORKFLOW_UI_H
#include <stdint.h>
void workflow_init(uintptr_t base);
void workflow_registry_bind(void *services);
void workflow_files_constructed(void *files);
void *workflow_project_services(void);
int workflow_jobs_busy(void); /* UI thread; protects an in-flight copy/mono job. */
void workflow_master_volume_bind(void *parameter,void *control);
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
int workflow_active(void);
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
