/* Use the already verified DRM scanout allocation/selection code. This
 * process and all pixels live in RAM while the MPC app is stopped. */
#define main unused_usb_boot_screen_main
#define EARLY_DRM_CUSTOM_FILL 1
#include "early_drm_splash.c"
#undef main
#include <glob.h>
#include <limits.h>
#include <sound/asound.h>
#include "usb_drive_input.h"
static void line(struct screen*s,const char*text,unsigned y,uint32_t colour){
 am_splash_text_aa(s->map,s->pitch,text,y,(const uint8_t(*)[1])am_font_url,
                  AM_FONT_URL_W,AM_FONT_URL_H,AM_FONT_URL_W*AM_FONT_URL_H,colour);
}
/* Match the application's B1/B3 positions while it is stopped for USB sharing. */
static const char*top_label(const char*state,unsigned button){
 if(!strcmp(state,"confirm"))return button==0?"Cancel":button==2?"Return":"";
 if(!strcmp(state,"waiting")||!strcmp(state,"sharing"))return button==2?"Disconnect":"";
 return "";
}
static void top_key(struct screen*s,unsigned button,const char*label){
 if(!*label)return;
 unsigned x=button==2?215:2,w=103;
 for(unsigned y=1;y<21;++y){uint32_t*row=(void*)((char*)s->map+y*s->pitch);
  for(unsigned col=x;col<x+w;++col)row[col]=y==20?0xff3d4856u:0xff202832u;
 }
 size_t length=strlen(label);unsigned cw=AM_FONT_URL_W,ch=AM_FONT_URL_H;
 if(length*cw>w-4||ch>19)return;
 unsigned left=x+(w-(unsigned)length*cw)/2,top=1+(19-ch)/2;
 for(unsigned i=0;i<length;++i){unsigned c=(unsigned char)label[i];if(c<32||c>126)continue;
  const uint8_t*g=am_font_url[c-32];
  for(unsigned row=0;row<ch;++row){uint32_t*line=(void*)((char*)s->map+(top+row)*s->pitch);
   for(unsigned col=0;col<cw;++col){unsigned a=g[row*cw+col];if(!a)continue;
    uint32_t*px=line+left+i*cw+col,b=*px,out=0xff000000u;
    for(unsigned shift=0;shift<24;shift+=8)out|=((255*a+((b>>shift)&255)*(255-a)+127)/255)<<shift;
    *px=out;
   }
  }
 }
}
static void draw(struct screen*s,const char*state){
 for(unsigned y=0;y<240;++y){uint32_t*p=(void*)((char*)s->map+y*s->pitch);for(unsigned x=0;x<320;++x)p[x]=0xff101419;}
 top_key(s,0,top_label(state,0));top_key(s,2,top_label(state,2));
 line(s,"SD Card Access",32,0xffffc247);
 if(!strcmp(state,"waiting")||!strcmp(state,"ready")){
  line(s,"Connect USB to a computer",73,0xffffffff);line(s,"Waiting for USB...",104,0xffa8b3c2);
 }else if(!strcmp(state,"sharing")){
  line(s,"SD card is on your computer",73,0xffffffff);line(s,"Eject the disk on the PC",104,0xffff6060);
 }else if(!strcmp(state,"confirm")){
  line(s,"Disk safely ejected on PC?",73,0xffff6060);line(s,"Return resumes Am MOD",112,0xffa8b3c2);
 }else if(!strcmp(state,"no-input")){
  line(s,"Eject the disk on the PC",73,0xffff6060);line(s,"Unplug USB to return",112,0xffffc247);
 }else if(!strcmp(state,"error")){
  line(s,"USB transfer could not start",73,0xffff6060);line(s,"Returning to Am MOD...",110,0xffffffff);
 }else{
  line(s,"Preparing / restoring SD",73,0xffffffff);line(s,"Please wait...",110,0xffa8b3c2);
 }
 line(s,"Keep the SD card inserted",184,0xffff6060);line(s,"USB audio / MIDI paused",209,0xffa8b3c2);
}
/* Draw the transfer page before selecting scanout. Never select the boot
 * artwork and rely on DIRTYFB (unsupported by some panel drivers) to replace it. */
