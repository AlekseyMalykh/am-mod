#ifndef AM_MIXER_SAMPLE_FEEDBACK_H
#define AM_MIXER_SAMPLE_FEEDBACK_H
#include "mixer_model.h"

enum mixer_sample_gate {
    MIXER_SAMPLE_AUDIBLE,
    MIXER_SAMPLE_MUTED,
    MIXER_SAMPLE_SOLO_EXCLUDED
};

/* Same precedence as the Track and Direct bus gates. */
static inline enum mixer_sample_gate mixer_sample_project_gate(
    const struct mixer_project *project,unsigned pad)
{
    if(!project||pad>=MIXER_PADS)return MIXER_SAMPLE_AUDIBLE;
    unsigned output=project->output[pad];
    if(output>MIXER_TRACKS)return MIXER_SAMPLE_AUDIBLE;
    if(output&&project->track[output-1].mix.mute)return MIXER_SAMPLE_MUTED;
    if(output&&project->track[output-1].mix.solo)return MIXER_SAMPLE_AUDIBLE;
    for(unsigned i=0;i<MIXER_TRACKS;++i)
        if(project->track[i].mix.solo)return MIXER_SAMPLE_SOLO_EXCLUDED;
    return MIXER_SAMPLE_AUDIBLE;
}

/* UI thread only. Reads the audio-confirmed Mixer model. */
enum mixer_sample_gate mixer_phone_sample_gate(unsigned pad);
#endif
