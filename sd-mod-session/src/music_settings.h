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
enum { MUSIC_ROOT, MUSIC_MODE, MUSIC_TRANSPOSE, MUSIC_COLOUR0, MUSIC_COLOUR1, MUSIC_COLOUR2, MUSIC_LOCAL_TONIC, MUSIC_LOCAL_SCALE, MUSIC_ORIGINAL_COLOUR };
struct music_sample { char name[MUSIC_NAME_BYTES]; int root,mode,transpose,colour[3],tonic,scale,original_colour; };
struct music_project { int tonic,scale; unsigned count; struct music_sample samples[MUSIC_MAX_SAMPLES]; uint64_t pad_eq[128]; unsigned char sequence_colours[128]; unsigned char pad_colours[128]; };
#define MUSIC_SEQUENCE_COLOURS 14
extern const char *const music_sequence_colour_names[MUSIC_SEQUENCE_COLOURS];
extern const uint32_t music_sequence_colours[MUSIC_SEQUENCE_COLOURS];
enum { MUSIC_LIGHTING_STANDARD, MUSIC_LIGHTING_MOD };
int music_pad_lighting_mode(void);
int music_set_pad_lighting_mode(unsigned mode); /* UI owner; durable when SD available */
void music_lighting_preferences_init(void);
int music_lighting_preferences_open(const char *directory); /* caller provides owned settings directory */
int music_lighting_preferences_durable(void);
/* Atomic display snapshot: eight user slots and six service colours form a
 * permutation. Changing a service colour swaps its previous owner. */
enum { MUSIC_SERVICE_SELECT, MUSIC_SERVICE_FREE, MUSIC_SERVICE_DANGER,
       MUSIC_SERVICE_ORIGINAL, MUSIC_SERVICE_PLAYHEAD, MUSIC_SERVICE_MUTED,
       MUSIC_SERVICE_COUNT };
#define MUSIC_USER_COLOURS 8
#define MUSIC_PALETTE_SIZE (MUSIC_USER_COLOURS+MUSIC_SERVICE_COUNT)
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
const char *music_palette_name(unsigned colour);
uint32_t music_palette_rgb(unsigned colour);
uint32_t music_user_rgb(unsigned raw);
const char *music_identity_name(unsigned raw);
int music_cursor_on(uint64_t phase,unsigned duty); /* duty in thousandths */
/* Persisted IDs remain stable. Display identity never uses service colours.
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
/* Pad colour in Sample Mix K2 without Shift: same palette, 0 = Stock. */
int music_pad_colour(unsigned pad);
int music_set_pad_colour(unsigned pad,unsigned colour);
int music_pad_colours_used(void);
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
/* 0 = I, 1 = IV or V, 2 = other scale degree, -1 = outside the scale. */
int music_degree_role(unsigned mask,int degree);
uint64_t music_pad_light(uint64_t snapshot,unsigned pad,int original_pad,int *replace);
/* Recorded Tune modifiers already contain transposition. */
uint64_t music_pitch_light(uint64_t snapshot,int semitones,int *replace);
#ifdef __cplusplus
}
#endif
#endif
