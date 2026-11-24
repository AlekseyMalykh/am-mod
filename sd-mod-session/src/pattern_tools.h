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
/* Monotonic value receipt across edits/history/load; not a native lease. */
uint64_t pt_serial(void);
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
int pt_pad_number(unsigned,int note);
int pt_encode(unsigned,const struct pr_note*,struct step_event*);
int pt_commit(unsigned,const struct pr_plan*);
struct pt_arrangement {uint64_t revision;int64_t length;unsigned tracks,clips,read_only;};
struct pt_arr_track {uint64_t id;char name[32];unsigned muted,solo;};
struct pt_arr_clip {uint64_t id,track,pattern;int64_t start,length,offset;unsigned muted;};
int pt_arr_info(struct pt_arrangement*);
int pt_arr_track_at(unsigned,struct pt_arr_track*);
int pt_arr_clip_at(unsigned,struct pt_arr_clip*);
int pt_arr_clip_by_id(uint64_t,struct pt_arr_clip*);
int pt_pattern_index(uint64_t);
int pt_arr_add_track(const char*,uint64_t*);
int pt_arr_add_clip(uint64_t,unsigned,int64_t,int64_t,uint64_t*);
int pt_arr_copy(uint64_t,uint64_t,int64_t,uint64_t*);
int pt_arr_paste(const struct pt_arr_clip*,uint64_t,int64_t,uint64_t*);
int pt_arr_move(uint64_t,uint64_t,int64_t);
int pt_arr_resize(uint64_t,int64_t);
int pt_arr_cut(uint64_t,int64_t,uint64_t*);
int pt_arr_join(uint64_t,uint64_t);
int pt_arr_remove(uint64_t);
int pt_arr_track_state(uint64_t,int,int);
int pt_arr_length(int64_t);
/* One Store/history transaction. track=0 creates a track; template=0 makes
 * an explicitly unassigned draft. Other templates copy only owned routes. */
int pt_arr_new_pattern(uint64_t track,uint64_t route_template,int64_t start,int64_t length,uint64_t*clip);
int pt_arr_variation(uint64_t clip);
int pt_arr_edit_clip(uint64_t clip,uint64_t track,int64_t start,int64_t length,uint64_t expected_revision);
int pt_pattern_assigned(unsigned index);
/* Snapshot must match the displayed complete Pattern, including duplicates.
 * copy=0 removes selected events from only this clip's new remainder.
 * copy=1 leaves the original clip intact. One history transaction. */
int pt_arr_separate(uint64_t clip,uint64_t pattern,uint64_t serial,
    const struct pr_note*,unsigned count,int copy,uint64_t*result_clip);
const char*pt_error(void);
void pt_archive_save(void*,void*,void(*)(void*,void*));
void pt_archive_load(void*,void*,void(*)(void*,void*));
int pt_archive_read(void*,void*,int(*)(void*,void*));
#ifdef __cplusplus
}
#include "arrangement.h"
#include "arrangement_native_plan.h"
namespace am_pattern {
int pt_prepare_song(PreparedArrangement&);
bool pt_prepared_current(const PreparedArrangement&);
int pt_prepare_native_plan(const NativeDestination&,NativeArrangementPlan&);
bool pt_native_plan_current(const NativeArrangementPlan&,const NativeDestination&);
}
#endif
#endif
