#ifndef MPC_MOD_FX_HS_H
#define MPC_MOD_FX_HS_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Henry Skillz effects (src/hs_dsp, used with the author's permission) hosted as Am MOD
 * Mixer effects. Same contract as fx_own: parameters normalized 0..1, interleaved stereo in place.
 * The object starts with its uint32_t id so fx_own can route calls by it. */
#define FX_HS_SATURATOR 0x414d4f09u
#define FX_HS_BASS      0x414d4f0au
#define FX_HS_HEAT      0x414d4f0bu
#define FX_HS_COUNT 3u
int fx_hs_is(uint32_t id);
void *fx_hs_create(uint32_t id,double rate,const float *parameters);
void fx_hs_update(void *,const float *parameters);
void fx_hs_destroy(void *);
float fx_hs_process(void *,float *samples,size_t frames);
#ifdef __cplusplus
}
#endif
#endif
