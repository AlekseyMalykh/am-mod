/* HS Heat: a stereo distortion/filter with 8 drive circuits, a multimode filter and modulation.
 * DESIGN: chain = drive -> circuit -> dirt -> multimode filter -> 2-band EQ
 *   -> wet level -> dry/wet mix -> volume; the 8 circuit names and characters, each with its own
 *   EQ voicing; some circuits colour the sound at drive 0; filter types LP2 12 dB, LP1 6 dB, BP, HP1
 *   6 dB, HP2 12 dB, notch, peak; resonance self-oscillates; Dirt overdrives the filter input;
 *   FREQ PAN spreads L/R cutoff in opposite directions; envelope follower and LFO (TRI SINE SQR SAW
 *   EXP RAMP RND) both have a hard-wired route to cutoff.
 * GUESSED (marked "guess"): every gain, frequency, voicing, curve, time and rate. The EQ corners
 *   follow one user's MKI measurement (low shelf ~200 Hz, high shelf ~2.5 kHz). */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "ladspa.h"
#include "hs_heat.h"

enum { IL, IR, OL, OR, FIRST, PORTS = FIRST + HE_COUNT, CONTROL = 16 };
enum { TANH, HARD, FUZZ, TWO_STAGE };

struct circuit {
    float lo_db, mid_hz, mid_db, hi_db;   /* voicing before the clipper (guess) */
    float gain_min, gain_max;             /* drive 0..127 in dB (guess) */
    float bias, headroom;                 /* asymmetry for even harmonics, clip point */
    int shape;
    float post_lp_hz, post_db;
};
static const struct circuit circuits[8] = {
    /* Clean Boost: loud, overdriven-mixer at the top */  {0, 1000, 0, 0,     0, 20, 0,   2, TANH, 20000, 0},
    /* Saturation: tape, woolly and warm */                {2, 1000, 0, -2,    0, 24, .1f, 1, TANH, 9000, 0},
    /* Enhancement: tube glow and sheen */                 {0, 2500, 2, 3,     0, 24, .25f, 1, TANH, 16000, 0},
    /* Mid Drive: mid-range body */                        {-3, 900, 7, -2,    6, 36, .05f, 1, TANH, 8000, -2},
    /* Rough Crunch: gritty, worn */                       {0, 1500, 3, 0,     8, 40, .2f, 1, HARD, 7000, -4},
    /* Classic Dist: upper mids, acid bass */              {-4, 1800, 8, 0,   10, 44, 0,   1, HARD, 5500, -5},
    /* Round Fuzz: lots of harmonics, upper octave */      {2, 600, 0, -3,    15, 50, .3f, 1, FUZZ, 6000, -6},
    /* High Gain: maxed out */                             {-6, 1200, 4, 2,   20, 56, .1f, 1, TWO_STAGE, 6500, -7},
};

struct biquad { float b0, b1, b2, a1, a2, z1[2], z2[2]; };
struct heat {
    float *port[PORTS];
    unsigned long rate;
    float values[HE_COUNT];
    struct biquad pre[3], eq[2];
    float drive, wet, mix, volume, post_lp, post_gain, dirt, reso_k, freq_oct, pan_oct;
    float env_atk, env_rel, env_depth, lfo_depth, lfo_step, dc;
    int circuit, ftype, wave, ready, countdown;
    /* per channel state */
    float x1[2], x2[2], inter[2], post[2], dcx[2], dcy[2], s1[2], s2[2];
    float g, k;                          /* filter coefficients, updated at control rate */
    float gl, gr;
    float env, phase, random_value;
    struct hs_mod mod;
};

