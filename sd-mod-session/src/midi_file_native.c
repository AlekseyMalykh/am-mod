#define _GNU_SOURCE
#include "midi_file_native.h"
#include "midi_event.h"
#include "midi_route.h"
#include "midi_tools.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>
#define F(p,o,t) (*(t*)((char*)(p)+(o)))
#define FN(o,t) ((t)(image+(o)))
static int fail(char e[96],const char*m){snprintf(e,96,"%s",m);return 0;}
int am_midi_file_root(char*out,size_t size){
 char root[1024],canonical[PATH_MAX];struct stat st;
 int fd=open("/run/mpc-trial-app/storage-meta/save-path",O_RDONLY|O_NOFOLLOW|O_CLOEXEC|O_NONBLOCK);if(fd<0)return 0;
 ssize_t n=fstat(fd,&st)||!S_ISREG(st.st_mode)||st.st_size>=(off_t)sizeof(root)?-1:read(fd,root,sizeof(root)-1);close(fd);if(n<=0)return 0;
 while(n&&(root[n-1]=='\n'||root[n-1]=='\r'))--n;
 root[n]=0;
 if(n<8||strlen(root)!=(size_t)n||strncmp(root,"/media/",7)||!realpath(root,canonical)||strcmp(root,canonical))return 0;
 return snprintf(out,size,"%s",root)>0&&strlen(root)<size;
}
int am_midi_file_load(const char*path,struct am_smf*out,char error[96]){
 char root[1024],canonical[PATH_MAX];
 if(!path||!am_midi_file_root(root,sizeof(root))||!realpath(path,canonical)||strcmp(canonical,path)||strncmp(path,root,strlen(root))||path[strlen(root)]!='/')return fail(error,"MIDI file must be on this card");
 int fd=open(path,O_RDONLY|O_NOFOLLOW|O_CLOEXEC|O_NONBLOCK);struct stat st;
 if(fd<0)return fail(error,"Could not open MIDI file");
 if(fstat(fd,&st)||!S_ISREG(st.st_mode)||st.st_size<14||st.st_size>AM_SMF_MAX_BYTES){close(fd);return fail(error,"Invalid MIDI file size");}
 size_t size=(size_t)st.st_size,at=0;unsigned char *bytes=malloc(size);if(!bytes){close(fd);return fail(error,"MIDI allocation failed");}
 while(at<size){ssize_t n=read(fd,bytes+at,size-at);if(n<0&&errno==EINTR)continue;if(n<=0)break;at+=(size_t)n;}
 close(fd);int ok=at==size?am_smf_read(bytes,size,out,error):fail(error,"Incomplete MIDI read");free(bytes);return ok;
}
void *am_midi_file_project(uintptr_t image){
 if(!image)return NULL;
 void *app=F(image,0x71e3dc8,void*);if(!app||F(app,0,uintptr_t)!=image+0x6e07288)return NULL;
 void *audio=F(app,0xd8,void*);if(!audio||F(audio,0,uintptr_t)!=image+0x6e0c430)return NULL;
 void *core=F(audio,0x148,void*);if(!core||F(core,0,uintptr_t)!=image+0x6e0cf38)return NULL;
 void *controller=F(core,0x588,void*);if(!controller||F(controller,0,uintptr_t)!=image+0x6d95318)return NULL;
 return F(controller,0x28,void*);
}
static int signature_events(uintptr_t image,void*seq,uint32_t offset,struct am_smf*d){
 void *list=F(seq,0xe8,void*);if(!list)return 0;
 void *end=(char*)list+0x30,*node=F(list,0x30,void*);unsigned budget=512;
 while(node!=end&&budget--){
  if(!node)return 0;
  int numerator=F(node,0x10,int),beat_length=F(node,0x14,int),bar=F(node,0x18,int);
  if(numerator<1||numerator>255||beat_length<1||3840%beat_length||bar<0)return 0;
  unsigned denominator=(unsigned)(3840/beat_length),power=0;
  while((1u<<power)<denominator&&power<8)++power;
  if(power>7||(1u<<power)!=denominator)return 0;
  int position[3]={bar,0,0};int64_t tick=FN(0x29bf150,int64_t(*)(void*,const int*))(list,position);
  int64_t length=FN(0x29571f0,int64_t(*)(void*))(seq);
  if(tick<0||tick>UINT32_MAX-offset)return 0;
  if(tick<length){struct am_smf_event e={.tick=offset+(uint32_t)tick,.kind=AM_SMF_SIGNATURE,.value=(uint32_t)numerator<<24|power<<16|24u<<8|8};if(!am_smf_event_add(d,e))return 0;}
  node=F(node,0,void*);
 }
 return node==end;
}
static int snapshot_sequence(uintptr_t image,void*project,void*things,void*seq,void*song,uint32_t offset,struct am_smf*d,unsigned *budget){
 if(!seq||F(seq,0,uintptr_t)!=image+0x6ecdff0)return 0;
 int64_t length=FN(0x29571f0,int64_t(*)(void*))(seq);if(length<=0||length>UINT32_MAX-offset)return 0;
 /* Native sequence snapshot295b3d0 copies BPM188 and TempoEnable491.
  * Project snapshot2915d64 copies MasterTempoAC0 and MasterEnableA68. */
 int master=!!(F(project,0xa68,unsigned char)&1)||!F(seq,0x491,unsigned char)||(song&&F(song,0x28,unsigned char));
 float bpm=master?F(project,0xac0,float):F(seq,0x188,float);
 if(!isfinite(bpm)||bpm<20.f||bpm>999.f)return 0;
 struct am_smf_event tempo={.tick=offset,.kind=AM_SMF_TEMPO,.value=(uint32_t)lround(60000000.0/bpm)};
 if(!am_smf_event_add(d,tempo)||!signature_events(image,seq,offset,d))return 0;
 void *track=F(things,0x368,void*),*program=track?F(track,0xbb8,void*):NULL;
 if(!track||!program)return 0;
 void *list=FN(0x2957ae0,void*(*)(void*,void*,int))(seq,track,0);
 if(!list){if(offset+(uint32_t)length>d->duration)d->duration=offset+(uint32_t)length;return 1;}
 void *end=(char*)list+0x68,*node=F(list,0x78,void*);unsigned ports[257]={0};
 while(node!=end){
  if(!node||!*budget)return 0;
  --*budget;
  struct step_event source;memcpy(&source,(char*)node+0x20,sizeof(source));
  if((source.type&255)==3){
   struct am_midi_event event;if(!am_midi_event_copy(&source,(am_midi_choice_get)(image+0x27e4230),AM_MIDI_EVENT_NOTE_ON,&event))return 0;
   unsigned sound=(unsigned)F(program,0x14a8+event.source_note*4,int);
   if(sound>=128||F(program,0x12a8+sound*4,int)!=event.source_note||event.tick<0||event.tick>=length||event.duration<0)return 0;
   unsigned layer=F(&source,0x14,uint16_t)&0x8001?AM_MIDI_LAYER_LEVELS:AM_MIDI_LAYER_PAD;
   struct am_midi_map map;am_midi_layer_resolve(sound,layer,&map);struct am_midi_target target;
   unsigned port=map.out_port?map.out_port:AM_MIDI_USB;
   int routed=am_midi_route_event(sound,layer,port,1+((source.type>>12)&15),&event,&target);
   if(!routed&&!event.muted&&event.velocity&&map.out_kind!=AM_MIDI_OFF)return 0;
   if(routed){
    uint16_t output_track=(uint16_t)(1+sound*2+layer);uint64_t tick=(uint64_t)offset+event.tick,duration=event.duration>0?(uint64_t)event.duration:1;
    if(tick>UINT32_MAX||duration>UINT32_MAX-tick)return 0;
    if(!ports[output_track]){struct am_smf_event p={.track=output_track,.kind=AM_SMF_PORT,.value=port-1};if(!am_smf_event_add(d,p))return 0;ports[output_track]=1;}
    if((target.on.data[0]&0xf0)==0x90){struct am_smf_note n={(uint32_t)tick,(uint32_t)duration,output_track,(uint8_t)(target.on.data[0]&15),target.on.data[1],target.on.data[2]};if(!am_smf_note_add(d,n))return 0;}
    else {struct am_smf_event e={.tick=(uint32_t)tick,.track=output_track,.kind=AM_SMF_MESSAGE,.size=target.on.size};memcpy(e.data,target.on.data,3);if(!am_smf_event_add(d,e))return 0;
     if(target.off.size){e.tick+=(uint32_t)duration;e.size=target.off.size;memcpy(e.data,target.off.data,3);if(!am_smf_event_add(d,e))return 0;}}
   }
  }
  node=FN(0xb51738,void*(*)(void*))(node);
 }
 if(offset+(uint32_t)length>d->duration)d->duration=offset+(uint32_t)length;
 return 1;
}
int am_midi_file_snapshot(uintptr_t image,void*project,void*things,void*seq,void*song,struct am_smf*out,char error[96]){
 if(!image||!project||!things||!out||F(project,0,uintptr_t)!=image+0x6ecc648)return fail(error,"MIDI project unavailable");
 struct am_smf d;am_smf_init(&d);unsigned budget=1048576;int ok=0;
 if(song){
  const int *begin=F(song,0x30,const int*),*end=F(song,0x38,const int*);uint32_t offset=0;
  if(!begin||end<begin||(end-begin)%2||end-begin>4096)goto done;
  for(const int *step=begin;step<end;step+=2){if(step[0]<0||step[0]>=128||step[1]<1||step[1]>99)goto done;
   void *sequence=FN(0x2909be0,void*(*)(void*,int))(project,step[0]);if(!sequence)goto done;
   int64_t length=FN(0x29571f0,int64_t(*)(void*))(sequence);
   for(int repeat=0;repeat<step[1];++repeat){if(!snapshot_sequence(image,project,things,sequence,song,offset,&d,&budget))goto done;if(length<=0||length>UINT32_MAX-offset)goto done;offset+=(uint32_t)length;}
  }
  ok=begin!=end;
 }else ok=snapshot_sequence(image,project,things,seq,NULL,0,&d,&budget);
done:
 if(!ok){am_smf_free(&d);return fail(error,"MIDI snapshot refused: invalid data or capacity");}
 am_smf_free(out);*out=d;*error=0;return 1;
}
static int save_document(const struct am_smf*d,const char*prefix,char path[512],char error[96]){
 char root[1024],folder[1200],canonical[PATH_MAX];if(!am_midi_file_root(root,sizeof(root)))return fail(error,"SD card unavailable");
 if(snprintf(folder,sizeof(folder),"%s/Samples/Export",root)>=(int)sizeof(folder)||!realpath(folder,canonical)||strcmp(folder,canonical))return fail(error,"SD export directory unavailable");
 if(strlen(folder)+80>=512)return fail(error,"MIDI export path too long");
 int dir=open(folder,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);if(dir<0)return fail(error,"Could not open SD export directory");
 unsigned char *bytes=NULL;size_t size=0;if(!am_smf_write(d,&bytes,&size,error)){close(dir);return 0;}
 static unsigned serial;char temp[80],name[80];snprintf(temp,sizeof(temp),".am-midi-%ld-%u.part",(long)getpid(),++serial);
 int fd=openat(dir,temp,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);size_t at=0;int ok=0;
 if(fd<0){free(bytes);close(dir);return fail(error,"Could not create MIDI export");}
 while(at<size){ssize_t n=write(fd,bytes+at,size-at);if(n<0&&errno==EINTR)continue;if(n<=0)break;at+=(size_t)n;}
 if(at==size&&!fsync(fd))ok=1;
 close(fd);free(bytes);
 if(ok){ok=0;for(unsigned i=1;i<=9999;++i){snprintf(name,sizeof(name),"%s-%03u.mid",prefix,i);
   if(!syscall(SYS_renameat2,dir,temp,dir,name,1)){ok=1;break;}if(errno!=EEXIST)break;}}
 if(!ok)unlinkat(dir,temp,0);else (void)fsync(dir);close(dir);
 if(!ok)return fail(error,"MIDI export could not be committed");
 if(snprintf(path,512,"%s/%s",folder,name)>=512)return fail(error,"MIDI export path too long");
 return 1;
}
int am_midi_file_export(uintptr_t image,void*project,void*things,void*sequence,void*song,char path[512],char error[96]){
 struct am_smf d;am_smf_init(&d);int ok=am_midi_file_snapshot(image,project,things,sequence,song,&d,error);
 if(ok)ok=save_document(&d,song?"Song-MIDI":"Sequence-MIDI",path,error);
 am_smf_free(&d);return ok;
}
