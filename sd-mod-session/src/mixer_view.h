#ifndef AM_MIXER_VIEW_H
#define AM_MIXER_VIEW_H
#include "mixer_model.h"
#include "mixer_meters.h"
#include "resample_view.h"
#ifdef __cplusplus
extern "C" {
#endif
enum mixer_page {MIXER_OVERVIEW,MIXER_MIX,MIXER_FX,MIXER_ROUTE,MIXER_KNOB_FX,MIXER_ADVANCED,MIXER_ASSIGN,MIXER_INSPECT};
struct mixer_view_state {
    enum mixer_page page;
    unsigned selected,primary,master,cursor,scroll,bank,fx_slot;
    uint64_t part_id,epoch,now_ms,meter_ms;
    const struct mixer_project*project;
    const struct mixer_meter_snapshot*meter;
    unsigned pending;
    unsigned sound,fx_catalog,parameter_page,pickup;
    unsigned shifted;
    uint64_t present[2];
    const char*message;
};
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
