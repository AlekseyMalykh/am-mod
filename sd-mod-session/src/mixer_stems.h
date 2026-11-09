#ifndef AM_MIXER_STEMS_H
#define AM_MIXER_STEMS_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Pre-main: hook the pinned native writer close. */
int mixer_stems_install(uintptr_t image);
/* True only if the binary hook was installed against the expected firmware. */
int mixer_stems_available(void);
void mixer_stems_disarm(void);
/* UI: arm the next confirmed Song export to write Track and Master stems. */
void mixer_stems_arm(int with_fx);
int mixer_stems_armed(void);
void mixer_stems_song_frame(void *context,void *writer,const char *path,unsigned rate);
/* Offline export worker (engine entry from 18b556c/18b98bc): 1 = rendered
 * through the offline Mixer, 0 = caller renders natively. program_vptr
 * identifies the kit Program class in the worker copy. */
int mixer_stems_offline_block(void*engine,void*clock,unsigned frames,uintptr_t program_vptr);
const char*mixer_stems_last_folder(void);
unsigned mixer_stems_finished(void);
unsigned mixer_stems_failed(void);
#ifdef __cplusplus
}
#endif
#endif
