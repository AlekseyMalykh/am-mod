#include "hs_saturator.h"

#include <math.h>

static float clamp(float value, float low, float high)
{
    return fminf(high, fmaxf(low, value));
}

void hs_saturator_configure(struct hs_saturator *state, float sample_rate,
                            float drive_db, float tone, float mix)
{
    sample_rate = clamp(sample_rate, 8000, 192000);
    float cutoff = 900.0f * exp2f(4.0f * clamp(tone, 0, 1));
    cutoff = fminf(cutoff, sample_rate * 0.45f);
    state->drive = powf(10.0f, clamp(drive_db, 0, 24) / 20.0f);
    state->tone_coefficient = 1.0f - expf(-6.2831853f * cutoff / sample_rate);
    state->mix = clamp(mix, 0, 1);
}

void hs_saturator_process(struct hs_saturator *state, float *left, float *right,
                          size_t frames)
{
    if (state->mix == 0 || state->drive <= 1) {
        state->tone_state[0] = state->tone_state[1] = 0;
        return;
    }
    float *const channels[] = {left, right};
    for (int channel = 0; channel < 2; ++channel) {
        float *samples = channels[channel];
        float colored = state->tone_state[channel];
        for (size_t i = 0; i < frames; ++i) {
            float dry = samples[i];
            float driven = dry * state->drive;
            float clipped = driven / (1.0f + fabsf(driven));
            colored += state->tone_coefficient * (clipped - dry - colored);
            samples[i] = dry + state->mix * colored;
        }
        state->tone_state[channel] = fabsf(colored) > 1e-20f ? colored : 0;
    }
}
