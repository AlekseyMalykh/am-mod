#ifndef AM_USB_AUDIO_H
#define AM_USB_AUDIO_H
#include <stddef.h>
#define AM_USB_CHANNELS 18u
#define AM_USB_CAPACITY 8192u
/* One live render producer and one USB playback consumer. No allocation,
 * locks, device access or waiting on the render path. Whole blocks drop on
 * overflow; reconnect/project changes discard old audio. */
void usb_audio_capture(const float *const channels[AM_USB_CHANNELS],unsigned frames);
void usb_audio_reset(void);
void usb_audio_enable(unsigned enabled);
unsigned usb_audio_enabled(void);
unsigned usb_audio_peek(float *interleaved,unsigned frames);
void usb_audio_consume(unsigned frames);
#endif
