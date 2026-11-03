#include <assert.h>
#include <stdarg.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
static int fake_open(const char*,int,...),fake_ioctl(int,unsigned long,...),fake_close(int),fake_munmap(void*,size_t);
static void*fake_mmap(void*,size_t,int,int,int,off_t);
#define open fake_open
#define ioctl fake_ioctl
#define close fake_close
#define mmap fake_mmap
#define munmap fake_munmap
#define main splash_main
#include "src/early_drm_splash.c"
#undef open
#undef ioctl
#undef close
#undef mmap
#undef munmap
#undef main
static unsigned opened,closed,created,destroyed,added,removed,selected,dropped;
static uint32_t active_fb;static int reject_master,wrong_mode,bad_pitch,fail_select,wrong_scanout,no_dirty,huge_resources;
static unsigned char*memory;static size_t bytes;
static int fake_open(const char*p,int flags,...){assert(!strcmp(p,"/dev/dri/card0")&&(flags&O_NOFOLLOW));++opened;return 7;}
static int fake_close(int fd){assert(fd==7);++closed;return 0;}
static void*fake_mmap(void*p,size_t size,int prot,int flags,int fd,off_t offset){assert(!p&&fd==7&&offset==4096&&(prot&PROT_WRITE)&&(flags&MAP_SHARED));bytes=size;memory=malloc(size);assert(memory);memset(memory,0xcd,size);return memory;}
static int fake_munmap(void*p,size_t size){assert(p==memory&&size==bytes);free(memory);memory=NULL;return 0;}
static int fake_ioctl(int fd,unsigned long request,...){
 assert(fd==7);va_list args;va_start(args,request);
 if(request==DRM_IOCTL_SET_MASTER){va_end(args);if(reject_master){errno=EBUSY;return -1;}return 0;}
 if(request==DRM_IOCTL_DROP_MASTER){va_end(args);assert(selected==1);++dropped;return 0;}
 void*p=va_arg(args,void*);va_end(args);assert(p);
 switch(request){
 case DRM_IOCTL_MODE_GETRESOURCES:{struct drm_mode_card_res*r=p;
  if(r->crtc_id_ptr){assert(!r->count_fbs&&r->count_crtcs==1);*(uint32_t*)(uintptr_t)r->crtc_id_ptr=17;*(uint32_t*)(uintptr_t)r->connector_id_ptr=21;*(uint32_t*)(uintptr_t)r->encoder_id_ptr=31;}
  r->count_fbs=1;r->count_crtcs=huge_resources?17:1;r->count_connectors=1;r->count_encoders=1;return 0;}
 case DRM_IOCTL_MODE_GETCONNECTOR:{struct drm_mode_get_connector*c=p;assert(c->connector_id==21);
  if(c->modes_ptr){struct drm_mode_modeinfo*m=(void*)(uintptr_t)c->modes_ptr;memset(m,0,sizeof(*m));m->hdisplay=wrong_mode?640:320;m->vdisplay=240;m->type=DRM_MODE_TYPE_PREFERRED;strcpy(m->name,"fixture");*(uint32_t*)(uintptr_t)c->encoders_ptr=31;}
  c->count_modes=1;c->count_props=0;c->count_encoders=1;c->encoder_id=31;c->connection=1;return 0;}
 case DRM_IOCTL_MODE_GETENCODER:{struct drm_mode_get_encoder*e=p;assert(e->encoder_id==31);e->crtc_id=17;e->possible_crtcs=1;return 0;}
 case DRM_IOCTL_MODE_CREATE_DUMB:{struct drm_mode_create_dumb*d=p;assert(d->width==320&&d->height==240&&d->bpp==32);d->handle=55;d->pitch=bad_pitch?100:1344;d->size=(uint64_t)d->pitch*240;++created;return 0;}
 case DRM_IOCTL_MODE_MAP_DUMB:{struct drm_mode_map_dumb*m=p;assert(m->handle==55);m->offset=4096;return 0;}
 case DRM_IOCTL_MODE_ADDFB2:{struct drm_mode_fb_cmd2*f=p;assert(f->width==320&&f->height==240&&f->pixel_format==DRM_FORMAT_XRGB8888&&f->handles[0]==55&&f->pitches[0]==1344);f->fb_id=69;++added;return 0;}
 case DRM_IOCTL_MODE_SETCRTC:{struct drm_mode_crtc*c=p;assert(c->fb_id==69&&c->crtc_id==17&&c->mode_valid&&c->count_connectors==1&&*(uint32_t*)(uintptr_t)c->set_connectors_ptr==21);if(fail_select){errno=EIO;return -1;}active_fb=69;++selected;return 0;}
 case DRM_IOCTL_MODE_DIRTYFB:if(no_dirty){errno=EOPNOTSUPP;return -1;}assert(((struct drm_mode_fb_dirty_cmd*)p)->fb_id==69);return 0;
 case DRM_IOCTL_MODE_GETCRTC:{struct drm_mode_crtc*c=p;assert(c->crtc_id==17);c->mode_valid=1;c->fb_id=wrong_scanout?70:active_fb;return 0;}
 case DRM_IOCTL_MODE_RMFB:assert(*(uint32_t*)p==69);if(active_fb==69)active_fb=0;++removed;return 0;
 case DRM_IOCTL_MODE_DESTROY_DUMB:assert(((struct drm_mode_destroy_dumb*)p)->handle==55);++destroyed;return 0;
 default:assert(!"unexpected ioctl");return -1;
 }
}
static void reset(void){opened=closed=created=destroyed=added=removed=selected=dropped=0;active_fb=0;reject_master=wrong_mode=bad_pitch=fail_select=wrong_scanout=no_dirty=huge_resources=0;}
static void failure(void){struct screen s={.fd=-1};assert(!show(&s));cleanup(&s);assert(opened==closed&&created==destroyed&&added==removed&&!dropped&&!memory);}
int main(void){
 setbuf(stdout,NULL);reset();struct screen s={.fd=-1};no_dirty=1;assert(show(&s)&&active_fb==69&&dropped==1&&s.map&&s.pitch==1344);
 unsigned bright=0;for(unsigned y=0;y<240;++y){for(unsigned x=0;x<320;++x)bright+=((uint32_t*)(memory+y*s.pitch))[x]!=0xff090c10;for(unsigned x=1280;x<s.pitch;++x)assert(memory[y*s.pitch+x]==0xcd);}assert(bright>100&&bright<20000);
 active_fb=81;cleanup(&s);assert(active_fb==81&&opened==closed&&created==destroyed&&added==removed&&!memory);
 puts("PASS real DRM UAPI layouts: enumerate bounded resources, select connected 320x240 panel, fill padded XRGB buffer, modeset/verify active framebuffer, release master; unsupported DIRTYFB is harmless; cleanup cannot remove native's replacement frame");
 reset();reject_master=1;failure();reset();huge_resources=1;failure();reset();wrong_mode=1;failure();reset();bad_pitch=1;failure();reset();fail_select=1;failure();reset();wrong_scanout=1;failure();
 puts("PASS busy DRM owner, oversized resources, wrong panel, invalid pitch, modeset and scanout-verification failures unwind only owned resources and never report readiness");return 0;
}
