#ifndef AM_MIDI_FILE_NATIVE_H
#define AM_MIDI_FILE_NATIVE_H
#include "midi_file.h"
int am_midi_file_root(char *path,size_t size);
int am_midi_file_load(const char *path,struct am_smf*,char error[96]);
void *am_midi_file_project(uintptr_t image);
/* UI owner only: snapshot borrowed native objects before writing any file. */
int am_midi_file_snapshot(uintptr_t image,void *project,void *things,void *sequence,
 void *song,struct am_smf*,char error[96]);
int am_midi_file_export(uintptr_t image,void *project,void *things,void *sequence,
 void *song,char path[512],char error[96]);
#endif
