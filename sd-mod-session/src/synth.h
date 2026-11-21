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
uint64_t am_synth_pad_info(unsigned pad,unsigned*note); /* 16 Levels pad -> project light value (0 none) and MIDI note */
void am_synth_key(const unsigned char *data,unsigned size); /* note on/off from any channel (MIDI Input mode: Instrument) */
/* Parameters 0..6: Wave, Cutoff, Attack, Decay, Sustain, Release, Level (0..127; Wave 0..3), MIDI channel (0..15 = channel 1..16). */
#define AM_SYNTH_PARAMS 22u /* 11..21: Filter type, Resonance, Env>Cutoff, LFO rate, LFO depth, LFO target (Pitch/Filter/Amp), Unison (1-4), Detune, Spread, Glide, Velocity sens */
int am_synth_direct_out(void); /* synth is heard on the output even when the Input monitor is off */
void am_synth_fader(unsigned value);
void am_synth_keys_cfg(unsigned layout,unsigned root_raw,unsigned scale_raw,unsigned octave,unsigned transpose,const unsigned char*cols6);
void am_synth_keys_get_cfg(unsigned*layout,unsigned*root_raw,unsigned*scale_raw,unsigned*octave,unsigned*transpose,unsigned char*cols16);
/* Key menu values -> semitone root (with transpose 0..48 = -24..+24) and scale table index; raw 0 = project tonic / scale */
int am_synth_keys_get(void);
void am_synth_keys_set(int on); /* pads on the Sample Record screen play the synth */
void am_synth_pad(unsigned pad,int down);
struct am_synth_state{float peak,env_level,bend,mod;unsigned env_stage,voices;unsigned notes[4];}; /* env_stage: 1 attack 2 decay 3 sustain 4 release */
int am_synth_peek(struct am_synth_state*out); /* live state for the Mixer illustration; 0 when the synth is idle */
int am_synth_mic_muted(void); /* synth in use and Mic param Off: the input callback silences the microphone */
unsigned am_synth_param_get(unsigned index);
void am_synth_param_set(unsigned index,unsigned value);
void am_synth_render_add(float *left,float *right,unsigned frames);
#endif
