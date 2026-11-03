#include <assert.h>
#include <stdarg.h>
#include <sys/ioctl.h>
static int test_ioctl(int fd,unsigned long request,...);
#define ioctl test_ioctl
#define main splash_main
#include "src/early_splash.c"
#undef main
#undef ioctl
static unsigned reset_calls;static int reject_unblank;
static int test_ioctl(int fd,unsigned long request,...){
 assert(fd==7&&request==FBIOBLANK);va_list args;va_start(args,request);int value=va_arg(args,int);va_end(args);
 assert(value==(reset_calls%2?FB_BLANK_UNBLANK:FB_BLANK_NORMAL));++reset_calls;
 if(reject_unblank&&value==FB_BLANK_UNBLANK){errno=EIO;return -1;}return 0;
}
int main(void){
 assert(present(7)&&reset_calls==2);reject_unblank=1;assert(!present(7)&&reset_calls==4);
 assert(decode());unsigned bright=0;for(unsigned i=0;i<320*240;++i)bright+=(pixels[i]&0xffffff)>0x808080;assert(bright>100&&bright<20000);
 struct fb_var_screeninfo v={.xres=320,.yres=240,.xres_virtual=320,.yres_virtual=240,.bits_per_pixel=16,.red={11,5,0},.green={5,6,0},.blue={0,5,0}};
 struct fb_fix_screeninfo f={.smem_len=320*240*2,.type=FB_TYPE_PACKED_PIXELS,.visual=FB_VISUAL_TRUECOLOR,.line_length=640};assert(layout(&v,&f));
 assert(channel(255,v.red)==0xf800&&channel(255,v.green)==0x7e0&&channel(255,v.blue)==0x1f);
 v.xoffset=1;assert(!layout(&v,&f));v.xoffset=0;v.green.offset=11;assert(!layout(&v,&f));v.green.offset=5;
 f.smem_len--;assert(!layout(&v,&f));f.smem_len++;
 v.bits_per_pixel=32;v.red=(struct fb_bitfield){16,8,0};v.green=(struct fb_bitfield){8,8,0};v.blue=(struct fb_bitfield){0,8,0};v.transp=(struct fb_bitfield){24,8,0};f.line_length=1280;f.smem_len=320*240*4;assert(layout(&v,&f));
 v.transp.offset=32;assert(!layout(&v,&f));v.transp.length=0;assert(layout(&v,&f));
 v.xres_virtual=UINT32_MAX;v.xoffset=UINT32_MAX-320;assert(!layout(&v,&f));
 puts("PASS blank/unblank occurs in order before frame transfer; rejected display reset is an error; own Am MOD image, RGB565/ARGB8888 conversion, stride/offset/memory bounds and overlapping-channel rejection");return 0;
}
