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
    uint64_t present[2];
    const char*message;
    const char*name_edit;
    uint64_t name_elapsed;
    unsigned name_pads,name_clear_armed;
    const struct mixer_fx_monitor*fx_signal;
    struct mixer_sample_comp comp; /* stock Sample master compressor, shown as a pinned Master FX row */
    unsigned fx_illustrated; /* 0 Classic; set by the host from the card setting */
    double rate;
};
/* Routing Pads addresses sounds; Sends/Sidechain address Mixer tracks. */
static inline int mixer_view_sound_pads(const struct mixer_view_state *s)
{return s->page==MIXER_ASSIGN||(s->page==MIXER_INSPECT&&s->route_tab==MIXER_ROUTE_PADS);}
/* UI-owner only. Existing 320x240 rect/text painter, no image/framebuffer,
 * global mode state or native object ownership. Meter age>250ms => N/A. */
void mixer_view_overview(const struct mixer_view_state*,const struct resample_painter*);
void mixer_view_channel_strip(const struct mixer_view_state*,unsigned track,int x,int width,const struct resample_painter*);
void mixer_view_context(const struct mixer_view_state*,const struct resample_painter*);
void mixer_view_paint(const struct mixer_view_state*,const struct resample_painter*);
/* Dynamic-only draw: touches meter/Peak bounds, no faders/context/footer.
 * mask bit0..7 Track, bit8 Master; caller combines changed bounds on UI tick. */
void mixer_view_meters(const struct mixer_view_state*,unsigned mask,const struct resample_painter*);
int mixer_view_gain_y(float db); /* shared exact unity scale:102..177 */
float mixer_view_gain_db(int y);
#ifdef __cplusplus
}
#endif
#endif
