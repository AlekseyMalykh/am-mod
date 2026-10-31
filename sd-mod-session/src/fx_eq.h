#ifndef MPC_MOD_FX_EQ_H
#define MPC_MOD_FX_EQ_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

#define FX_EQ_MAX_BANDS 8
enum fx_eq_type { FX_EQ_BELL, FX_EQ_LOW_SHELF, FX_EQ_HIGH_SHELF, FX_EQ_HIGH_PASS, FX_EQ_LOW_PASS, FX_EQ_NOTCH };
struct fx_eq_band { double hz, gain_db, q; unsigned type, enabled; };
struct fx_eq_config { unsigned count, bypass; double output_db; struct fx_eq_band band[FX_EQ_MAX_BANDS]; };
struct fx_eq_coeff { double b0,b1,b2,a1,a2; };
struct fx_eq_history { double z1[2],z2[2]; };
struct fx_eq {
    double rate,mix,mix_target,output,output_target;
    unsigned ramp,remaining;
    struct fx_eq_coeff coeff[FX_EQ_MAX_BANDS],target[FX_EQ_MAX_BANDS];
    struct fx_eq_history history[FX_EQ_MAX_BANDS];
};
void fx_eq_default(struct fx_eq_config *);
int fx_eq_valid(const struct fx_eq_config *,double rate);
int fx_eq_coefficients(const struct fx_eq_band *,double rate,struct fx_eq_coeff *);
int fx_eq_init(struct fx_eq *,double rate,const struct fx_eq_config *);
/* Audio-owner call at a block boundary. Caller delivers a complete snapshot.
 * No UI object, allocator, lock or native model is used by these functions. */
int fx_eq_update(struct fx_eq *,const struct fx_eq_config *);
void fx_eq_process(struct fx_eq *,float *stereo,size_t frames);
double fx_eq_response_db(const struct fx_eq_config *,double rate,double hz);
#ifdef __cplusplus
}
#endif
#endif
