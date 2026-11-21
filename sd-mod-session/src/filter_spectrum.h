#ifndef MPC_FILTER_SPECTRUM_H
#define MPC_FILTER_SPECTRUM_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Install before native main: suppress only waveform sample rasterisation
 * inside this Sample view's Filter pages, leaving native graph overlays. */
int filter_spectrum_install(uintptr_t base,int (*enabled)(void *sample));
void filter_spectrum_tint_provider(int (*provider)(uint32_t *rgb));
/* Draw at the native waveform stage, before its filter/envelope paths. */
void filter_spectrum_painter(void (*painter)(void *view,void *graphics));
#ifdef __cplusplus
}
#endif
#endif
