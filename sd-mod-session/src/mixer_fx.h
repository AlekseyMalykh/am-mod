#ifndef AM_MIXER_FX_H
#define AM_MIXER_FX_H
#include "mixer_live.h"
#ifdef __cplusplus
extern "C" {
#endif
struct mixer_fx;
/* UI/control owner. Context outlives session DSP and all retired processors. */
struct mixer_fx*mixer_fx_create(uintptr_t image,const struct mixer_native_binding*);
void mixer_fx_refresh(struct mixer_fx*,const struct mixer_native_binding*);
const struct fx_backend*mixer_fx_backend(struct mixer_fx*);
int mixer_fx_ready(const struct mixer_fx*);
int mixer_fx_fault(const struct mixer_fx*);
void mixer_fx_destroy(struct mixer_fx*);
#ifdef __cplusplus
}
#endif
#endif
