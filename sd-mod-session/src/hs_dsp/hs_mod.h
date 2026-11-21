#ifndef HS_MOD_H
#define HS_MOD_H
/* MOD: any HS effect parameter follows a key bus envelope (hs_env.h). Five control ports appended
   after an effect's own: Source Off/A-D, Target (the effect's own parameter index), Depth +/-100 %
   of the target's range, Attack and Release of the follower. Old projects load the missing ports at
   their lower bounds, so Source must stay "Off" at 0. The knob keeps showing the set value; the
   effect hears set value + depth x envelope, worked out once per run() block. */
enum { HS_MOD_SRC, HS_MOD_TARGET, HS_MOD_DEPTH, HS_MOD_ATTACK, HS_MOD_RELEASE, HS_MOD_PORTS };

/* The MOD and MOD TIME pages for banks.inc, base = the effect's first MOD parameter. */
#define HS_MOD_BANKS(base, module) \
    {"MOD", module, {(base)+HS_MOD_SRC, (base)+HS_MOD_TARGET, (base)+HS_MOD_DEPTH, -1, -1, -1}, \
        {"Source", "Target", "Depth", "", "", ""}, {"", "", "%"}}, \
    {"MOD TIME", module, {(base)+HS_MOD_ATTACK, (base)+HS_MOD_RELEASE, -1, -1, -1, -1}, \
        {"Attack", "Release", "", "", "", ""}, {"MS", "MS"}}

#ifndef __cplusplus
#include <math.h>
#include "ladspa.h"
#include "hs_env.h"

#define HS_MOD_TYPE (LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL)
#define HS_MOD_TYPES HS_MOD_TYPE, HS_MOD_TYPE, HS_MOD_TYPE, HS_MOD_TYPE, HS_MOD_TYPE
#define HS_MOD_NAMES "Mod Source (Off,A,B,C,D)", "Mod Target", "Mod Depth (%)", "Mod Attack (ms)", "Mod Release (ms)"
#define HS_MOD_DEFAULTS 0, 0, 0, 5.623f, 223.6f     /* the hint defaults below */
/* first = port index of the Source port, targets = how many of the effect's own controls come before it */
#define HS_MOD_HINTS(first, targets) \
    [(first)+HS_MOD_SRC] = {LADSPA_HINT_BOUNDED_BELOW|LADSPA_HINT_BOUNDED_ABOVE|LADSPA_HINT_INTEGER|LADSPA_HINT_DEFAULT_0, 0, HS_ENV_BUSES}, \
    [(first)+HS_MOD_TARGET] = {LADSPA_HINT_BOUNDED_BELOW|LADSPA_HINT_BOUNDED_ABOVE|LADSPA_HINT_INTEGER|LADSPA_HINT_DEFAULT_0, 0, (targets)-1}, \
    [(first)+HS_MOD_DEPTH] = {LADSPA_HINT_BOUNDED_BELOW|LADSPA_HINT_BOUNDED_ABOVE|LADSPA_HINT_DEFAULT_MIDDLE, -100, 100}, \
    [(first)+HS_MOD_ATTACK] = {LADSPA_HINT_BOUNDED_BELOW|LADSPA_HINT_BOUNDED_ABOVE|LADSPA_HINT_LOGARITHMIC|LADSPA_HINT_DEFAULT_LOW, 1, 1000}, \
    [(first)+HS_MOD_RELEASE] = {LADSPA_HINT_BOUNDED_BELOW|LADSPA_HINT_BOUNDED_ABOVE|LADSPA_HINT_LOGARITHMIC|LADSPA_HINT_DEFAULT_MIDDLE, 10, 5000}

struct hs_mod { float env; };

/* v = the effect's clamped control values, MOD ports at v[first]; hints = the effect's hints for
   v[0..]. Call before the effect compares v with its last settings. Source Off leaves v untouched.
   ponytail: the bus is a block-old peak, so the follower runs at block rate, not per sample. */
static inline void hs_mod_apply(struct hs_mod *m, float *v, int first, const LADSPA_PortRangeHint *hints,
                                unsigned long frames, unsigned long rate)
{
    const float *mod = v + first;
    int src = (int)lroundf(mod[HS_MOD_SRC]), t = (int)lroundf(mod[HS_MOD_TARGET]);
    if (!src || t < 0 || t >= first || !frames || !rate) return;
    float peak = hs_env_read(src);
    float level = peak > 1e-3f ? fminf(1, 1 + 20*log10f(peak)/60) : 0;   /* -60..0 dBFS -> 0..1 */
    float ms = level > m->env ? mod[HS_MOD_ATTACK] : mod[HS_MOD_RELEASE];
    m->env += -expm1f(-1000.0f*frames/(rate*ms))*(level - m->env);
    if (!(m->env >= 1e-6f)) m->env = 0;                                  /* denormal / NaN */
    const LADSPA_PortRangeHint *h = &hints[t];
    float lo = h->LowerBound, hi = h->UpperBound, d = mod[HS_MOD_DEPTH]*.01f*m->env;
    if (!LADSPA_IS_HINT_BOUNDED_BELOW(h->HintDescriptor) || !LADSPA_IS_HINT_BOUNDED_ABOVE(h->HintDescriptor)) return;
    float x = LADSPA_IS_HINT_LOGARITHMIC(h->HintDescriptor) && lo > 0 && v[t] > 0
        ? v[t]*powf(hi/lo, d) : v[t] + d*(hi - lo);
    v[t] = fminf(hi, fmaxf(lo, x));
}
#endif
#endif
