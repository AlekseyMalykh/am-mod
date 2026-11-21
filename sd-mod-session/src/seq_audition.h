#ifndef SEQ_AUDITION_H
#define SEQ_AUDITION_H
#include <stddef.h>
#include <stdint.h>
/* Sample Swap Audition: temporarily points layer 0 of one pad at the sample
 * that the sample browser holds in its private audition instrument. Only the
 * layer's sample pointer and its trim range change; the project, the
 * sequence events and the pad's own settings are never edited. */
struct seq_audition_ops{
 void*(*instrument)(void*program,unsigned pad);
 int(*get_sample)(void*instrument,uintptr_t out[2]); /* out[1] holds one reference */
 void(*release)(uintptr_t ctrl);
 int64_t(*frames)(uintptr_t sample);
 int(*chopped)(uintptr_t sample);
 int(*looping)(void*layer);
 uint64_t(*now_ms)(void);
 int(*root_of)(uintptr_t sample); /* Orig Note 0..11 (C..B) or -1 */
 int(*ready)(uintptr_t sample); /* audio data loaded; NULL = always ready (host tests) */
};
void seq_audition_init(uintptr_t base);
void seq_audition_set_ops(const struct seq_audition_ops*ops); /* host tests */
/* 1 = on, 0 = off, -1 = refused (reason copied to msg). */
int seq_audition_toggle(void*program,unsigned pad,void*audition_program,char*msg,size_t size);
void seq_audition_tick(void*current_program,void*audition_program,int browser_visible);
void seq_audition_off(const char*why);
/* Project save runs on another thread: put the original sample back for
 * the duration of serialization. */
void seq_audition_save_begin(void);
void seq_audition_save_end(void);
int seq_audition_active(void);
/* K2: Orig Note of the audition file (0 = none, then C..B). The pad is
 * transposed by the difference to the pad sample's Orig Note while audition
 * is on; its Semi Tune is put back on exit. Returns 1 when consumed. */
int seq_audition_knob(unsigned value);
struct seq_audition_view {int state,file_root,pad_root,transpose;unsigned pad;};
int seq_audition_view(struct seq_audition_view*);
/* Preview Off restores the source; a completed file load resumes audition. */
void seq_audition_preview(int enabled);
void seq_audition_info(char*out,size_t size);
/* 0 off, 1 waiting for a file, 2 swapped, 3 held for save. */
int seq_audition_state(void);
unsigned seq_audition_pad(void);
int seq_audition_plan(int64_t a_first,int64_t a_last,int64_t a_frames,int64_t b_first,int64_t b_last,int64_t b_frames,int64_t*mid_first,int64_t*mid_last);
#endif
