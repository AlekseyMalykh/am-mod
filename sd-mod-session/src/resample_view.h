#ifndef MPC_RESAMPLE_VIEW_H
#define MPC_RESAMPLE_VIEW_H
#include "resample_flow.h"
#ifdef __cplusplus
extern "C" {
#endif
struct resample_painter {
    void *user;
    void (*rect)(void*,int x,int y,int width,int height,uint32_t colour);
    void (*text)(void*,const char*,int x,int y,int width,int height,unsigned size,uint32_t colour,int centre);
};
/*320x240, black background, no mirrored pad grid. UI owner only. */
void resample_view_paint(const struct resample_flow *,const struct resample_painter *);
#ifdef __cplusplus
}
#endif
#endif
