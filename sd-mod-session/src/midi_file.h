#ifndef AM_MIDI_FILE_H
#define AM_MIDI_FILE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define AM_SMF_MAX_NOTES 65536u
#define AM_SMF_MAX_EVENTS 131072u
#define AM_SMF_MAX_TRACKS 512u
#define AM_SMF_MAX_BYTES (16u*1024u*1024u)
enum { AM_SMF_MESSAGE,AM_SMF_TEMPO,AM_SMF_SIGNATURE,AM_SMF_PORT };
struct am_smf_note {uint32_t tick,duration;uint16_t track;uint8_t channel,note,velocity;};
struct am_smf_event {uint32_t tick,value;uint16_t track;uint8_t kind,size,data[3];};
struct am_smf {
 struct am_smf_note *notes;struct am_smf_event *events;
 unsigned count,capacity,event_count,event_capacity,tracks,division;
 uint32_t duration;unsigned unmatched_off;
};
void am_smf_init(struct am_smf*);
void am_smf_free(struct am_smf*);
int am_smf_note_add(struct am_smf*,struct am_smf_note);
int am_smf_event_add(struct am_smf*,struct am_smf_event);
/* SMF 0/1 with PPQN. All failures leave the caller's document/buffer unchanged. */
int am_smf_read(const void*,size_t,struct am_smf*,char error[96]);
int am_smf_write(const struct am_smf*,unsigned char **bytes,size_t *size,char error[96]);
uint64_t am_smf_native_tick(uint32_t tick,unsigned division);
enum { AM_SMF_PAD_MAP,AM_SMF_TUNE,AM_SMF_CHOP };
struct am_smf_import_options {
 int track,channel; /* -1: all; otherwise zero based. */
 unsigned mode,sound,port,chops,skip_unmapped,fit;
 int original_note;uint32_t length;
};
struct am_smf_pad_note {uint32_t tick,duration;int16_t tune;uint16_t slice;uint8_t sound,channel,velocity;};
struct am_smf_import_info {unsigned accepted,unmapped,ambiguous,out_of_range,filtered,controllers;uint64_t end;};
/* Uses the current MIDI input map, falling back to the supplied native notes.
 * Port 0 ignores source scope, but ambiguous maps are refused, never guessed.
 * Output and count change only after the entire import plan is valid. */
int am_smf_import(const struct am_smf*,const struct am_smf_import_options*,
 const int native_notes[128],struct am_smf_pad_note*,unsigned capacity,unsigned *count,
 struct am_smf_import_info*,char error[96]);
#ifdef __cplusplus
}
#endif
#endif
