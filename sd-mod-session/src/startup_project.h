#ifndef AM_STARTUP_PROJECT_H
#define AM_STARTUP_PROJECT_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
int startup_project_install(uintptr_t image);
int startup_song_export_ready(void);
/* Stock External destination: <card>/Samples/User/Export/... is writable SD. */
int startup_song_export_external(const char *path);
/* Pinned Internal exports retain their native location; External stays on SD.
 * Controllers without persistent Internal support retain the legacy SD alias. */
int startup_song_export_target(const char *path,char *out,size_t size);
/* Actual path of the stock mixdown file for the chosen storage. */
int startup_song_export_native_file(const char *path,char *out,size_t size);
int startup_project_loading(void);
/* UI reached a usable screen without a boot Load (e.g. no Temp-Project). */
void startup_project_ready(void);
/* New Project or restored identity; retain the shared temporary audio workspace. */
void startup_project_changed(void);
#ifdef __cplusplus
}
#endif
#endif
