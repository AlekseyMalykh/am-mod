#ifndef AM_MIXER_ARCHIVE_H
#define AM_MIXER_ARCHIVE_H
#include "mixer_model.h"
#ifdef __cplusplus
extern "C" {
#endif
enum mixer_archive_mode { MIXER_ARCHIVE_LEGACY, MIXER_ARCHIVE_KNOWN, MIXER_ARCHIVE_OPAQUE };
enum mixer_archive_edit { MIXER_EDIT_OK, MIXER_EDIT_STALE, MIXER_EDIT_READONLY, MIXER_EDIT_INVALID };
/* Optional experimental archive owner, never called from the audio thread.
 * Snapshots contain model defaults while opaque data is preserved read-only.
 * revision is an edit ticket: Load/New/another successful edit invalidates it. */
void mixer_archive_init(uintptr_t base);
void mixer_archive_reset(void);
/* Source identity changes for native Load/New, independently of mix edits.
 * A read/load scope remains busy through exceptions and nested callbacks. */
uint64_t mixer_archive_generation(void);
int mixer_archive_loading(void);
int mixer_archive_snapshot(struct mixer_project*,uint64_t *revision,unsigned *mode);
enum mixer_archive_edit mixer_archive_replace(const struct mixer_project*,uint64_t revision);
uint64_t mixer_archive_saved_count(void);
/* C ABI callbacks invoked from C++ native serialization. Allocation/native
 * serialization exceptions propagate to the native caller; no silent skip.
 * load() only stages data within the current read() transaction; it never
 * replaces the project on its own. read() publishes only on native success.
 * Hooks are gated by MPC_MIXER_ARCHIVE and disabled in stable builds. */
void mixer_archive_save(void *data,void *archive,void (*original)(void*,void*));
void mixer_archive_load(void *data,void *archive,void (*original)(void*,void*));
int mixer_archive_read(void *data,void *json,int (*original)(void*,void*));
#ifdef __cplusplus
}
#endif
#endif
