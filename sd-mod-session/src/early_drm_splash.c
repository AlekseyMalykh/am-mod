/* Explicit DRM scanout for the short interval between stock and the mod.
 * fbdev writes can succeed without selecting that buffer on the LCD. Here
 * the kernel selects our own buffer, and GETCRTC verifies the active fb id.
 * Drop DRM master before signalling readiness so the app can take over.
 * Keep only our buffer alive until another fb appears, cancellation or timeout. */
#define _GNU_SOURCE
#include <drm/drm.h>
#include <drm/drm_mode.h>
#include <drm/drm_fourcc.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "am_mod_splash.inc"
#include "splash_branding.h"
#ifndef EARLY_DRM_ROOT
#define EARLY_DRM_ROOT "/run/mpc-clone-trial-v1"
#endif
#define READY EARLY_DRM_ROOT "/boot-splash-ready"
#define STOP EARLY_DRM_ROOT "/boot-splash-stop"
struct screen {int fd;uint32_t handle,fb,crtc,connector,pitch;uint64_t size;void*map;struct drm_mode_modeinfo mode;};
static volatile sig_atomic_t stopping;
static void stop(int sig){(void)sig;stopping=1;}
static uint64_t now_ms(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000+t.tv_nsec/1000000;}
static int failed(const char*stage){fprintf(stderr,"EARLY_DRM FAILED stage=%s errno=%d\n",stage,errno);return 0;}
static int choose(struct screen*s){
 uint32_t crtcs[16],connectors[16],encoders[32];
 struct drm_mode_card_res r={0};
 if(ioctl(s->fd,DRM_IOCTL_MODE_GETRESOURCES,&r))return failed("resources_count");
 if(!r.count_crtcs||r.count_crtcs>16||!r.count_connectors||r.count_connectors>16||r.count_encoders>32){errno=EOVERFLOW;return failed("resource_bounds");}
 r.count_fbs=0; /* No fb-id output buffer: query the count without copying ids. */
 r.crtc_id_ptr=(uintptr_t)crtcs;r.connector_id_ptr=(uintptr_t)connectors;r.encoder_id_ptr=(uintptr_t)encoders;
 if(ioctl(s->fd,DRM_IOCTL_MODE_GETRESOURCES,&r)||r.count_crtcs>16||r.count_connectors>16||r.count_encoders>32)return failed("resources_read");
 for(unsigned i=0;i<r.count_connectors;++i){
  struct drm_mode_get_connector c={.connector_id=connectors[i]};
  if(ioctl(s->fd,DRM_IOCTL_MODE_GETCONNECTOR,&c))continue;
  if(c.connection!=1||!c.count_modes||c.count_modes>64||c.count_encoders>32||c.count_props>128)continue;
  struct drm_mode_modeinfo modes[64];uint32_t ids[32],props[128];uint64_t values[128];
  c.modes_ptr=(uintptr_t)modes;c.encoders_ptr=(uintptr_t)ids;c.props_ptr=(uintptr_t)props;c.prop_values_ptr=(uintptr_t)values;
  if(ioctl(s->fd,DRM_IOCTL_MODE_GETCONNECTOR,&c)||c.connection!=1||c.count_modes>64||c.count_encoders>32||c.count_props>128)continue;
  int selected=-1;
  for(unsigned j=0;j<c.count_modes;++j)if(modes[j].hdisplay==320&&modes[j].vdisplay==240){selected=(int)j;if(modes[j].type&DRM_MODE_TYPE_PREFERRED)break;}
  if(selected<0)continue;
  uint32_t crtc=0;
  for(unsigned j=0;j<c.count_encoders&&!crtc;++j){
   struct drm_mode_get_encoder e={.encoder_id=ids[j]};if(ioctl(s->fd,DRM_IOCTL_MODE_GETENCODER,&e))continue;
   for(unsigned k=0;k<r.count_crtcs;++k)if(e.possible_crtcs&(1u<<k)){
    if(!crtc)crtc=crtcs[k];
    if(e.crtc_id==crtcs[k]){crtc=e.crtc_id;break;}
   }
  }
  if(!crtc)continue;
  s->connector=c.connector_id;s->crtc=crtc;s->mode=modes[selected];
  fprintf(stderr,"EARLY_DRM CONNECTOR id=%u type=%u crtc=%u mode=%.32s size=320x240\n",s->connector,c.connector_type,crtc,s->mode.name);return 1;
 }
 errno=ENODEV;return failed("no_connected_320x240_panel");
}
static int fill(struct screen*s){
 if(s->pitch<1280||s->pitch>16384||s->size<(uint64_t)s->pitch*240||s->size>4*1024*1024){errno=EOVERFLOW;return failed("buffer_bounds");}
 unsigned n=0;
 for(unsigned i=0;i<sizeof(am_splash_runs)/sizeof(*am_splash_runs);++i){
  unsigned count=am_splash_runs[i].count;
  if(!count||count>320*240-n){errno=EINVAL;return failed("image_bounds");}
  for(unsigned j=0;j<count;++j,++n)((uint32_t*)((char*)s->map+(n/320)*s->pitch))[n%320]=am_splash_runs[i].argb;
 }
 if(n!=320*240){errno=EINVAL;return failed("image_size");}
 am_splash_branding(s->map,s->pitch);return 1;
}
static void cleanup(struct screen*s){
 if(s->fb){uint32_t fb=s->fb;if(ioctl(s->fd,DRM_IOCTL_MODE_RMFB,&fb))fprintf(stderr,"EARLY_DRM RMFB errno=%d\n",errno);s->fb=0;}
 if(s->map&&s->map!=MAP_FAILED){munmap(s->map,s->size);s->map=NULL;}
 if(s->handle){struct drm_mode_destroy_dumb d={.handle=s->handle};(void)ioctl(s->fd,DRM_IOCTL_MODE_DESTROY_DUMB,&d);s->handle=0;}
 if(s->fd>=0){close(s->fd);s->fd=-1;}
}
static int show(struct screen*s){
 s->fd=open("/dev/dri/card0",O_RDWR|O_NOFOLLOW|O_CLOEXEC);if(s->fd<0)return failed("open_card0");
 if(ioctl(s->fd,DRM_IOCTL_SET_MASTER,0))return failed("set_master");
 if(!choose(s))return 0;
 struct drm_mode_create_dumb d={.height=240,.width=320,.bpp=32};
 if(ioctl(s->fd,DRM_IOCTL_MODE_CREATE_DUMB,&d))return failed("create_buffer");
 s->handle=d.handle;s->pitch=d.pitch;s->size=d.size;
 if(s->pitch<1280||s->pitch>16384||s->size<(uint64_t)s->pitch*240||s->size>4*1024*1024){errno=EOVERFLOW;return failed("buffer_bounds");}
 struct drm_mode_map_dumb m={.handle=s->handle};if(ioctl(s->fd,DRM_IOCTL_MODE_MAP_DUMB,&m))return failed("map_offset");
 s->map=mmap(NULL,s->size,PROT_READ|PROT_WRITE,MAP_SHARED,s->fd,m.offset);if(s->map==MAP_FAILED)return failed("map_buffer");
 if(!fill(s))return 0;
 struct drm_mode_fb_cmd2 fb={.width=320,.height=240,.pixel_format=DRM_FORMAT_XRGB8888};fb.handles[0]=s->handle;fb.pitches[0]=s->pitch;
 if(ioctl(s->fd,DRM_IOCTL_MODE_ADDFB2,&fb))return failed("add_fb");
 s->fb=fb.fb_id;
 struct drm_mode_crtc c={.set_connectors_ptr=(uintptr_t)&s->connector,.count_connectors=1,.crtc_id=s->crtc,.fb_id=s->fb,.mode_valid=1,.mode=s->mode};
 if(ioctl(s->fd,DRM_IOCTL_MODE_SETCRTC,&c))return failed("select_scanout");
 struct drm_mode_fb_dirty_cmd dirty={.fb_id=s->fb};int flushed=ioctl(s->fd,DRM_IOCTL_MODE_DIRTYFB,&dirty);
 int flush_errno=flushed<0?errno:0;
 struct drm_mode_crtc current={.crtc_id=s->crtc};
 if(ioctl(s->fd,DRM_IOCTL_MODE_GETCRTC,&current)||!current.mode_valid||current.fb_id!=s->fb){errno=EIO;return failed("verify_scanout");}
 fprintf(stderr,"EARLY_DRM ACTIVE_SCANOUT framebuffer=%u crtc=%u pitch=%u dirty=%d dirty_errno=%d\n",s->fb,s->crtc,s->pitch,flushed,flush_errno);
 fprintf(stderr,"EARLY_DRM BUILD version=%s repository=%s\n",AM_MOD_VERSION,AM_MOD_REPO_URL);
 if(ioctl(s->fd,DRM_IOCTL_DROP_MASTER,0))return failed("drop_master");
 fprintf(stderr,"EARLY_DRM MASTER_RELEASED app_can_take_over\n");return 1;
}
static int ready(const struct screen*s){
 int fd=open(READY,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);if(fd<0)return failed("ready_open");
 char message[80];int n=snprintf(message,sizeof(message),"%ld %u\n",(long)getpid(),s->fb);
 int ok=write(fd,message,n)==n;if(close(fd))ok=0;
 if(!ok)return failed("ready_write");
 return 1;
}
int main(int argc,char**argv){
 if(argc!=4||strcmp(argv[1],"--hold")||strcmp(argv[2],READY)||strcmp(argv[3],STOP))return 2;
 struct stat root;if(lstat(EARLY_DRM_ROOT,&root)||!S_ISDIR(root.st_mode)||S_ISLNK(root.st_mode)||root.st_uid)return 1;
 setbuf(stderr,NULL);struct sigaction action={.sa_handler=stop};sigemptyset(&action.sa_mask);sigaction(SIGTERM,&action,NULL);sigaction(SIGINT,&action,NULL);
 struct screen s={.fd=-1};if(!show(&s)||!ready(&s)){cleanup(&s);return 1;}
 uint64_t shown_since=now_ms(),deadline=shown_since+120000;const char*reason="timeout";
 while(now_ms()<deadline){
  if(stopping){reason="signal";break;}if(!access(STOP,F_OK)){reason="controller_stop";break;}
  struct drm_mode_crtc current={.crtc_id=s.crtc};
  if(!ioctl(s.fd,DRM_IOCTL_MODE_GETCRTC,&current)&&current.mode_valid&&current.fb_id&&current.fb_id!=s.fb){reason="native_frame_took_over";break;}
  struct timespec pause={.tv_nsec=100000000};while(nanosleep(&pause,&pause)&&errno==EINTR&&!stopping){}
 }
 fprintf(stderr,"EARLY_DRM RELEASE reason=%s shown_ms=%llu\n",reason,(unsigned long long)(now_ms()-shown_since));cleanup(&s);return 0;
}