static const float defaults[] = {1, 40, 0, 0, 100, 0, 0, 0, 0, 0, 20, 60, 0, 64, 0, 0, 100, 0, HS_MOD_DEFAULTS};
#define B (LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE)
#define INT (B | LADSPA_HINT_INTEGER)
static const LADSPA_PortRangeHint hints[PORTS] = {
    [FIRST+HE_CIRCUIT] = {INT|LADSPA_HINT_DEFAULT_1,0,7},
    [FIRST+HE_DRIVE] = {B|LADSPA_HINT_DEFAULT_LOW,0,127},
    [FIRST+HE_WET] = {B|LADSPA_HINT_DEFAULT_0,-24,6},
    [FIRST+HE_FTYPE] = {INT|LADSPA_HINT_DEFAULT_0,0,7},
    [FIRST+HE_FREQ] = {B|LADSPA_HINT_DEFAULT_HIGH,0,127},
    [FIRST+HE_RESO] = {B|LADSPA_HINT_DEFAULT_0,0,127},
    [FIRST+HE_DIRT] = {B|LADSPA_HINT_DEFAULT_0,0,127},
    [FIRST+HE_EQ_LO] = {B|LADSPA_HINT_DEFAULT_0,-64,63},
    [FIRST+HE_EQ_HI] = {B|LADSPA_HINT_DEFAULT_0,-64,63},
    [FIRST+HE_ENV_DEPTH] = {B|LADSPA_HINT_DEFAULT_0,-128,127},
    [FIRST+HE_ENV_ATK] = {B|LADSPA_HINT_DEFAULT_LOW,0,127},
    [FIRST+HE_ENV_REL] = {B|LADSPA_HINT_DEFAULT_MIDDLE,0,127},
    [FIRST+HE_LFO_DEPTH] = {B|LADSPA_HINT_DEFAULT_0,-128,127},
    [FIRST+HE_LFO_SPEED] = {B|LADSPA_HINT_DEFAULT_MIDDLE,0,127},
    [FIRST+HE_LFO_WAVE] = {INT|LADSPA_HINT_DEFAULT_0,0,6},
    [FIRST+HE_FREQ_PAN] = {B|LADSPA_HINT_DEFAULT_0,-64,63},
    [FIRST+HE_MIX] = {B|LADSPA_HINT_DEFAULT_MAXIMUM,0,100},
    [FIRST+HE_VOLUME] = {B|LADSPA_HINT_DEFAULT_0,-24,12},
    HS_MOD_HINTS(FIRST+HE_MOD, HE_MOD)
};
#undef B
#undef INT

static inline float coef(unsigned long rate, float ms) { return -expm1f(-1000.0f/(rate*ms)); }
static inline float db(float v) { return powf(10, v*.05f); }

/* RBJ cookbook: 0 low shelf, 1 peak, 2 high shelf */
static void design(struct biquad *b, int type, float hz, float gain_db, float q, float rate)
{
    float A = powf(10, gain_db/40), w = 6.2831853f*fminf(hz, .45f*rate)/rate, c = cosf(w), s = sinf(w);
    float b0, b1, b2, a0, a1, a2;
    if (type == 1) {
        float al = s/(2*q);
        b0 = 1+al*A; b1 = -2*c; b2 = 1-al*A; a0 = 1+al/A; a1 = -2*c; a2 = 1-al/A;
    } else {
        float al = s*.70710678f, sq = 2*sqrtf(A)*al, sign = type == 0 ? 1 : -1;   /* RBJ: low shelf takes +(A-1)cos in b0 */
        b0 = A*((A+1)-sign*(A-1)*c+sq); b1 = 2*sign*A*((A-1)-sign*(A+1)*c); b2 = A*((A+1)-sign*(A-1)*c-sq);
        a0 = (A+1)+sign*(A-1)*c+sq; a1 = -2*sign*((A-1)+sign*(A+1)*c); a2 = (A+1)+sign*(A-1)*c-sq;
    }
    b->b0 = b0/a0; b->b1 = b1/a0; b->b2 = b2/a0; b->a1 = a1/a0; b->a2 = a2/a0;
}

static inline float run_bq(struct biquad *b, float x, int c)
{
    float y = b->b0*x + b->z1[c];
    b->z1[c] = b->b1*x - b->a1*y + b->z2[c];
    b->z2[c] = b->b2*x - b->a2*y;
    return y;
}

