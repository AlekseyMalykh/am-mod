#define _GNU_SOURCE
#include "src/screen_mirror_drm.c"
#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/stat.h>
static uint32_t gpu[2][MIRROR_WIDTH*MIRROR_HEIGHT],cpu[2][MIRROR_WIDTH*MIRROR_HEIGHT];
static uint32_t presented=100;
static unsigned begins,ends,maps,queries;
static int change_during_copy,fail_map,fail_start,fail_end;
int __wrap_ioctl(int fd,unsigned long request,...){
 va_list args;va_start(args,request);void*arg=va_arg(args,void*);va_end(args);
 if(request==DRM_IOCTL_MODE_GETCRTC){struct drm_mode_crtc*c=arg;assert(fd==20||fd==21);c->mode_valid=1;c->fb_id=presented;++queries;return 0;}
 if(request==DRM_IOCTL_PRIME_HANDLE_TO_FD){struct drm_prime_handle*p=arg;assert(p->handle==10||p->handle==11);p->fd=(int)p->handle+30;return 0;}
 if(request==DMA_BUF_IOCTL_SYNC){struct dma_buf_sync*s=arg;assert(fd==40||fd==41);assert(s->flags&DMA_BUF_SYNC_READ);assert(!(s->flags&DMA_BUF_SYNC_WRITE));
  if(s->flags&DMA_BUF_SYNC_END){++ends;return fail_end?-1:0;}
  if(fail_start)return -1;
  ++begins;memcpy(cpu[fd-40],gpu[fd-40],sizeof(cpu[0]));return 0;
 }
 if(request==DRM_IOCTL_MODE_MAP_DUMB){struct drm_mode_map_dumb*m=arg;if(fail_map)return -1;m->offset=(uint64_t)(fd-20);return 0;}
 assert(!"unexpected DRM ioctl");return -1;
}
void*__real_mmap(void*,size_t,int,int,int,off_t);
int __real_munmap(void*,size_t);
void*__wrap_mmap(void*p,size_t n,int prot,int flags,int fd,off_t offset){
 if(fd==20||fd==21){(void)offset;assert(n==sizeof(cpu[0])&&prot==PROT_READ&&flags==MAP_SHARED);++maps;
  if(change_during_copy)presented=101;
  return cpu[fd-20];}
 return __real_mmap(p,n,prot,flags,fd,offset);
}
int __wrap_munmap(void*p,size_t n){if(p==cpu[0]||p==cpu[1]){assert(n==sizeof(cpu[0]));return 0;}return __real_munmap(p,n);}
static void capture(void){last_frame=0;capture_buffer(10,35);}
static void pixel(unsigned r,unsigned g,unsigned b){
 FILE*f=fopen(SCREEN_MIRROR_FRAME,"rb");assert(f);unsigned char bmp[MIRROR_BMP_BYTES];assert(fread(bmp,1,sizeof(bmp),f)==sizeof(bmp));fclose(f);
 assert(bmp[54]==b&&bmp[55]==g&&bmp[56]==r);
}
int main(void){
 unlink(SCREEN_MIRROR_FLAG);unlink(SCREEN_MIRROR_FRAME);
 FILE*f=fopen(SCREEN_MIRROR_FLAG,"w");assert(f);fclose(f);
 buffers[0]=(struct mirror_buffer){10,20,-1,100,10,1280,32};buffers[1]=(struct mirror_buffer){10,21,-1,101,11,1280,32};
 for(unsigned i=0;i<MIRROR_WIDTH*MIRROR_HEIGHT;++i){gpu[0][i]=0xffff0000;gpu[1][i]=0xff0000ff;cpu[0][i]=cpu[1][i]=0xff00ff00;}
 errno=ERANGE;capture();assert(errno==ERANGE);pixel(255,0,0);assert(begins==1&&ends==1&&maps==1&&queries==2);
 /* The queued blue buffer is not scanned out yet: keep the completed red
  * buffer. Then a quiet/modal screen updates without another PageFlip. */
 struct stat first,unchanged;assert(!stat(SCREEN_MIRROR_FRAME,&first));
 capture();pixel(255,0,0);assert(!stat(SCREEN_MIRROR_FRAME,&unchanged));assert(first.st_ino==unchanged.st_ino);
 /* A lost RAM frame must be republished even if LCD pixels did not change. */
 unlink(SCREEN_MIRROR_FRAME);capture();pixel(255,0,0);
 presented=101;capture();pixel(0,0,255);
 unlink(SCREEN_MIRROR_FRAME);presented=100;change_during_copy=1;capture();assert(access(SCREEN_MIRROR_FRAME,F_OK));change_during_copy=0;
 assert(begins==ends);
 fail_map=1;capture();assert(access(SCREEN_MIRROR_FRAME,F_OK)&&begins==ends);fail_map=0;
 unsigned old=begins;fail_start=1;capture();assert(access(SCREEN_MIRROR_FRAME,F_OK)&&begins==old);fail_start=0;
 fail_end=1;capture();assert(access(SCREEN_MIRROR_FRAME,F_OK));fail_end=0;
 unlink(SCREEN_MIRROR_FLAG);old=queries;capture();assert(queries==old);
 puts("PASS Screen Mirror uses presented KMS frame and DMA CPU read sync; cache invalidation, quiet modal, mid-copy flip, failures and Off checked");
 return 0;
}
