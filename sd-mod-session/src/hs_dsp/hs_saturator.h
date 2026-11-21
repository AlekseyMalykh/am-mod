#ifndef HS_SATURATOR_H
#define HS_SATURATOR_H

#include <stddef.h>

struct hs_saturator {
    float drive;
    float tone_coefficient;
    float mix;
    float tone_state[2];
};

/* Configure outside the audio callback. Drive is in dB, tone and mix are 0..1. */
void hs_saturator_configure(struct hs_saturator *state, float sample_rate,
                            float drive_db, float tone, float mix);
/* Call with two distinct, writable channel buffers. Zero-initialize state before first use. */
void hs_saturator_process(struct hs_saturator *state, float *left, float *right,
                          size_t frames);

#endif