/* Clippers and their antiderivatives, for first-order antialiasing. */
static inline float logcosh(float x) { float a = fabsf(x); return a + log1pf(expf(-2*a)) - .69314718f; }
static inline float hard(float x) { return fminf(1, fmaxf(-1, x)); }
static inline float hard_int(float x) { float a = fabsf(x); return a <= 1 ? .5f*x*x : a - .5f; }
static inline float adaa_tanh(float x, float *last)
{
    float x0 = *last, d = x - x0; *last = x;
    return fabsf(d) < 1e-4f ? tanhf(.5f*(x+x0)) : (logcosh(x) - logcosh(x0))/d;
}
static inline float adaa_hard(float x, float *last)
{
    float x0 = *last, d = x - x0; *last = x;
    return fabsf(d) < 1e-4f ? hard(.5f*(x+x0)) : (hard_int(x) - hard_int(x0))/d;
}

static LADSPA_Handle instantiate(const LADSPA_Descriptor *d, unsigned long rate)
{
    (void)d;
    if (!rate) return NULL;
    struct heat *s = calloc(1, sizeof *s);
    if (s) { s->rate = rate; s->dc = 1 - 6.2831853f*5/rate; s->random_value = .3f; }
    return s;
}

static void activate(LADSPA_Handle h)
{
    struct heat *s = h;
    unsigned long rate = s->rate;
    float r = s->random_value;
    float *port[PORTS];
    memcpy(port, s->port, sizeof port);
    memset(s, 0, sizeof *s);
    memcpy(s->port, port, sizeof port);
    s->rate = rate; s->dc = 1 - 6.2831853f*5/rate; s->random_value = r;
}

static void connect(LADSPA_Handle h, unsigned long port, LADSPA_Data *p)
{
    struct heat *s = h;
    if (port < PORTS) s->port[port] = p;
}

static void configure(struct heat *s, const float *v)
{
    float rate = s->rate;
    int c = (int)lroundf(v[HE_CIRCUIT]);
    const struct circuit *cc = &circuits[c];
    if (c != s->circuit || !s->ready) {
        design(&s->pre[0], 0, 150, cc->lo_db, .707f, rate);
        design(&s->pre[1], 1, cc->mid_hz, cc->mid_db, .8f, rate);
        design(&s->pre[2], 2, 3000, cc->hi_db, .707f, rate);
    }
    s->circuit = c;
    s->drive = db(cc->gain_min + (cc->gain_max - cc->gain_min)*v[HE_DRIVE]/127)/cc->headroom;
    s->post_lp = coef(s->rate, 1000/(6.2831853f*fminf(cc->post_lp_hz, .45f*rate)));
    s->post_gain = db(cc->post_db)*cc->headroom;
    s->wet = db(v[HE_WET]);
    s->mix = v[HE_MIX]*.01f;
    s->volume = db(v[HE_VOLUME]);
    s->ftype = (int)lroundf(v[HE_FTYPE]);
    s->dirt = v[HE_DIRT] > 0 ? db(v[HE_DIRT]*18/127) : 0;           /* guess: up to +18 dB */
    /* resonance: k = 2 (flat) down past 0 so the top of the range self-oscillates */
    s->reso_k = 2 - 2.04f*powf(v[HE_RESO]/127, .6f);
    s->freq_oct = log2f(20) + v[HE_FREQ]*(log2f(1000)/127);         /* guess: 20 Hz..20 kHz */
    s->pan_oct = v[HE_FREQ_PAN]/64*2;                                 /* guess: +-2 octaves */
    design(&s->eq[0], 0, 200, v[HE_EQ_LO]/64*12, .707f, rate);       /* guess: +-12 dB */
    design(&s->eq[1], 2, 2500, v[HE_EQ_HI]/64*12, .707f, rate);
    s->env_atk = coef(s->rate, .5f*powf(400, v[HE_ENV_ATK]/127));    /* 0.5..200 ms */
    s->env_rel = coef(s->rate, 5*powf(400, v[HE_ENV_REL]/127));      /* 5 ms..2 s */
    s->env_depth = v[HE_ENV_DEPTH]/128*5;                             /* up to 5 octaves */
    s->lfo_depth = v[HE_LFO_DEPTH]/128*4;
    s->lfo_step = .02f*powf(1000, v[HE_LFO_SPEED]/127)/rate;         /* 0.02..20 Hz */
    s->wave = (int)lroundf(v[HE_LFO_WAVE]);
    memcpy(s->values, v, sizeof s->values);
    s->ready = 1;
}

