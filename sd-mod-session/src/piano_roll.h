#ifndef AM_PIANO_ROLL_H
#define AM_PIANO_ROLL_H
#include "step_native.h"
#include <stddef.h>
#include <stdint.h>
#define PR_MAX_EVENTS 8192u
enum pr_layer { PR_PADS, PR_TUNE, PR_CHOP };
enum pr_snap { PR_SNAP_START, PR_SNAP_END, PR_SNAP_BOTH };
struct pr_note {struct step_event event;int sound,tune,slice;unsigned flags,selected;};
struct pr_plan {
 struct step_event *removed,*added;
 unsigned removed_count,added_count,capacity;
 uint64_t revision;
 const char *title;
};
struct pr_ops {
 void *user;
 /* Encode only the changed Sound/Tune/Slice; preserve all other native bytes. */
 int (*encode)(void*,const struct pr_note*,struct step_event*);
 /* One native history command. Failure must not mutate the sequence. */
 int (*commit)(void*,const struct pr_plan*);
};
struct piano_roll {
 struct pr_note *notes,*clipboard;
 unsigned count,clip_count,capacity;
 unsigned pending_commit,selection_after_count;
 uint64_t revision;
 int64_t length,cursor_tick,left,span,clip_tick;
 int cursor_row,top,rows_visible,layer,sound,grid,zoom,row_height,range,chop_count;
 int64_t anchor_tick;
 int anchor_row,rows[241],row_count,clip_row,clip_layer;
 int layout_low,layout_high,follow,note_focus,focused;
 unsigned char loaded[128];
 unsigned char pinned[128];
 unsigned char preview_rows[241];
 int preview_valid,preview_low,preview_high,preview_anchor_row;
 int64_t preview_start,preview_end,preview_anchor_tick;
 struct pr_plan plan;
 struct pr_ops ops;
 char error[96];
};
int pr_init(struct piano_roll*,struct pr_ops);
void pr_destroy(struct piano_roll*);
void pr_reset(struct piano_roll*);
void pr_reset_view(struct piano_roll*); /* Retain clipboard within one project. */
/* UI-owned value snapshot; caller supplies complete data or refuses the view. */
int pr_load(struct piano_roll*,const struct pr_note*,unsigned,int64_t,uint64_t,const unsigned char[128]);
void pr_rows(struct piano_roll*);
void pr_reveal_edit(struct piano_roll*);
int pr_note_row(const struct piano_roll*,const struct pr_note*);
int pr_row_index(const struct piano_roll*,int value);
void pr_view_layer(struct piano_roll*,int layer,int sound);
void pr_cursor(struct piano_roll*,int64_t,int row);
void pr_select(struct piano_roll*,int additive);
void pr_range(struct piano_roll*);
void pr_range_apply(struct piano_roll*);
unsigned pr_selected(const struct piano_roll*);
void pr_select_all(struct piano_roll*);
int64_t pr_grid_ticks(int);
int pr_copy(struct piano_roll*);
int pr_delete(struct piano_roll*,int cut);
int pr_paste(struct piano_roll*);
/* Preview is always derived from the unmodified selection, never accumulated. */
int pr_edit(struct piano_roll*,int64_t time_delta,int row_delta,int64_t length_delta,int velocity_delta);
int pr_quantize(struct piano_roll*,enum pr_snap);
int pr_apply(struct piano_roll*);
void pr_cancel(struct piano_roll*);
void pr_zoom(struct piano_roll*,int horizontal,int value);
void pr_layout_range(struct piano_roll*,int low,int high);
void pr_follow(struct piano_roll*,int64_t play_tick,int playing);
int pr_browse(struct piano_roll*,int delta);
#endif
