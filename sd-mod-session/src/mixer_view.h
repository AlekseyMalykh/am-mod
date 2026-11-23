#ifndef AM_MIXER_VIEW_H
#define AM_MIXER_VIEW_H
#include "mixer_model.h"
#include "mixer_buses.h"
#include "resample_view.h"
#ifdef __cplusplus
extern "C" {
#endif
#define MIXER_FX_SLOTS 4u
#define MIXER_EQ_POINTS 4u
static inline uint32_t mixer_fx_slot_rgb(unsigned slot)
{
    static const uint32_t hue[MIXER_FX_SLOTS]={0xffc247u,0x9e68ffu,0x00c8ffu,0xff54afu};
    return slot<MIXER_FX_SLOTS?hue[slot]:0;
}
enum mixer_page {MIXER_OVERVIEW,MIXER_MIX,MIXER_FX,MIXER_ROUTE,MIXER_KNOB_FX,MIXER_ADVANCED,MIXER_ASSIGN,MIXER_INSPECT};
enum mixer_parameter {PARAM_LEVEL,PARAM_PAN,PARAM_SEND,PARAM_COLOUR,PARAMS};
enum mixer_route_tab {MIXER_ROUTE_PADS,MIXER_ROUTE_SENDS,MIXER_ROUTE_KEYS,MIXER_ROUTE_TABS};
enum mixer_fx_delete {MIXER_DELETE_NONE,MIXER_DELETE_EFFECT,MIXER_DELETE_POINT};
/* The Sample-mode master compressor (Shift+Pad 5) as the Master FX page sees it. */
#define MIXER_COMP_SLOT 7u /* fx_slot value of the pinned row on Master */
struct mixer_sample_comp {
    unsigned visible,available,type,bypass,warm; /* type 0 Duck, 1 OptoCat */
    float threshold,ratio,gain,attack,release,knee,reduction,level; /* level: input RMS dB for the cat */
};
struct mixer_view_state {
    enum mixer_page page;
    unsigned selected,primary,master,cursor,scroll,bank,fx_slot;
    uint64_t part_id,epoch,now_ms,meter_ms,revision;
    const struct mixer_project*project;
    const struct mixer_meter_snapshot*meter;
    unsigned pending;
    unsigned route_tab,route_focus;
    unsigned sound,fx_catalog,parameter_page,pickup,pad_lock;
    unsigned catalog_filter; /* Add Effect category: 0 = All */
    unsigned shifted,colour_edit,fx_editing,fx_parameters,fx_delete;
    unsigned sequence_muted; /* UI-owner snapshot: local pad mutes in the current sequence. */
    uint64_t present[2];
    const char*message;
    const char*name_edit,*name_title;
    uint64_t name_elapsed;
    unsigned name_pads;
    const struct mixer_fx_monitor*fx_signal;
    struct mixer_sample_comp comp; /* stock Sample master compressor, shown as a pinned Master FX row */
    unsigned fx_illustrated; /* 0 Classic; set by the host from the card setting */
    unsigned input_tab; /* Input page: 1 FX, 2 ROUTE (B1 is Back) */
    unsigned input_edit; /* ROUTE: the selected row is being edited with the encoder */
    unsigned input_tuner; /* Input: the full-screen tuner window is open */
    unsigned synth_pick,synth_prev,synth_transpose,synth_colour,synth_role; unsigned char synth_cols[6]; /* project colour slots: tonic, scale, other, original, V, IV */ /* transpose 0..24 = -12..+12; colour index; prev: page before the 16 Levels settings (B2) */
     /* Loop held: pads 1-3 jump between synth pages */
    unsigned synth_hold,synth_hold_note,synth_page,synth_shift,synth_cursor,synth_keys,synth_layout,synth_root,synth_scale,synth_octave; /* synth_keys: 16 Levels keyboard on pads */ /* trial synth window (Mixer pad 13) */
    double rate;
};
/* Loop + pad: synth modules sit on pads 13-16 (bottom row) and 9 (0-based 12..15, 8); -1 for other pads.
 * Modules: 0 Tone, 1 Envelope, 2 Keys, 3 Filter + LFO, 4 Voices. A module owns one or two K pages (see mixer_view_synth_slot). */
#define MIXER_SYNTH_PAGES 9u
#define MIXER_SYNTH_KEYS_PAGE 4u
static inline int mixer_view_synth_pad_page(unsigned pad){return pad>=12&&pad<16?(int)pad-12:pad==8?4:-1;}
static inline unsigned mixer_view_synth_module(unsigned page){static const unsigned char m[MIXER_SYNTH_PAGES]={0,0,1,1,2,3,3,4,4};return m[page%MIXER_SYNTH_PAGES];}
static inline unsigned mixer_view_synth_first_page(unsigned module){static const unsigned char f[5]={0,2,4,5,7};return f[module%5u];}
/* the other page of the same module (Keys has one) */
static inline unsigned mixer_view_synth_sibling(unsigned page){static const unsigned char n[MIXER_SYNTH_PAGES]={1,0,3,2,4,6,5,8,7};return n[page%MIXER_SYNTH_PAGES];}
/* synth param id (0-9), key setting (10-16) or Filter/LFO/Voices id (17-27 = engine param id-6) on a K knob, -1 none */
float mixer_view_synth_frac(const struct mixer_view_state*s,int id); /* 0..1 position of a synth slot, for K takeover */
static inline int mixer_view_synth_slot(unsigned page,unsigned knob,unsigned shift)
{
    /* ids: 0-9 synth, 10-16 keys, 17-27 filter/lfo/voices, 28-41 oscillators/filter env/extras. Often-used on the knob, rarely-used on Shift. */
    static const signed char slot[MIXER_SYNTH_PAGES][3][2]={
        {{0,28},{29,30},{31,34}},      /* 0 Osc: Wave|Osc2 Wave, Osc2 Mix|Osc2 Semi, Sub|Osc2 Fine */
        {{6,33},{32,-1},{35,-1}},      /* 1 Mix: Level|Drive, Noise, Bend range */
        {{2,3},{4,5},{27,-1}},         /* 2 Amp env: Attack|Decay, Sustain|Release, Velocity */
        {{36,37},{38,39},{19,-1}},     /* 3 Filter env: Attack|Decay, Sustain|Release, Env > Cutoff */
        {{14,10},{11,16},{12,15}},     /* 4 Keys: Transpose|Layout, Key|Role, Scale|Colour (as the Tune tab) */
        {{1,17},{18,-1},{40,-1}},      /* 5 Filter: Cutoff|Type, Resonance, Key track */
        {{20,41},{21,-1},{22,-1}},     /* 6 LFO: Rate|Shape, Depth, Target */
        {{23,24},{25,-1},{26,-1}},     /* 7 Voices: Unison|Detune, Spread, Glide */
        {{9,35},{7,8},{-1,-1}}};       /* 8 Controller: Fader (Off/Mod/Pitch)|Bend range, MIDI channel|Mic */
    return page<MIXER_SYNTH_PAGES&&knob<3&&shift<2?slot[page][knob][shift]:-1;
}
/* 16 Levels colour roles per layout (as the project Lights page): Mono/Piano: Original; Scale: Tonic, Scale, Other, Original; Degrees: I, Key notes, IV, V, Original */
/* Input ROUTE tab order: Source, Mode (Rear/USB only), Monitor, Threshold, Record via, Record track, Record FX, Tuner, MIDI In; ids as route_row. */
static inline unsigned mixer_view_input_rows(int rear){return rear?9u:8u;}
static inline unsigned mixer_view_input_row(unsigned pos,int rear){
    static const unsigned char n[8]={5,1,7,2,3,4,0,8},r[9]={5,6,1,7,2,3,4,0,8};
    return rear?r[pos%9u]:n[pos&7u];
}
static inline unsigned mixer_view_synth_roles(unsigned layout){static const unsigned char n[4]={1,1,4,5};return n[layout%4];}
/* index into synth_cols for role r of a layout */
static inline unsigned mixer_view_synth_field(unsigned layout,unsigned r)
{
    static const unsigned char scale[4]={0,1,2,3},degrees[5]={0,1,5,4,3};
    layout%=4;if(layout<2)return 3u;
    return layout==3?degrees[r%5]:scale[r%4];
}
static inline void mixer_view_synth_defaults(struct mixer_view_state*s)
{
    static const unsigned char d[6]={5,1,10,4,8,3};
    for(unsigned i=0;i<6;++i)s->synth_cols[i]=d[i];
}
static inline const struct mixer_channel*mixer_view_selected(const struct mixer_view_state*s)
{return s->master==2?&s->project->input:s->master?&s->project->master:s->project->track+s->primary;}
/* Routing Pads addresses sounds; Sends/Sidechain address Mixer tracks. */
static inline int mixer_view_sound_pads(const struct mixer_view_state *s)
{return s->page==MIXER_ASSIGN||(s->page==MIXER_INSPECT&&s->route_tab==MIXER_ROUTE_PADS);}
/* UI-owner only. Existing 320x240 rect/text painter, no image/framebuffer,
 * global mode state or native object ownership. Meter age>250ms => N/A. */
void mixer_view_overview(const struct mixer_view_state*,const struct resample_painter*);
void mixer_view_channel_strip(const struct mixer_view_state*,unsigned track,int x,int width,const struct resample_painter*);
void mixer_view_context(const struct mixer_view_state*,const struct resample_painter*);
void mixer_view_paint(const struct mixer_view_state*,const struct resample_painter*);
/* Full-screen tuner (320x240): note row, straight +-50 cent scale, moving needle, Hz. Shared by the Mixer Input page and Sample Record. */
void mixer_view_tuner(const struct resample_painter*);
/* Same window for another source: tag (bottom right), have/hz (0 = none), idle text, hint (bottom left). */
void mixer_view_tuner_ex(const struct resample_painter*,const char*tag,int have,float hz,const char*idle,const char*hint);
/* Dynamic-only draw: touches meter/Peak bounds, no faders/context/footer.
 * mask bit0..7 Track, bit8 Master; caller combines changed bounds on UI tick. */
void mixer_view_meters(const struct mixer_view_state*,unsigned mask,const struct resample_painter*);
int mixer_view_gain_y(float db); /* shared exact unity scale:102..177 */
float mixer_view_gain_db(int y);
#ifdef __cplusplus
}
#endif
#endif