static float lfo_value(struct heat *s)
{
    float p = s->phase;
    switch (s->wave) {
    case 0: return p < .5f ? 4*p - 1 : 3 - 4*p;                       /* TRI */
    case 1: return sinf(6.2831853f*p);                                /* SINE */
    case 2: return p < .5f ? 1 : -1;                                  /* SQR */
    case 3: return 1 - 2*p;                                           /* SAW (falling) */
    case 4: return 2*(expf(4*p) - 1)/(expf(4) - 1) - 1;               /* EXP */
    case 5: return 2*p - 1;                                           /* RAMP */
    default: return s->random_value;                                  /* RND */
    }
}

static void update_filter(struct heat *s, float block_env)
{
    float oct = s->freq_oct + s->env_depth*block_env + s->lfo_depth*lfo_value(s);
    float top = log2f(.45f*s->rate);
    float l = fminf(top, fmaxf(3.3f, oct + s->pan_oct)), r = fminf(top, fmaxf(3.3f, oct - s->pan_oct));
    s->gl = tanf(3.14159265f*exp2f(l)/s->rate);
    s->gr = tanf(3.14159265f*exp2f(r)/s->rate);
    s->k = s->reso_k;
}

static inline float filter(struct heat *s, float x, int c, float g)
{
    int t = s->ftype;
    if (t == 2 || t == 4) {                                           /* 6 dB/oct one-pole */
        float v = (x - s->s1[c])*g/(1 + g), lp = v + s->s1[c];
        s->s1[c] = lp + v;
        return t == 2 ? lp : x - lp;
    }
    /* TPT state variable filter; the soft limit on the band state keeps self-oscillation bounded */
    float k = s->k, a1 = 1/(1 + g*(g + k)), a2 = g*a1, a3 = g*a2;
    float v3 = x - s->s2[c], v1 = a1*s->s1[c] + a2*v3, v2 = s->s2[c] + a2*s->s1[c] + a3*v3;
    float band = 2*v1 - s->s1[c];
    s->s1[c] = band/sqrtf(1 + band*band*(1/16.0f));   /* soft limit near |4| */
    s->s2[c] = 2*v2 - s->s2[c];
    float hp = x - k*v1 - v2;
    switch (t) {
    case 1: return v2;               /* LP2 */
    case 3: return fmaxf(k, .25f)*v1; /* BP: unity at low resonance, rings louder near the top */
    case 5: return hp;               /* HP2 */
    case 6: return x - k*v1;         /* notch */
    default: return v2 - hp;         /* peak */
    }
}

static inline float circuit(struct heat *s, float x, int c)
{
    const struct circuit *cc = &circuits[s->circuit];
    for (int i = 0; i < 3; ++i) x = run_bq(&s->pre[i], x, c);
    float u = x*s->drive + cc->bias, y;
    switch (cc->shape) {
    case TANH: y = adaa_tanh(u, &s->x1[c]); break;
    case HARD: y = adaa_hard(u, &s->x1[c]); break;
    case FUZZ: y = adaa_tanh(u, &s->x1[c]); y += .35f*fabsf(y); break;   /* rectified part: octave up */
    default:
        y = adaa_tanh(u, &s->x1[c]);
        s->inter[c] += .35f*(y - s->inter[c]);
        y = adaa_hard(4*s->inter[c], &s->x2[c]);
    }
    s->post[c] += s->post_lp*(y - s->post[c]);
    float out = s->post[c] - s->dcx[c] + s->dc*s->dcy[c];             /* remove the bias DC */
    s->dcx[c] = s->post[c]; s->dcy[c] = out;
    return out*s->post_gain;
}

