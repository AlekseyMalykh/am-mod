#ifndef MPC_MOD_FX_EQ_EDITOR_H
#define MPC_MOD_FX_EQ_EDITOR_H
#include "fx_eq.h"
#include "resample_view.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Shared editor: each caller owns its own draft. No native model/audio
 * pointer, hidden assignment, allocation, or fake spectrum. */
struct fx_eq_editor {
    struct fx_eq_config config;
    double rate;
    unsigned selected,points,revision,paint_revision,shifted;
    int curve[277];
    char message[48];
};
enum { EQ_EDITOR_NONE=0,EQ_EDITOR_REDRAW=1,EQ_EDITOR_CHANGED=2,EQ_EDITOR_CLOSE=3 };
int fx_eq_editor_open(struct fx_eq_editor*,const struct fx_eq_config*,double rate);
int fx_eq_editor_button(struct fx_eq_editor*,unsigned button,int shift);
int fx_eq_editor_control(struct fx_eq_editor*,unsigned cc,unsigned value,int shift);
void fx_eq_editor_paint(struct fx_eq_editor*,const struct resample_painter*);
#ifdef __cplusplus
}
#endif
#endif
