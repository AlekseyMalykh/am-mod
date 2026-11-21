#ifndef AM_SYNTH_H
#define AM_SYNTH_H
/* Minimal polyphonic synth (trial). Notes on the synth MIDI channel (default 16) play it; the output is added to the
 * shared input chain so Input FX, monitoring and recording treat it as an input source.
 * CC on channel 16: 70 wave (0 sine, 1 saw, 2 square, 3 triangle), 71 cutoff, 72 release,
 * 73 attack, 75 decay, 76 sustain, 7 level. */
void am_synth_note(unsigned note,unsigned velocity,int on);
void am_synth_cc(unsigned cc,unsigned value);
void am_synth_midi(const unsigned char *data,unsigned size); /* raw message, channel 16 only */
int am_synth_active(void);
/* Parameters 0..6: Wave, Cutoff, Attack, Decay, Sustain, Release, Level (0..127; Wave 0..3), MIDI channel (0..15 = channel 1..16). */
#define AM_SYNTH_PARAMS 8u
unsigned am_synth_param_get(unsigned index);
void am_synth_param_set(unsigned index,unsigned value);
void am_synth_render_add(float *left,float *right,unsigned frames);
#endif
