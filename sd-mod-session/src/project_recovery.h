#ifndef MPC_PROJECT_RECOVERY_H
#define MPC_PROJECT_RECOVERY_H
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
#define PROJECT_RECOVERY_BYTES 2400
/* Capture on the UI thread before starting the exit-copy worker. */
void project_recovery_capture(uintptr_t base,void *saver);
/* Native Save returned success. Record the explicit target, including Internal,
 * without changing the current sample directory or autosave destination. */
void project_recovery_saved(const char *path);
void project_recovery_begin_copy(void);
void project_recovery_end_copy(void);
const char *project_recovery_copy_payload(void);
/* Called only after the native project archive has been read successfully. */
void project_recovery_loaded(const char *payload);
void project_recovery_reset(void);
void project_recovery_tick(uintptr_t base,void *services);
const char *project_recovery_notice(void);
int project_recovery_save_external(uintptr_t image,void *screen);
int project_recovery_save_k3_external(uintptr_t image,void *screen);
int project_recovery_save_control(uintptr_t image,void *screen,unsigned cc);
void project_recovery_save_paint(uintptr_t base,void *screen,void *graphics);
int project_recovery_save_button(uintptr_t base,void *screen,void *event);
/* Bounded, mount-independent format; paths are relative to MPC-Sample. */
int project_recovery_encode(const char *name,const char *relative,char *out,size_t size);
int project_recovery_decode(const char *payload,char *name,size_t namesz,char *relative,size_t pathsz);
#ifdef __cplusplus
}
#endif
#endif
