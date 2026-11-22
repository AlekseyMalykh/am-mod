#ifndef MPC_MUSIC_SETTINGS_H
#define MPC_MUSIC_SETTINGS_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define MUSIC_MAX_SAMPLES 512
#define MUSIC_NAME_BYTES 513
#define MUSIC_PAYLOAD_BYTES (MUSIC_MAX_SAMPLES*1100+4096)
enum { MUSIC_MONO, MUSIC_PIANO, MUSIC_TONIC, MUSIC_SCALE, MUSIC_DEGREES };
enum { MUSIC_ROOT, MUSIC_MODE, MUSIC_TRANSPOSE, MUSIC_COLOUR0, MUSIC_COLOUR1, MUSIC_COLOUR2, MUSIC_LOCAL_TONIC, MUSIC_LOCAL_SCALE, MUSIC_ORIGINAL_COLOUR, MUSIC_FIFTH_COLOUR, MUSIC_FOURTH_COLOUR };
/* Lights colour defaults: Tonic Red, Scale/key notes Cyan, Other Grey, IV Violet, V Yellow, Original Magenta. */
#define MUSIC_DEFAULT_TONIC_COLOUR 5
#define MUSIC_DEFAULT_FOURTH_COLOUR 3
#define MUSIC_DEFAULT_FIFTH_COLOUR 8
struct music_sample { char name[MUSIC_NAME_BYTES]; int root,mode,transpose,colour[3],tonic,scale,original_colour,fifth_colour,fourth_colour; };
struct music_project { int tonic,scale; unsigned count; struct music_sample samples[MUSIC_MAX_SAMPLES]; uint64_t pad_eq[128]; unsigned char sequence_colours[128]; unsigned char pad_colours[128]; uint64_t sequence_mutes[128][2]; };
#define MUSIC_SEQUENCE_COLOURS 48
extern const char *const music_sequence_colour_names[MUSIC_SEQUENCE_COLOURS];
extern const uint32_t music_sequence_colours[MUSIC_SEQUENCE_COLOURS];
enum { MUSIC_LIGHTING_STANDARD, MUSIC_LIGHTING_MOD };
enum { MUSIC_LINK_OFF, MUSIC_LINK_SELECTED, MUSIC_LINK_PLAYING, MUSIC_LINK_BOTH };
int music_pad_link_lighting(void);
int music_mute_group_lighting(void);
int music_set_pad_link_lighting(unsigned mode);
int music_set_mute_group_lighting(unsigned enabled);
/* Card-level colour roles for relation hints. Index = RGB palette slot
 * 0..31. System reservation applies to non-system roles. */
enum { MUSIC_RELATION_LINK, MUSIC_RELATION_GROUP, MUSIC_RELATION_COUNT };
unsigned music_relation_colour(unsigned role);
int music_set_relation_colour(unsigned role,unsigned slot);
uint32_t music_relation_rgb(unsigned role);
/* LED colour strength: 0 Standard (unchanged), 1 Strong, 2 Max. Removes the
 * white component of mixed colours so they do not look washed out. */
/* Mixer effect screens: 1 Illustrated (default), 0 Classic (plain traces). Card-wide. */
/* Default text input in naming/search windows: 1 Pads (default), 0 Encoder. Card-wide. */
unsigned music_text_input(void);
int music_set_text_input(unsigned pads);
unsigned music_fx_view(void);
int music_set_fx_view(unsigned view);
unsigned music_fx_mode(void);        /* saved choice: 0 Stock, 1 Am MOD */
unsigned music_fx_mode_active(void); /* mode in force since start-up */
int music_set_fx_mode(unsigned mode);
unsigned music_usb_audio(void); /* saved: 0 stereo, 1 Master + A-H; restart */
int music_set_usb_audio(unsigned mode);
/* Recording route (card preference, all projects): field 0 point (0 Input, 1 Track, 2 Master),
 * field 1 track (0..7 = A..H), field 2 effects recorded (0 dry, 1 wet). */
unsigned music_rec_get(unsigned field);
int music_rec_set(unsigned field,unsigned value);
unsigned music_led_vivid(void);
int music_set_led_vivid(unsigned level);
uint32_t music_led_correct(uint32_t rgb);
int music_pad_lighting_mode(void);
int music_set_pad_lighting_mode(unsigned mode); /* UI owner; durable when SD available */
void music_lighting_preferences_init(void);
const char *music_global_midi_text(void); /* card-wide MIDI map default, NULL when none */
void music_global_midi_baseline(void);    /* after a load: the current map is not an edit */
void music_global_sync(int force);        /* UI thread: persist changed MIDI map / Synth defaults */
int music_lighting_preferences_open(const char *directory); /* caller provides owned settings directory */
int music_lighting_preferences_durable(void);
/* Historical settings word keeps its stable permutation encoding. New service
 * assignments reference the same named, card-wide palette as every other selector. */
