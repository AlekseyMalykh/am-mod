#ifndef AM_MIXER_LEDS_H
#define AM_MIXER_LEDS_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Cold install before native lighting/input threads start, pinned firmware. */
int mixer_leds_install(uintptr_t image);
/* One UI publisher. Program is only an identity here. Native bank must agree
 * before publishing. Colours come from confirmed controller routing. */
void mixer_leds_publish(void *program,unsigned bank,const uint32_t rgb[16]);
void mixer_leds_disable(void);
/* Revoke native observations across Program replacement/address reuse. */
void mixer_leds_reset(void);
/* Called only for native RGB events, never our own timer resend. Unknown
 * sources revoke background permission; no brightness/colour heuristics. */
void mixer_leds_native_rgb(unsigned physical_pad);
/* Atomic snapshot only, no native model/Component calls. While enabled this
 * also claims critical/unknown pads with their unchanged native RGB so lower
 * custom palettes cannot overwrite them. */
int mixer_leds_light(unsigned pad,uint32_t native_rgb,uint64_t *out);
#ifdef __cplusplus
}
#endif
#endif
