#ifndef AM_PATTERN_TOOLS_H
#define AM_PATTERN_TOOLS_H
#include "piano_roll.h"
#ifdef __cplusplus
extern "C" {
#endif
struct pt_entry {char name[64];uint64_t id,revision;int64_t length;unsigned count,part;};
void pt_init(uintptr_t);
void pt_reset(void);
uint64_t pt_epoch(void);
/* Bind the current UI owner's operations to the project shown on screen. */
void pt_expect_epoch(uint64_t);
unsigned pt_count(void);
int pt_info(unsigned,struct pt_entry*);
/* UI-owned copied pr_notes only. No native pointer/reader enters this API.
 * mode 0 pads,1 mixer,2 selected/manual,3 empty with supplied route catalog. */
int pt_capture(const struct pr_note*,unsigned,int64_t,uint64_t,const char*,const int[128],const int[128],unsigned);
int pt_extract(unsigned,int64_t,int64_t);
int pt_variation(unsigned);
int pt_remove(unsigned);
int pt_history(int direction);
int pt_notes(unsigned,struct pr_note*,unsigned,unsigned char[128]);
int pt_note_number(unsigned,int pad);
int pt_encode(unsigned,const struct pr_note*,struct step_event*);
int pt_commit(unsigned,const struct pr_plan*);
const char*pt_error(void);
void pt_archive_save(void*,void*,void(*)(void*,void*));
void pt_archive_load(void*,void*,void(*)(void*,void*));
int pt_archive_read(void*,void*,int(*)(void*,void*));
#ifdef __cplusplus
}
#endif
#endif
