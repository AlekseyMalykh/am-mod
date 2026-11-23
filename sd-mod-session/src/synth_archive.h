#ifndef AM_SYNTH_ARCHIVE_H
#define AM_SYNTH_ARCHIVE_H
#include "synth_binding.h"
#ifdef __cplusplus
extern "C" {
#endif
enum am_synth_archive_mode {AM_SYNTH_ARCHIVE_LEGACY,AM_SYNTH_ARCHIVE_KNOWN,AM_SYNTH_ARCHIVE_OPAQUE};
enum am_synth_archive_edit {AM_SYNTH_EDIT_OK,AM_SYNTH_EDIT_STALE,AM_SYNTH_EDIT_READONLY,AM_SYNTH_EDIT_INVALID};
/* UI/project owner only; never acquire this metadata mutex from audio. */
void am_synth_archive_init(uintptr_t image);
void am_synth_archive_reset(void);
int am_synth_archive_snapshot(struct am_synth_binding*,uint64_t*revision,unsigned*mode);
enum am_synth_archive_edit am_synth_archive_replace(const struct am_synth_binding*,uint64_t revision);
uint64_t am_synth_archive_generation(void);
int am_synth_archive_loading(void);
/* Native named optional field `amSynthV1`; stage at load, commit only on a
 * successful outer read. Native compression/file commit remain unchanged. */
void am_synth_archive_save(void*data,void*archive,void(*original)(void*,void*));
void am_synth_archive_load(void*data,void*archive,void(*original)(void*,void*));
int am_synth_archive_read(void*data,void*json,int(*original)(void*,void*));
#ifdef __cplusplus
}
#endif
#endif
