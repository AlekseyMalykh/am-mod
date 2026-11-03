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
#ifdef __cplusplus
}
#endif
#endif
