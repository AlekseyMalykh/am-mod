#ifndef AM_INTERNAL_STORAGE_H
#define AM_INTERNAL_STORAGE_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/* True only with the new controller's pinned persistent user directories. */
int internal_storage_enabled(void);
int internal_storage_project_path(const char *path);
int internal_storage_sample_path(const char *path);
#ifdef __cplusplus
}
#endif
#endif
