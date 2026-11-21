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
#define FX_OWN_OPTO    0x414d4f08u
#define FX_OWN_HS_SATURATOR 0x414d4f09u /* Henry Skillz DSP, see fx_hs.h */
#define FX_OWN_HS_BASS      0x414d4f0au
#define FX_OWN_HS_HEAT      0x414d4f0bu
#define FX_OWN_COUNT 11u
int fx_own_is(uint32_t id);
void *fx_own_create(uint32_t id,double rate,const float *parameters /* normalized 0..1 */);
/* Size/predelay changes in Shimmer still need a prepared replacement. */
int fx_own_can_update(uint32_t,const float*,const float*);
void fx_own_update(void*,const float*);
void fx_own_destroy(void *);
/* Interleaved stereo, in place. Returns the lowest linear gain applied (1 = no reduction),
 * which the mixer shows as gain reduction. */
float fx_own_process(void *,float *samples,size_t frames);
struct comp_config;
/* OptoCat Comp: normalized parameters -> the same ranges as the Sample compressor. */
void fx_own_opto_config(const float *normalized,struct comp_config *out);
/* Display helpers shared with the screens. */
int fx_own_pitch_semitones(float normalized);   /* Pitch Shifter interval */
int fx_own_shimmer_semitones(float normalized); /* Shimmer interval */
#ifdef __cplusplus
}
#endif
#endif
