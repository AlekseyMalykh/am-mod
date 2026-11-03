#ifndef AM_MOD_STARTUP_SPLASH_H
#define AM_MOD_STARTUP_SPLASH_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* UI thread only. Decoration never drives loading, input or its timing. */
int startup_splash_draw(uintptr_t base,void *graphics);
#ifdef __cplusplus
}
#endif
#endif