static int fill(struct screen*s){draw(s,"preparing");return 1;}
static int present(struct screen*s,const char*state){
 if(ioctl(s->fd,DRM_IOCTL_SET_MASTER,0))return failed("transfer_master");
 draw(s,state);
 struct drm_mode_crtc c={.set_connectors_ptr=(uintptr_t)&s->connector,.count_connectors=1,.crtc_id=s->crtc,.fb_id=s->fb,.mode_valid=1,.mode=s->mode};
 int ok=!ioctl(s->fd,DRM_IOCTL_MODE_SETCRTC,&c);
 if(!ok)failed("transfer_present");
 if(ioctl(s->fd,DRM_IOCTL_DROP_MASTER,0))ok=0;
 return ok;
}
static int controls_standard_mode(int fd){
 /* The previous application enables 32-byte timestamp frames on this
  * substream. DRAIN discards the input queue; DROP is output-only in ALSA
  * and returns EINVAL for input. Then select plain MIDI bytes. */
 int stream=SNDRV_RAWMIDI_STREAM_INPUT;
 if(ioctl(fd,SNDRV_RAWMIDI_IOCTL_DRAIN,&stream)){fprintf(stderr,"USB_DRIVE_INPUT_DRAIN_FAILED errno=%d\n",errno);return 0;}
 struct snd_rawmidi_params params={.stream=SNDRV_RAWMIDI_STREAM_INPUT,.buffer_size=4096,.avail_min=1,.no_active_sensing=1,.mode=0};
 if(ioctl(fd,SNDRV_RAWMIDI_IOCTL_PARAMS,&params)){fprintf(stderr,"USB_DRIVE_INPUT_PARAMS_FAILED errno=%d\n",errno);return 0;}
 fprintf(stderr,"USB_DRIVE_INPUT_MODE standard\n");
 return 1;
}
static int open_controls(void){
 glob_t paths={0};int selected=-1;
 if(glob("/dev/snd/midiC*D*",0,NULL,&paths))return -1;
 for(size_t i=0;i<paths.gl_pathc;++i){
  int fd=open(paths.gl_pathv[i],O_RDONLY|O_NONBLOCK|O_CLOEXEC|O_NOFOLLOW);if(fd<0)continue;
  struct snd_rawmidi_info info={.stream=SNDRV_RAWMIDI_STREAM_INPUT};
  int valid=!ioctl(fd,SNDRV_RAWMIDI_IOCTL_INFO,&info);
  char sys[128],device[PATH_MAX];snprintf(sys,sizeof(sys),"/sys/class/sound/card%d/device",info.card);
  int internal=valid&&realpath(sys,device)&&usb_drive_internal_transport(device);
  if(valid)fprintf(stderr,"USB_DRIVE_INPUT_CANDIDATE path=%s id=%.64s name=%.80s\n",paths.gl_pathv[i],info.id,info.name);
  if(internal&&usb_drive_control_identity((const char*)info.id,sizeof(info.id),(const char*)info.name,sizeof(info.name))){
   if(!controls_standard_mode(fd)){fprintf(stderr,"USB_DRIVE_INPUT_MODE_FAILED errno=%d\n",errno);close(fd);continue;}
   if(selected>=0){close(selected);close(fd);selected=-1;break;} /* Ambiguous: fail closed. */
   selected=fd;fprintf(stderr,"USB_DRIVE_INPUT path=%s name=%.80s\n",paths.gl_pathv[i],info.name);
  }else close(fd);
 }
 globfree(&paths);return selected;
}
static int request_exit(const char*phase_path){
 char path[512];int n=snprintf(path,sizeof(path),"%s",phase_path);if(n<0||(size_t)n>=sizeof(path))return 0;
 char*slash=strrchr(path,'/');if(!slash)return 0;
 if((size_t)(slash-path)+sizeof("/exit-request")>sizeof(path))return 0;
 strcpy(slash,"/exit-request");
 int fd=open(path,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);if(fd<0)return 0;
 const char text[]="AM_USB_EXIT 1\n";int ok=write(fd,text,sizeof(text)-1)==(ssize_t)(sizeof(text)-1);
 if(close(fd))ok=0;
 return ok;
}
#ifndef USB_DRIVE_SCREEN_NO_MAIN
int main(int argc,char**argv){
 if(argc!=3)return 2;
 struct screen s={.fd=-1};
 struct sigaction action={.sa_handler=stop};sigemptyset(&action.sa_mask);sigaction(SIGTERM,&action,NULL);sigaction(SIGINT,&action,NULL);
 if(!show(&s)){cleanup(&s);return 1;}
 int fd=open(argv[2],O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);if(fd<0){cleanup(&s);return 1;}close(fd);
 int controls=open_controls();struct usb_drive_input input={0};
 uint64_t input_after=now_ms()+700;
 char previous[32]="";int requested=0;unsigned logged_input=0;
 while(!stopping){
  char state[32]="preparing";FILE*f=fopen(argv[1],"r");if(f){if(!fgets(state,sizeof(state),f))state[0]=0;fclose(f);state[strcspn(state,"\r\n")]=0;}
  if(!strcmp(state,"done"))break;
  int interactive=!strcmp(state,"sharing")||!strcmp(state,"waiting");
  if(controls>=0){
   unsigned char bytes[256];ssize_t count;
   while((count=read(controls,bytes,sizeof(bytes)))>0){
    for(ssize_t i=0;i<count;++i){
     if(bytes[i]<0xf8&&logged_input<96){fprintf(stderr,"USB_DRIVE_INPUT_BYTE %02x\n",bytes[i]);++logged_input;}
     int action=usb_drive_input_byte(&input,bytes[i],interactive&&!requested&&now_ms()>=input_after);
     if(action==2){
      if(request_exit(argv[1])){requested=1;fprintf(stderr,"USB_DRIVE_EXIT_CONFIRMED\n");}
     }
    }
   }
  }
  const char*view=requested?"returning":interactive&&input.confirm?"confirm":interactive&&controls<0?"no-input":state;
  if(strcmp(previous,view)){if(!present(&s,view))break;snprintf(previous,sizeof(previous),"%s",view);}
  usleep(100000);
 }
 if(controls>=0)close(controls);
 cleanup(&s);return 0;
}
#endif