enum { MUSIC_SERVICE_SELECT, MUSIC_SERVICE_FREE, MUSIC_SERVICE_DANGER,
       MUSIC_SERVICE_ORIGINAL, MUSIC_SERVICE_PLAYHEAD, MUSIC_SERVICE_MUTED,
       MUSIC_SERVICE_COUNT };
#define MUSIC_USER_COLOURS 8
#define MUSIC_PALETTE_SIZE (MUSIC_USER_COLOURS+MUSIC_SERVICE_COUNT)
#define MUSIC_PALETTE_SLOTS 32
#define MUSIC_PALETTE_BANK_SIZE 16
unsigned music_palette_reserved(void);
int music_set_palette_reserved(unsigned enabled);
int music_palette_available(unsigned slot);
unsigned music_palette_choice_count(void);
unsigned music_palette_choice(unsigned index);
unsigned music_palette_choice_index(unsigned slot);
unsigned music_light_colour_count(void);
unsigned music_light_colour_choice(unsigned index);
unsigned music_light_colour_index(unsigned raw);
const char *music_light_colour_name(unsigned raw);
uint32_t music_light_colour_rgb(unsigned raw);
#define MUSIC_COLOUR_NAME_BYTES 24
enum { MUSIC_SETTING_LIGHTING, MUSIC_SETTING_WAVE, MUSIC_SETTING_SPECTRUM, MUSIC_SETTING_BLINK, MUSIC_SETTING_TAIL };
#define MUSIC_TAIL_CHOICES 8
unsigned music_tail_ms(unsigned choice);
uint64_t music_mod_settings_snapshot(void);
int music_mod_settings_valid(uint64_t value);
int music_mod_settings_restore(uint64_t value); /* project load; no disk write */
int music_mod_setting(unsigned field);
int music_set_mod_setting(unsigned field,unsigned value);
int music_mod_restore_recommended(unsigned section);
unsigned music_service_colour(unsigned role);
int music_set_service_colour(unsigned role,unsigned colour);
uint32_t music_service_rgb(unsigned role);
int music_service_colours_fallback(void);
const char *music_palette_name(unsigned colour);
uint32_t music_palette_rgb(unsigned colour);
uint32_t music_palette_default_rgb(unsigned colour);
int music_palette_set_rgb(unsigned colour,uint32_t rgb); /* card preference */
int music_palette_present(unsigned colour);
struct music_palette_edit {
    uint32_t rgb;
    char name[MUSIC_COLOUR_NAME_BYTES];
    unsigned reset;
};
/* Apply edited slots together. Publish only after the card write succeeds. */
int music_palette_apply(const struct music_palette_edit edits[MUSIC_PALETTE_SLOTS],unsigned mask);
int music_palette_save(unsigned colour,uint32_t rgb,const char *name);
int music_palette_reset(unsigned colour);
unsigned music_identity_count(void);
unsigned music_identity_choice(unsigned index);
unsigned music_identity_index(unsigned raw);
uint32_t music_user_rgb(unsigned raw);
const char *music_identity_name(unsigned raw);
int music_cursor_on(uint64_t phase,unsigned duty); /* duty in thousandths */
/* Persisted legacy IDs remain stable. New IDs 16..47 address shared slots.
 * Reserved system colours are omitted from all non-system selectors.
 * Raw getters/setters retain legacy metadata for Undo and project round trips. */
#define MUSIC_IDENTITY_CHOICES (MUSIC_USER_COLOURS+1)
static inline unsigned music_colour_choice(unsigned index){
 static const unsigned ids[MUSIC_IDENTITY_CHOICES]={0,8,3,4,6,13,9,10,7};
 return ids[index<MUSIC_IDENTITY_CHOICES?index:MUSIC_IDENTITY_CHOICES-1];
}
static inline unsigned music_colour_effective(unsigned raw){
 static const unsigned ids[MUSIC_SEQUENCE_COLOURS]={8,8,3,3,4,4,6,7,8,9,10,3,8,13};
 return ids[raw<MUSIC_SEQUENCE_COLOURS?raw:0];
}
static inline unsigned music_colour_choice_index(unsigned raw){
 if(!raw)return 0;
 unsigned safe=music_colour_effective(raw);
 for(unsigned i=1;i<MUSIC_IDENTITY_CHOICES;++i)if(music_colour_choice(i)==safe)return i;
 return 0;
}
static inline uint32_t music_identity_rgb(unsigned raw){return music_pad_lighting_mode()?music_user_rgb(raw):0x2030ffu;}
int music_sequence_colour(unsigned sequence);
int music_set_sequence_colour(unsigned sequence,unsigned colour);
/* Sequence Pad Mutes: per sequence (0..127) a mask of the 128 pads (pad = bank*16+index). Project data only. */
int music_sequence_pad_muted(unsigned sequence,unsigned pad);
int music_sequence_pad_toggle(unsigned sequence,unsigned pad);   /* new state 0/1, -1 on bad arguments */
int music_sequence_mutes_get(unsigned sequence,uint64_t out[2]); /* 1 if any pad is muted */
int music_sequence_mutes_clear(unsigned sequence);
int music_sequence_mutes_copy(unsigned from,unsigned to);
/* Own project field "mpcSampleModSeqMutesV1" (separate from the main payload, ignored by older builds).
 * encode returns 0 when no sequence has a mute (the field is then not written). */
