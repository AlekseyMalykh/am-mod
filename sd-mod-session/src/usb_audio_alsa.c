#define _GNU_SOURCE
#include "usb_audio.h"
#include <dlfcn.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <stdatomic.h>
#include <pthread.h>
#include <unistd.h>
#ifndef MPC_USB_ACTIVE_PATH
#define MPC_USB_ACTIVE_PATH "/run/mpc-trial-app/usb-audio-18.active"
#endif
/* ALSA's stable C ABI. Keep development headers out of the firmware build.
 * Playback is device -> host. USB capture (phone -> MPC) is never changed. */
typedef struct _snd_pcm pcm_t;
typedef struct _snd_pcm_hw_params params_t;
typedef long frames_t;
static int (*real_open)(pcm_t**,const char*,int,int);
static int (*real_close)(pcm_t*),(*real_drop)(pcm_t*),(*real_prepare)(pcm_t*);
static int (*real_any)(pcm_t*,params_t*),(*real_channels)(pcm_t*,params_t*,unsigned);
static int (*real_near)(pcm_t*,params_t*,unsigned*),(*real_get)(const params_t*,unsigned*);
static int (*real_min)(const params_t*,unsigned*),(*real_max)(const params_t*,unsigned*);
static int (*real_format)(pcm_t*,params_t*,int),(*real_apply)(pcm_t*,params_t*);
static frames_t (*real_writei)(pcm_t*,const void*,unsigned long);
static frames_t (*real_writen)(pcm_t*,void**,unsigned long);
static pthread_once_t once=PTHREAD_ONCE_INIT;
static _Atomic(pcm_t*) target;
static params_t *target_params;
static int sample_format=-1,configured;
static float block[AM_USB_CAPACITY*AM_USB_CHANNELS];
static unsigned char packed[AM_USB_CAPACITY*AM_USB_CHANNELS*8];
static void*planes[AM_USB_CHANNELS];
static void resolve(void){
#define RESOLVE(name,member) *(void**)(&member)=dlsym(RTLD_NEXT,name)
 RESOLVE("snd_pcm_open",real_open);RESOLVE("snd_pcm_close",real_close);
 RESOLVE("snd_pcm_drop",real_drop);RESOLVE("snd_pcm_prepare",real_prepare);
 RESOLVE("snd_pcm_hw_params_any",real_any);RESOLVE("snd_pcm_hw_params_set_channels",real_channels);
 RESOLVE("snd_pcm_hw_params_set_channels_near",real_near);RESOLVE("snd_pcm_hw_params_get_channels",real_get);
 RESOLVE("snd_pcm_hw_params_get_channels_min",real_min);RESOLVE("snd_pcm_hw_params_get_channels_max",real_max);
 RESOLVE("snd_pcm_hw_params_set_format",real_format);RESOLVE("snd_pcm_hw_params",real_apply);
 RESOLVE("snd_pcm_writei",real_writei);RESOLVE("snd_pcm_writen",real_writen);
#undef RESOLVE
}
static int ours(pcm_t*p){return p&&p==atomic_load_explicit(&target,memory_order_acquire);}
static int bytes(int f){return f==2?2:f==6||f==10||f==14?4:f==16?8:f==32?3:0;}
int snd_pcm_open(pcm_t**p,const char*name,int stream,int mode){
 pthread_once(&once,resolve);if(!real_open)return -ENOSYS;
 int r=real_open(p,name,stream,mode);
 /* Exact device identity, never substring-match external audio interfaces. */
 if(!r&&stream==0&&name&&(!strcmp(name,"hw:UAC2Gadget")||!strcmp(name,"hw:UAC2Gadget,0"))&&
    access(MPC_USB_ACTIVE_PATH,F_OK)==0){
  pcm_t*nil=NULL;
  if(atomic_compare_exchange_strong(&target,&nil,*p)){configured=0;sample_format=-1;target_params=NULL;usb_audio_enable(1);}
 }
 return r;
}
int snd_pcm_close(pcm_t*p){
 if(ours(p)){atomic_store(&target,NULL);configured=0;target_params=NULL;usb_audio_enable(0);}
 return real_close?real_close(p):-ENOSYS;
}
int snd_pcm_drop(pcm_t*p){if(ours(p))usb_audio_reset();return real_drop?real_drop(p):-ENOSYS;}
int snd_pcm_prepare(pcm_t*p){if(ours(p))usb_audio_reset();return real_prepare?real_prepare(p):-ENOSYS;}
int snd_pcm_hw_params_any(pcm_t*p,params_t*a){
 if(ours(p))target_params=a;
 return real_any?real_any(p,a):-ENOSYS;
}
int snd_pcm_hw_params_set_channels(pcm_t*p,params_t*a,unsigned n){
 if(ours(p)){if(n!=2)return -EINVAL;target_params=a;n=AM_USB_CHANNELS;}
 return real_channels?real_channels(p,a,n):-ENOSYS;
}
int snd_pcm_hw_params_set_channels_near(pcm_t*p,params_t*a,unsigned*n){
 if(!real_near||!n)return -EINVAL;
 if(!ours(p))return real_near(p,a,n);
 if(*n!=2)return -EINVAL;
 unsigned channels=AM_USB_CHANNELS;int r=real_near(p,a,&channels);
 if(!r&&channels!=AM_USB_CHANNELS)return -EINVAL;
 if(!r){*n=2;target_params=a;}return r;
}
int snd_pcm_hw_params_get_channels(const params_t*a,unsigned*n){
 int r=real_get?real_get(a,n):-ENOSYS;
 if(!r&&atomic_load(&target)&&a==target_params&&*n==AM_USB_CHANNELS)*n=2;
 return r;
}
int snd_pcm_hw_params_get_channels_min(const params_t*a,unsigned*n){
 int r=real_min?real_min(a,n):-ENOSYS;
 if(!r&&atomic_load(&target)&&a==target_params)*n=2;
 return r;
}
int snd_pcm_hw_params_get_channels_max(const params_t*a,unsigned*n){
 int r=real_max?real_max(a,n):-ENOSYS;
 if(!r&&atomic_load(&target)&&a==target_params)*n=2;
 return r;
}
int snd_pcm_hw_params_set_format(pcm_t*p,params_t*a,int f){
 if(ours(p)&&!bytes(f))return -EINVAL;
 int r=real_format?real_format(p,a,f):-ENOSYS;
 if(!r&&ours(p))sample_format=f;
 return r;
}
int snd_pcm_hw_params(pcm_t*p,params_t*a){
 int r=real_apply?real_apply(p,a):-ENOSYS;
 if(!r&&ours(p)){unsigned n=0;
  if(!real_get||real_get(a,&n)||n!=AM_USB_CHANNELS||!bytes(sample_format))return -EINVAL;
  target_params=a;configured=1;usb_audio_reset();}
 return r;
}
static float decode(const unsigned char*p,int f){
 if(f==2){int16_t x;memcpy(&x,p,2);return x/32768.f;}
 if(f==14){float x;memcpy(&x,p,4);return isfinite(x)?x:0;}
 if(f==16){double x;memcpy(&x,p,8);return isfinite(x)?(float)x:0;}
 if(f==32){int32_t x=(int32_t)((unsigned)p[0]|((unsigned)p[1]<<8)|((unsigned)p[2]<<16));if(x&0x800000)x|=(int32_t)0xff000000u;return x/8388608.f;}
 int32_t x;memcpy(&x,p,4);return x/(f==6?8388608.f:2147483648.f);
}
static void encode(unsigned char*p,float v,int f){
 if(!isfinite(v))v=0;
 if(f==14){memcpy(p,&v,4);return;}
 if(f==16){double d=v;memcpy(p,&d,8);return;}
 double x=fmax(-1.,fmin(1.,(double)v));
 int64_t scale=f==2?32768:f==6||f==32?8388608:INT64_C(2147483648);
 int64_t q=x>=1?scale-1:(int64_t)(x*scale);
 if(f==32){for(unsigned i=0;i<3;++i)p[i]=(unsigned char)((uint64_t)q>>(i*8));}
 else if(f==2){int16_t s=(int16_t)q;memcpy(p,&s,2);}
 else {int32_t s=(int32_t)q;memcpy(p,&s,4);}
}
static frames_t write_expanded(pcm_t*p,const void*input,void**noninterleaved,unsigned long n){
 if(!configured||!n||n>AM_USB_CAPACITY)return -EINVAL;
 unsigned size=(unsigned)bytes(sample_format),have=usb_audio_peek(block,(unsigned)n);
 /* Missing mixer blocks never replay a previous project. Preserve stock
  * Master in the gap; all additional outputs are explicitly silent. */
 for(unsigned f=have;f<n;++f)for(unsigned c=0;c<AM_USB_CHANNELS;++c){
  const unsigned char*src=c<2?(noninterleaved?(const unsigned char*)noninterleaved[c]+f*size:(const unsigned char*)input+(f*2+c)*size):NULL;
  block[f*AM_USB_CHANNELS+c]=src?decode(src,sample_format):0;
 }
 for(unsigned c=0;c<AM_USB_CHANNELS;++c){planes[c]=packed+(size_t)c*n*size;
  for(unsigned f=0;f<n;++f){size_t offset=noninterleaved?((size_t)c*n+f)*size:((size_t)f*AM_USB_CHANNELS+c)*size;
   encode(packed+offset,block[f*AM_USB_CHANNELS+c],sample_format);}}
 frames_t r=noninterleaved?real_writen(p,planes,n):real_writei(p,packed,n);
 if(r>0)usb_audio_consume((unsigned)r<have?(unsigned)r:have);
 return r;
}
frames_t snd_pcm_writei(pcm_t*p,const void*data,unsigned long n){
 if(!real_writei)return -ENOSYS;
 return ours(p)?write_expanded(p,data,NULL,n):real_writei(p,data,n);
}
frames_t snd_pcm_writen(pcm_t*p,void**data,unsigned long n){
 if(!real_writen)return -ENOSYS;
 return ours(p)?write_expanded(p,NULL,data,n):real_writen(p,data,n);
}
