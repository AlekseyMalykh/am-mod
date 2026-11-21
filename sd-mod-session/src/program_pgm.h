#ifndef AM_PROGRAM_PGM_H
#define AM_PROGRAM_PGM_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define AM_PGM_BYTES 10756u
#define AM_PGM_PADS 64u
#define AM_PGM_LAYERS 4u
/* Source units, NOT equivalent Sample parameter values. No native pointers,
 * file I/O, allocation, audio-thread work or mutation of the current project. */
struct am_pgm_layer {
    char name[17];
    unsigned level,velocity_low,velocity_high,play_mode;
    int tune_cents;
};
struct am_pgm_pad {
    struct am_pgm_layer layers[AM_PGM_LAYERS];
    unsigned overlap,mute_group,attack,decay,decay_mode,velocity_level;
    unsigned filter1,frequency1,resonance1,velocity_frequency1;
    unsigned filter2,frequency2,resonance2;
    unsigned level,pan,output,fx_send,fx_level,filter_attenuation;
};
struct am_pgm_slider {
    unsigned pad,parameter,layer_low,layer_high,attack_low,attack_high,decay_low,decay_high;
    int tune_low,tune_high,filter_low,filter_high;
};
struct am_pgm_program {
    struct am_pgm_pad pads[AM_PGM_PADS];
    uint8_t note_for_pad[64],pad_for_note[128]; /* 64 means unassigned. */
    unsigned program_change;
    struct am_pgm_slider sliders[2];
};
enum am_pgm_status { AM_PGM_OK, AM_PGM_SIZE, AM_PGM_FORMAT, AM_PGM_NAME,
    AM_PGM_RANGE, AM_PGM_ARGUMENT };
struct am_pgm_error { enum am_pgm_status status; size_t offset; };
/* Recognises exact MPC1000 PGM1.00, irrespective of extension. Input can be
 * unaligned. Output is unchanged on ANY error. Bytes after a name's first NUL
 * are deliberately ignored: real MPC programs contain stale name storage.
 * Empty layers/pads have undefined parameter storage; ranges are checked only
 * on named layers / occupied pads. Unknown padding is not interpreted. */
enum am_pgm_status am_pgm_read(const void*,size_t,struct am_pgm_program*,struct am_pgm_error*);
/* Preflight only. Nonempty source pads map to source+first_pad. Occupied
 * destination pads are refused, including partial overlaps. No overwrite mode
 * yet. On failure mapping is unchanged; unused source pads have target255.
 * MIDI tables remain separate: native note IDs must not be inferred from pad
 * ordinal or overwritten by an incomplete one-note-per-pad approximation. */
int am_pgm_plan(const struct am_pgm_program*,unsigned first_pad,
    const uint8_t occupied[128],uint8_t mapping[64]);
#ifdef __cplusplus
}
#endif
#endif
