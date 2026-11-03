#ifndef AM_SPLASH_BRANDING_H
#define AM_SPLASH_BRANDING_H
#include <stdint.h>
#include <string.h>
#include "mod_build.h"
#include "branding_font.h"
static inline void am_splash_text(uint32_t*p,unsigned pitch,const char*s,unsigned y,unsigned sx,unsigned sy,uint32_t colour){
 size_t length=strlen(s);if(!length||length>320/(8*sx)||y+8*sy>240)return;
 unsigned left=(320-(unsigned)length*8*sx)/2;
 for(unsigned i=0;i<length;++i){unsigned c=(unsigned char)s[i];if(c>=128)continue;
  for(unsigned row=0;row<8;++row)for(unsigned bit=0;bit<8;++bit)if(am_font8x8[c][row]&(1u<<bit))
   for(unsigned dy=0;dy<sy;++dy)for(unsigned dx=0;dx<sx;++dx)
    ((uint32_t*)((char*)p+(y+row*sy+dy)*pitch))[left+i*8*sx+bit*sx+dx]=colour;
 }
}
static inline void am_splash_branding(uint32_t*p,unsigned pitch){
 if(!p||pitch<1280||pitch%4)return;
 am_splash_text(p,pitch,AM_MOD_VERSION,201,1,1,0xffd4d8dfu);
 am_splash_text(p,pitch,AM_MOD_REPO_LABEL,222,1,1,0xffa8b3c2u);
}
#endif
