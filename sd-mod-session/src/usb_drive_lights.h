#ifndef AM_USB_DRIVE_LIGHTS_H
#define AM_USB_DRIVE_LIGHTS_H
#include <stdint.h>
/* Pinned Sample MCU sender159de9c/14e4794 selects product5c; the native
 * encoder23b0070 emits this 12-byte Akai RGB frame with 7-bit components. */
static inline uint64_t usb_drive_amber_light(void){return UINT64_C(0x3f80000000ffc247);}
static inline void usb_drive_amber_frame(unsigned char bytes[16*12]){
 for(unsigned pad=0;pad<16;++pad){unsigned char *p=bytes+pad*12;
  p[0]=0xf0;p[1]=0x47;p[2]=0x7f;p[3]=0x5c;p[4]=0x65;p[5]=0;p[6]=4;
  p[7]=(unsigned char)pad;p[8]=0x7f;p[9]=0x61;p[10]=0x23;p[11]=0xf7;
 }
}
#endif