#define SEQ_MUTES_PAYLOAD_BYTES 8192
int music_seqmutes_encode(const struct music_project *p,char *out,size_t size);
int music_seqmutes_decode(const char *in,size_t size,struct music_project *out);
/* Pad colour in Sample Mix K2 without Shift: same palette, 0 = Stock. */
int music_pad_colour(unsigned pad);
int music_set_pad_colour(unsigned pad,unsigned colour);
int music_pad_colours_used(void);
/* Sample window pad colour source (card preference): 0 = the pad's own colour, 1 = colour of the
 * Mixer track the pad is assigned to (Am MOD only; falls back to the pad colour). */
/* Card preference: hide only pad-selection overlays while transport runs. */
unsigned music_hide_play_cursor(void);
int music_set_hide_play_cursor(unsigned hide);
extern unsigned am_midi_held_notes(void);
#pragma weak am_midi_held_notes
/* A pad played from incoming MIDI shows as pressed even with the cursor hidden. */
static inline int music_pad_cursor_visible(unsigned transport){
 return !(transport&1u)||!music_pad_lighting_mode()||!music_hide_play_cursor()||(am_midi_held_notes&&am_midi_held_notes()!=0);
}
unsigned music_piano_roll_colour(void);
int music_set_piano_roll_colour(unsigned value);
unsigned music_pad_colour_source(void);
int music_set_pad_colour_source(unsigned value);
int music_pad_colour_locked(void); /* Track colour source in force: Pad Colour cannot be edited */
extern uint32_t music_pad_track_rgb(unsigned pad) __attribute__((weak)); /* 0 = no track colour */
static inline uint32_t music_pad_rgb(unsigned pad){
 uint32_t track=music_pad_track_rgb?music_pad_track_rgb(pad):0;
 return track?track:music_identity_rgb((unsigned)music_pad_colour(pad));
}
extern const char *const music_note_names[12];
extern const char *const music_scale_names[8];
extern const char *const music_colour_names[12];
extern const uint32_t music_colours[12];
void music_defaults(struct music_project *p);
void music_reset(void);
uint64_t music_generation(void);
void music_get_key(int *tonic,int *scale);
void music_set_key(int tonic,int scale);
int music_get_sample(const char *name,struct music_sample *out);
int music_set_sample(const char *name,int field,int value);
void music_snapshot(struct music_project *out);
void music_replace(const struct music_project *p);
uint64_t music_get_pad_eq(unsigned pad);
int music_set_pad_eq(unsigned pad,uint64_t value);
int music_encode(const struct music_project *p,char *out,size_t size);
int music_decode(const char *in,size_t size,struct music_project *out);
void music_key_text(int tonic,int scale,char *out,size_t size);
int music_key_from_text(const char *text,int *tonic,int *scale);
int music_key_edited(void);
/* Pack all data needed on native lighting/DSP paths into one atomic snapshot. */
void music_publish(const struct music_sample *sample,int semitones);
/* Independent UI snapshot; does not change the ordinary Sample selection. */
uint64_t music_light_snapshot(const struct music_sample *sample,int semitones);
uint64_t music_published(void);
int music_transposition(void);
/* UI order differs from stable project enum IDs: Monochrome, Piano, Scale, Degrees. */
int music_mode_ui_index(int mode);
int music_mode_from_ui_index(int index);
#define MUSIC_LAYOUT_CHOICES 4
/* 0 = I, 1 = IV, 3 = V, 2 = other scale degree, -1 = outside the scale. */
int music_degree_role(unsigned mask,int degree);
uint64_t music_pad_light(uint64_t snapshot,unsigned pad,int original_pad,int *replace);
/* Pad layout of 16 Levels Tune. Degrees maps pads to consecutive key notes;
 * Transpose16 then moves by scale degrees. Returns 1 and the semitone offset
 * when native's chromatic pad pitch must be replaced. */
#define MUSIC_LAYOUT_MAX_PITCH 39
int music_layout_pad_pitch(uint64_t snapshot,unsigned pad,int original_pad,int *pitch);
int music_layout_semitones(uint64_t snapshot,int index);
/* Recorded Tune modifiers already contain transposition. */
uint64_t music_pitch_light(uint64_t snapshot,int semitones,int *replace);
#ifdef __cplusplus
}
#endif
#endif