static void run(LADSPA_Handle h, unsigned long frames)
{
    struct heat *s = h;
    if (!s->port[IL] || !s->port[IR] || !s->port[OL] || !s->port[OR]) return;
    float v[HE_COUNT];
    for (int i = 0; i < HE_COUNT; ++i) {
        float *p = s->port[FIRST+i];
        const LADSPA_PortRangeHint *r = &hints[FIRST+i];
        v[i] = p && isfinite(*p) ? fminf(r->UpperBound, fmaxf(r->LowerBound, *p)) : defaults[i];
    }
    hs_mod_apply(&s->mod, v, HE_MOD, hints + FIRST, frames, s->rate);
    if (!s->ready || memcmp(v, s->values, sizeof v)) { configure(s, v); s->countdown = 0; }

    for (unsigned long i = 0; i < frames; ++i) {
        float dry[2] = {s->port[IL][i], s->port[IR][i]};
        float peak = fmaxf(fabsf(dry[0]), fabsf(dry[1]));
        s->env += (peak > s->env ? s->env_atk : s->env_rel)*(peak - s->env);
        s->phase += s->lfo_step;
        if (s->phase >= 1) { s->phase -= 1; s->random_value = 2*(rand()/(float)RAND_MAX) - 1; }
        if (s->ftype && --s->countdown <= 0) {
            /* envelope as 0..1 over a 48 dB window */
            float e = s->env > 1e-6f ? fminf(1, fmaxf(0, 1 + 20*log10f(s->env)/48)) : 0;
            update_filter(s, e);
            s->countdown = CONTROL;
        }
        for (int c = 0; c < 2; ++c) {
            float x = circuit(s, dry[c], c);
            if (s->ftype) {
                if (s->dirt) x = tanhf(x*s->dirt)/powf(s->dirt, .7f);
                x = filter(s, x, c, c ? s->gr : s->gl);
            }
            x = run_bq(&s->eq[1], run_bq(&s->eq[0], x, c), c);
            float out = (dry[c] + s->mix*(x*s->wet - dry[c]))*s->volume;
            s->port[c ? OR : OL][i] = out;
        }
    }
    /* flush denormals; recover from a NaN sample */
    float *st[] = {s->x1, s->x2, s->inter, s->post, s->dcx, s->dcy, s->s1, s->s2,
                   s->pre[0].z1, s->pre[0].z2, s->pre[1].z1, s->pre[1].z2, s->pre[2].z1, s->pre[2].z2,
                   s->eq[0].z1, s->eq[0].z2, s->eq[1].z1, s->eq[1].z2};
    for (unsigned k = 0; k < sizeof st / sizeof *st; ++k)
        for (int c = 0; c < 2; ++c)
            if (!(fabsf(st[k][c]) >= 1e-24f && isfinite(st[k][c]))) st[k][c] = 0;
    if (!isfinite(s->env)) s->env = 0;
}

#define AI (LADSPA_PORT_INPUT | LADSPA_PORT_AUDIO)
#define AO (LADSPA_PORT_OUTPUT | LADSPA_PORT_AUDIO)
#define C (LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL)
static const LADSPA_PortDescriptor types[] = {AI,AI,AO,AO,C,C,C,C,C,C,C,C,C,C,C,C,C,C,C,C,C,C,HS_MOD_TYPES};
static const char *const names[] = {
    "Input L", "Input R", "Output L", "Output R",
    "Circuit", "Drive", "Wet (dB)", "Filter type", "Filter freq", "Resonance", "Dirt",
    "EQ low", "EQ high", "Env > freq", "Env attack", "Env release", "LFO > freq", "LFO speed",
    "LFO wave", "Freq pan", "Mix (%)", "Volume (dB)", HS_MOD_NAMES
};
_Static_assert(sizeof names / sizeof *names == PORTS && sizeof types / sizeof *types == PORTS &&
               sizeof defaults / sizeof *defaults == HE_COUNT, "Heat port count");

LADSPA_Descriptor hs_heat_descriptor = {
    .UniqueID = HS_HEAT_ID, .Label = "hs_heat", .Name = "HS Heat",
    .Maker = "SampQOL Lab", .Copyright = "Prototype",
    .Properties = LADSPA_PROPERTY_HARD_RT_CAPABLE,
    .PortCount = PORTS, .PortDescriptors = types, .PortNames = names, .PortRangeHints = hints,
    .instantiate = instantiate, .connect_port = connect, .activate = activate, .run = run, .cleanup = free
};
