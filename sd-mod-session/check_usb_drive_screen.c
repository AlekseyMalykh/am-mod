#define USB_DRIVE_SCREEN_NO_MAIN 1
#define ioctl fake_ioctl
#include "src/usb_drive_screen.c"
#undef ioctl
#include <assert.h>
#include <stdarg.h>
static unsigned masters,commits,drops,input_drops,input_params;
static int fail_params;
int fake_ioctl(int fd,unsigned long request,...){
 (void)fd;
 if(request==DRM_IOCTL_SET_MASTER)++masters;
 else if(request==DRM_IOCTL_MODE_SETCRTC)++commits;
 else if(request==DRM_IOCTL_DROP_MASTER)++drops;
 /* Mirror the kernel ABI: DROP supports output only, DRAIN clears input. */
 else if(request==SNDRV_RAWMIDI_IOCTL_DROP){va_list args;va_start(args,request);int*s=va_arg(args,int*);assert(*s==SNDRV_RAWMIDI_STREAM_INPUT);va_end(args);errno=EINVAL;return -1;}
 else if(request==SNDRV_RAWMIDI_IOCTL_DRAIN){va_list args;va_start(args,request);int*s=va_arg(args,int*);assert(*s==SNDRV_RAWMIDI_STREAM_INPUT);va_end(args);++input_drops;}
 else if(request==SNDRV_RAWMIDI_IOCTL_PARAMS){
  va_list args;va_start(args,request);struct snd_rawmidi_params*p=va_arg(args,struct snd_rawmidi_params*);va_end(args);
  assert(input_drops>input_params);assert(p->stream==SNDRV_RAWMIDI_STREAM_INPUT&&p->mode==0&&p->buffer_size==4096&&p->avail_min==1);++input_params;
  if(fail_params){errno=EIO;return -1;}
 }
 else {errno=ENOTTY;return -1;}
 return 0;
}
static int cc(struct usb_drive_input*p,unsigned code,unsigned value,int enabled){
 assert(!usb_drive_input_byte(p,value?0x90:0x80,enabled));assert(!usb_drive_input_byte(p,code,enabled));
 return usb_drive_input_byte(p,value,enabled);
}
static int click(struct usb_drive_input*p,unsigned code){assert(!cc(p,code,127,1));return cc(p,code,0,1);}
int main(void){
 unsigned char amber[192],received[192];usb_drive_amber_frame(amber);
 for(unsigned pad=0;pad<16;++pad){unsigned char*p=amber+pad*12;assert(p[0]==0xf0&&p[3]==0x5c&&p[7]==pad&&p[8]==127&&p[9]==97&&p[10]==35&&p[11]==0xf7);}
 int pipes[2];assert(!pipe(pipes));struct amber_writer writer={0};amber_tick(pipes[1],1000,&writer);
 assert(read(pipes[0],received,sizeof(received))==192&&!memcmp(received,amber,192));assert(writer.next_ms==2000&&!writer.failed);
 amber_tick(pipes[1],1500,&writer);assert(writer.next_ms==2000);
 amber_tick(pipes[1],2000,&writer);assert(read(pipes[0],received,sizeof(received))==192&&!memcmp(received,amber,192));close(pipes[0]);close(pipes[1]);
 int stream=SNDRV_RAWMIDI_STREAM_INPUT;
 assert(fake_ioctl(42,SNDRV_RAWMIDI_IOCTL_DROP,&stream)==-1&&errno==EINVAL);
 assert(controls_standard_mode(42));assert(input_drops==1&&input_params==1);
 fail_params=1;assert(!controls_standard_mode(42));fail_params=0;
 struct usb_drive_input p={0};
 assert(!cc(&p,0x0e,127,0));assert(!cc(&p,0x0e,0,1));assert(!p.confirm);
 assert(click(&p,0x0e)==1&&p.confirm);
 assert(click(&p,0x0c)==1&&!p.confirm);
 assert(click(&p,0x0e)==1&&p.confirm);
 assert(!cc(&p,0x0e,127,1));assert(!cc(&p,0x0e,127,1));
 assert(cc(&p,0x0e,0,1)==2&&!p.confirm);
 assert(!cc(&p,0x10,127,1));assert(!cc(&p,0x10,0,1));
 unsigned char ignore[]={0xf0,0xb,0xe,0,0xf7,0x0e,0,0xb0,0x0e,127,0x0e,0,0x91,0x0e,127,0x0e,0};
 for(unsigned i=0;i<sizeof(ignore);++i)assert(!usb_drive_input_byte(&p,ignore[i],1));
 unsigned char running[]={0x90,0x0e,127,0xf8,0x0e,0};
 int action=0;for(unsigned i=0;i<sizeof(running);++i)action=usb_drive_input_byte(&p,running[i],1);
 assert(action==1&&p.confirm);
 /* Exact MIDI payloads from the physical usb7 log, after selecting standard
  * mode. Note Off velocity can be nonzero without becoming a new press. */
 struct usb_drive_input physical={0};
 unsigned char recorded[]={0x90,0x0e,0x7f,0x80,0x0e,0x00,0x90,0x0e,0x7f,0x80,0x0e,0x00};
 unsigned actions=0;for(unsigned i=0;i<sizeof(recorded);++i){int a=usb_drive_input_byte(&physical,recorded[i],1);if(a)actions=(actions<<4)|(unsigned)a;}
 assert(actions==0x12);
 assert(!cc(&physical,0x0c,127,1));
 assert(!usb_drive_input_byte(&physical,0x80,1));assert(!usb_drive_input_byte(&physical,0x0c,1));assert(usb_drive_input_byte(&physical,64,1)==1);
 /* Physical usb5 report: identical SerialMIDI IDs, distinct input names.
  * Only one of these two real candidates may be selected. */
 const char*names[]={"Control Surface","External MIDI"};unsigned selected=0;
 for(unsigned i=0;i<2;++i)selected+=usb_drive_control_identity("SerialMIDI",11,names[i],strlen(names[i])+1);
 assert(selected==1);
 assert(!usb_drive_control_identity("UAC2Gadget",11,"Control Surface",16));
 assert(!usb_drive_control_identity("SerialMIDI",11,"Control Surface external",25));
 assert(!usb_drive_control_identity("SerialMIDI",10,"Control Surface",16));
 assert(!usb_drive_control_identity("SerialMIDI",11,"Control Surface",15));
 assert(usb_drive_internal_transport("/sys/devices/platform/fe660000.serial/sound/card1"));
 assert(!usb_drive_internal_transport("/sys/devices/platform/usb/usb1/1-1/sound/card2"));
 assert(!usb_drive_internal_transport(NULL));
 uint32_t pixels[320*240];struct screen s={.fd=-1,.map=pixels,.pitch=1280};
 assert(fill(&s));assert(pixels[0]==0xff101419);
 assert(!strcmp(top_label("sharing",2),"Disconnect")&&!strcmp(top_label("confirm",0),"Cancel")&&!strcmp(top_label("confirm",2),"Return"));
 draw(&s,"sharing");unsigned ink=0;for(unsigned y=1;y<20;++y)for(unsigned x=215;x<318;++x)ink+=pixels[y*320+x]==0xffffffffu;assert(ink);
 draw(&s,"confirm");ink=0;for(unsigned y=1;y<20;++y)for(unsigned x=2;x<105;++x)ink+=pixels[y*320+x]==0xffffffffu;assert(ink);
 for(unsigned i=0;i<4;++i)assert(present(&s,i==0?"sharing":i==1?"confirm":i==2?"returning":"no-input"));
 assert(masters==4&&commits==4&&drops==4);
 char dir[]="/tmp/am-usb-screen-XXXXXX";assert(mkdtemp(dir));char phase[256],request[256];
 snprintf(phase,sizeof(phase),"%s/phase",dir);snprintf(request,sizeof(request),"%s/exit-request",dir);
 assert(request_exit(phase));assert(!request_exit(phase));
 FILE*f=fopen(request,"r");assert(f);char text[64];assert(fgets(text,sizeof(text),f));fclose(f);assert(!strcmp(text,"AM_USB_EXIT 1\n"));
 assert(!unlink(request));assert(!symlink("/dev/null",request));assert(!request_exit(phase));assert(!unlink(request));assert(!rmdir(dir));
 puts("PASS USB screen: fresh B3 clicks, confirmation/cancel, MIDI isolation, initial page, KMS commits, exclusive exit request");
 return 0;
}
