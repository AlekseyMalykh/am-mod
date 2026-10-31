#ifndef MPC_MUSIC_ARCHIVE_H
#define MPC_MUSIC_ARCHIVE_H
#include <stdint.h>
void music_archive_init(uintptr_t base);
void music_archive_save(void *data,void *archive,void (*original)(void*,void*));
void music_archive_load(void *data,void *archive,void (*original)(void*,void*));
int music_archive_read(void *data,void *json,int (*original)(void*,void*));
void music_archive_key(void *data,void *archive,const void *pair,void (*original)(void*,void*,const void*));
int music_archive_status(void);
unsigned music_archive_saved_count(void);
#endif
