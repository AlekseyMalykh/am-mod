/* One bounded framebuffer write before the application starts. Uses our own
 * Am MOD image. No display daemon, audio, pad messages or background painter. */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/stat.h>
struct SplashRun { unsigned count; uint32_t argb; };
#define constexpr static const
#include "am_mod_splash.inc"
#undef constexpr
static uint32_t pixels[320*240];
static unsigned char row[320*4];
static int failed(const char*stage){fprintf(stderr,"EARLY_SPLASH FAILED stage=%s errno=%d\n",stage,errno);return 1;}
static uint32_t channel(unsigned v,struct fb_bitfield f){return f.length?((v*((1u<<f.length)-1)+127)/255)<<f.offset:0;}
static int layout(struct fb_var_screeninfo*v,struct fb_fix_screeninfo*f){
 if(v->xres!=320||v->yres!=240||v->xres_virtual<320||v->yres_virtual<240||
    v->xoffset>v->xres_virtual-320||v->yoffset>v->yres_virtual-240||
    (v->bits_per_pixel!=16&&v->bits_per_pixel!=32)||f->type!=FB_TYPE_PACKED_PIXELS||
    f->visual!=FB_VISUAL_TRUECOLOR||f->line_length<(uint64_t)(v->xoffset+320)*(v->bits_per_pixel/8)||
    (uint64_t)(v->yoffset+240)*f->line_length>f->smem_len)return 0;
 struct fb_bitfield fields[]={v->red,v->green,v->blue,v->transp};uint32_t used=0;
 for(unsigned i=0;i<4;++i){struct fb_bitfield s=fields[i];
  if(s.msb_right||s.length>8||s.offset>v->bits_per_pixel||s.offset+s.length>v->bits_per_pixel||(!s.length&&i<3))return 0;
  uint32_t mask=s.length?((1u<<s.length)-1)<<s.offset:0;if(used&mask)return 0;used|=mask;
 }return 1;
}
static int decode(void){
 unsigned n=0;for(unsigned i=0;i<sizeof(am_splash_runs)/sizeof(*am_splash_runs);++i){
  if(!am_splash_runs[i].count||am_splash_runs[i].count>320*240-n)return 0;
  for(unsigned j=0;j<am_splash_runs[i].count;++j)pixels[n++]=am_splash_runs[i].argb;
 }return n==320*240;
}
static int present(int fd){
 /* Stock uses DRM. A plain fb write may target a disabled fbdev plane after
  * the application releases DRM master. Match the blank/unblank cycle in
  * the public SampQOL launcher BEFORE transferring our own pixels. */
 int blank=ioctl(fd,FBIOBLANK,FB_BLANK_NORMAL);int blank_errno=errno;
 int unblank=ioctl(fd,FBIOBLANK,FB_BLANK_UNBLANK);int unblank_errno=errno;
 fprintf(stderr,"EARLY_SPLASH RESET blank=%d blank_errno=%d unblank=%d unblank_errno=%d\n",blank,blank<0?blank_errno:0,unblank,unblank<0?unblank_errno:0);
 return !blank&&!unblank;
}
int main(int argc,char**argv){
 if(argc!=2||strcmp(argv[1],"--show"))return 2;
 int fd=open("/dev/fb0",O_RDWR|O_CLOEXEC|O_NOFOLLOW);if(fd<0)return failed("open_fb0");
 struct fb_var_screeninfo v;struct fb_fix_screeninfo f;
 if(ioctl(fd,FBIOGET_VSCREENINFO,&v)){int result=failed("variable_info");close(fd);return result;}
 if(ioctl(fd,FBIOGET_FSCREENINFO,&f)){int result=failed("fixed_info");close(fd);return result;}
 fprintf(stderr,"EARLY_SPLASH MODE id=%.16s res=%ux%u virtual=%ux%u offset=%u,%u bpp=%u stride=%u bytes=%u type=%u visual=%u rgba=%u:%u,%u:%u,%u:%u,%u:%u\n",f.id,v.xres,v.yres,v.xres_virtual,v.yres_virtual,v.xoffset,v.yoffset,v.bits_per_pixel,f.line_length,f.smem_len,f.type,f.visual,v.red.offset,v.red.length,v.green.offset,v.green.length,v.blue.offset,v.blue.length,v.transp.offset,v.transp.length);
 if(!layout(&v,&f)){errno=EINVAL;int result=failed("unsupported_layout");close(fd);return result;}
 if(!decode()){errno=EINVAL;int result=failed("invalid_image");close(fd);return result;}
 if(!present(fd)){close(fd);return failed("blank_cycle");}
 unsigned bytes=v.bits_per_pixel/8;
 for(unsigned y=0;y<240;++y){
  for(unsigned x=0;x<320;++x){uint32_t rgb=pixels[y*320+x];uint32_t value=channel(rgb>>16&255,v.red)|channel(rgb>>8&255,v.green)|channel(rgb&255,v.blue)|channel(255,v.transp);memcpy(row+x*bytes,&value,bytes);}
  size_t done=0;off_t at=(off_t)(y+v.yoffset)*f.line_length+v.xoffset*bytes;
  if(lseek(fd,at,SEEK_SET)!=at){int result=failed("seek_row");close(fd);return result;}
  while(done<320*bytes){ssize_t n=write(fd,row+done,320*bytes-done);if(n<0&&errno==EINTR)continue;if(n<=0){int result=failed("write_row");close(fd);return result;}done+=(size_t)n;}
 }
 close(fd);puts("EARLY_SPLASH FRAME_WRITTEN Am_MOD physical_visibility_requires_device_check");return 0;
}
