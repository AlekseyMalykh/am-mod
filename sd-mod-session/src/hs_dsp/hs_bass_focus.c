/* HS Bass Focus: split at a crossover, make the lows mono and set their level; the highs pass.
 * Controls: crossover, bass level, mono/stereo.
 * Linkwitz-Riley 4th-order split, so low + high sum back flat (an all-pass, no level change). */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "ladspa.h"
#include "hs_bass_focus.h"

enum { IL, IR, OL, OR, FIRST, PORTS = FIRST + BF_COUNT };

struct biquad { float b0, b1, b2, a1, a2, z1, z2; };
struct focus {
    float *port[PORTS];
    float rate, freq, bass, gain;
    int mono, ready;
    struct biquad lp[2][2], hp[2][2];     /* [channel][stage] */
};

/* SampQOL loads DEFAULT_0 as the lower bound, so Bass uses DEFAULT_HIGH on -24..+8: .25*-24 + .75*8 = 0 dB.
   Crossover DEFAULT_MIDDLE on a log 30..500 Hz range is sqrt(30*500) = 122 Hz. */
static const LADSPA_PortRangeHint hints[PORTS] = {
    [FIRST+BF_FREQ] = {LADSPA_HINT_BOUNDED_BELOW|LADSPA_HINT_BOUNDED_ABOVE|LADSPA_HINT_LOGARITHMIC|LADSPA_HINT_DEFAULT_MIDDLE, 30, 500},
    [FIRST+BF_BASS] = {LADSPA_HINT_BOUNDED_BELOW|LADSPA_HINT_BOUNDED_ABOVE|LADSPA_HINT_DEFAULT_HIGH, -24, 8},
    [FIRST+BF_MONO] = {LADSPA_HINT_BOUNDED_BELOW|LADSPA_HINT_BOUNDED_ABOVE|LADSPA_HINT_TOGGLED|LADSPA_HINT_DEFAULT_1, 0, 1},
};

static void butter(struct biquad *b, int high, float hz, float rate)
{
    float w = 6.2831853f*hz/rate, c = cosf(w), al = sinf(w)*.70710678f, a0 = 1 + al;
    float k = high ? (1 + c)*.5f : (1 - c)*.5f;
    b->b0 = k/a0; b->b1 = (high ? -2*k : 2*k)/a0; b->b2 = k/a0;
    b->a1 = -2*c/a0; b->a2 = (1 - al)/a0;
}

static inline float run_bq(struct biquad *b, float x)
{
    float y = b->b0*x + b->z1;
    b->z1 = b->b1*x - b->a1*y + b->z2;
    b->z2 = b->b2*x - b->a2*y;
    return y;
}

static float param(const struct focus *s, int k, float lo, float hi, float def)
{
    const float *p = s->port[FIRST + k];
    return p && isfinite(*p) ? fminf(hi, fmaxf(lo, *p)) : def;
}

static LADSPA_Handle instantiate(const LADSPA_Descriptor *d, unsigned long rate)
{
    (void)d;
    if (!rate) return NULL;
    struct focus *s = calloc(1, sizeof *s);
    if (s) s->rate = rate;
    return s;
}

static void activate(LADSPA_Handle h)
{
    struct focus *s = h;
    for (int c = 0; c < 2; ++c) for (int k = 0; k < 2; ++k) s->lp[c][k].z1 = s->lp[c][k].z2 = s->hp[c][k].z1 = s->hp[c][k].z2 = 0;
    s->ready = 0;
}

static void connect(LADSPA_Handle h, unsigned long port, LADSPA_Data *p)
{
    struct focus *s = h;
    if (port < PORTS) s->port[port] = p;
}

static void run(LADSPA_Handle h, unsigned long n)
{
    struct focus *s = h;
    if (!s->port[IL] || !s->port[IR] || !s->port[OL] || !s->port[OR]) return;
    float freq = param(s, BF_FREQ, 30, 500, 122), bass = param(s, BF_BASS, -24, 8, 0);
    s->mono = param(s, BF_MONO, 0, 1, 1) > .5f;
    if (!s->ready || freq != s->freq) {
        for (int c = 0; c < 2; ++c) for (int k = 0; k < 2; ++k) {
            struct biquad *l = &s->lp[c][k], *hh = &s->hp[c][k];
            float l1 = l->z1, l2 = l->z2, h1 = hh->z1, h2 = hh->z2;
            butter(l, 0, freq, s->rate); butter(hh, 1, freq, s->rate);
            l->z1 = l1; l->z2 = l2; hh->z1 = h1; hh->z2 = h2;     /* keep state: no click on a sweep */
        }
        s->freq = freq;
    }
    if (!s->ready || bass != s->bass) { s->gain = powf(10, bass*.05f); s->bass = bass; }
    s->ready = 1;

    for (unsigned long i = 0; i < n; ++i) {
        float low[2], high[2], in[2] = {s->port[IL][i], s->port[IR][i]};
        for (int c = 0; c < 2; ++c) {
            float x = isfinite(in[c]) ? in[c] : 0;
            low[c] = run_bq(&s->lp[c][1], run_bq(&s->lp[c][0], x));
            high[c] = run_bq(&s->hp[c][1], run_bq(&s->hp[c][0], x));
        }
        if (s->mono) low[0] = low[1] = .5f*(low[0] + low[1]);
        s->port[OL][i] = low[0]*s->gain + high[0];
        s->port[OR][i] = low[1]*s->gain + high[1];
    }
    for (int c = 0; c < 2; ++c) for (int k = 0; k < 2; ++k) {          /* flush denormals, recover from NaN */
        float *z[4] = {&s->lp[c][k].z1, &s->lp[c][k].z2, &s->hp[c][k].z1, &s->hp[c][k].z2};
        for (int j = 0; j < 4; ++j) if (!(fabsf(*z[j]) >= 1e-24f && isfinite(*z[j]))) *z[j] = 0;
    }
}

#define AI (LADSPA_PORT_INPUT | LADSPA_PORT_AUDIO)
#define AO (LADSPA_PORT_OUTPUT | LADSPA_PORT_AUDIO)
#define C (LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL)
static const LADSPA_PortDescriptor types[] = {AI,AI,AO,AO,C,C,C};
static const char *const names[] = {"Input L", "Input R", "Output L", "Output R",
                                    "Crossover (Hz)", "Bass (dB)", "Mono bass"};
_Static_assert(sizeof names / sizeof *names == PORTS && sizeof types / sizeof *types == PORTS, "Bass Focus port count");

LADSPA_Descriptor hs_bass_focus_descriptor = {
    .UniqueID = HS_BASS_FOCUS_ID, .Label = "hs_bass_focus", .Name = "HS Bass Focus",
    .Maker = "Henry Skillz", .Copyright = "Prototype",
    .Properties = LADSPA_PROPERTY_HARD_RT_CAPABLE,
    .PortCount = PORTS, .PortDescriptors = types, .PortNames = names, .PortRangeHints = hints,
    .instantiate = instantiate, .connect_port = connect, .activate = activate, .run = run, .cleanup = free
};
