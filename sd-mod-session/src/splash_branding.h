#ifndef AM_SPLASH_BRANDING_H
#define AM_SPLASH_BRANDING_H
#include <stdint.h>
#include <string.h>
#include "mod_build.h"
#include "branding_font_aa.h"
/* Anti-aliased, centred ASCII text blended over the splash background.
 * Glyphs come from build_branding_font.py (monospace, fixed cell). */
static inline void am_splash_text_aa(uint32_t*p,unsigned pitch,const char*s,unsigned y,const uint8_t (*font)[1],
                                     unsigned cw,unsigned ch,unsigned stride,uint32_t colour){
 size_t length=strlen(s);if(!length||length*cw>320||y+ch>240)return;
 unsigned left=(320-(unsigned)length*cw)/2;
 for(unsigned i=0;i<length;++i){unsigned c=(unsigned char)s[i];if(c<32||c>126)continue;
  const uint8_t*g=(const uint8_t*)font+(size_t)(c-32)*stride;
  for(unsigned row=0;row<ch;++row){uint32_t*line=(uint32_t*)((char*)p+(y+row)*pitch);
   for(unsigned col=0;col<cw;++col){unsigned a=g[row*cw+col];if(!a)continue;
    uint32_t*px=&line[left+i*cw+col],b=*px,out=0xff000000u;
    for(unsigned sh=0;sh<24;sh+=8){unsigned fg=(colour>>sh)&255,bg=(b>>sh)&255;out|=((fg*a+bg*(255-a)+127)/255)<<sh;}
    *px=out;}}
 }
}
static inline void am_splash_branding(uint32_t*p,unsigned pitch){
 if(!p||pitch<1280||pitch%4)return;
 am_splash_text_aa(p,pitch,AM_MOD_SPLASH_VERSION,198,(const uint8_t(*)[1])am_font_version,AM_FONT_VERSION_W,AM_FONT_VERSION_H,
                   AM_FONT_VERSION_W*AM_FONT_VERSION_H,0xffd4d8dfu);
 am_splash_text_aa(p,pitch,AM_MOD_REPO_LABEL,219,(const uint8_t(*)[1])am_font_url,AM_FONT_URL_W,AM_FONT_URL_H,
                   AM_FONT_URL_W*AM_FONT_URL_H,0xffa8b3c2u);
}
#endif
