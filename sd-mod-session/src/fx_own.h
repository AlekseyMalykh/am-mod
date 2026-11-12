#ifndef MPC_MOD_FX_OWN_H
#define MPC_MOD_FX_OWN_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Am MOD's own effects. Plain C DSP, no firmware objects: the stage is stored as a
 * normal native stage whose id lives in this range, so project files keep their format.
 * create/destroy: control owner (allocates). process: audio owner only, no allocation. */
#define FX_OWN_SHIMMER 0x414d4f01u
#define FX_OWN_PITCH   0x414d4f02u
#define FX_OWN_REVERSE 0x414d4f03u
#define FX_OWN_TREMOLO 0x414d4f04u
#define FX_OWN_SWELL   0x414d4f05u
#define FX_OWN_RING    0x414d4f06u
#define FX_OWN_CRUSH   0x414d4f07u
#define FX_OWN_COUNT 7u
int fx_own_is(uint32_t id);
void *fx_own_create(uint32_t id,double rate,const float *parameters /* normalized 0..1 */);
void fx_own_destroy(void *);
/* Interleaved stereo, in place. */
void fx_own_process(void *,float *samples,size_t frames);
/* Display helpers shared with the screens. */
int fx_own_pitch_semitones(float normalized);   /* Pitch Shifter interval */
int fx_own_shimmer_semitones(float normalized); /* Shimmer interval */
#ifdef __cplusplus
}
#endif
#endif
