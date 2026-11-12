#define _GNU_SOURCE
#include "project_recovery.h"
#ifdef MPC_EARLY_STARTUP
#include "startup_project.h"
#endif
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/stat.h>
#define F(p,o,T) (*(T*)((char*)(p)+(o)))
#define FN(o,T) ((T)(image+(o)))
#ifndef PROJECT_RECOVERY_STORAGE
#define PROJECT_RECOVERY_STORAGE "/run/mpc-trial-app/storage-meta/save-path"
#endif
static pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;
static char captured[PROJECT_RECOVERY_BYTES],pending[PROJECT_RECOVERY_BYTES];
static unsigned generation,applied;
static unsigned named_save;
static unsigned waiting_revision;static uint64_t waiting_since;
static _Thread_local char copying[PROJECT_RECOVERY_BYTES];
static char copy_payload[PROJECT_RECOVERY_BYTES];
static unsigned copy_active;
static char notice[180];static uint64_t notice_until;
static void*save_surface;static unsigned first_save,save_prompt,save_seeded,save_owned[112];
static char save_name[129];
static uint64_t milliseconds(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000+t.tv_nsec/1000000;}
static int valid_name(const char*s){
 size_t n=strnlen(s,129);if(!n||n>128||!strcmp(s,".")||!strcmp(s,".."))return 0;
 for(size_t i=0;i<n;++i)if((unsigned char)s[i]<32||strchr("/\\:",s[i]))return 0;
 return 1;
}
static int valid_relative(const char*s){
 size_t n=strnlen(s,1024);if(!n)return 1;
 if(n>=1024||strncmp(s,"Projects/",9)||n<14||strcmp(s+n-4,".xpj"))return 0;
 const char*p=s;
 while(*p){const char*end=strchr(p,'/');size_t len=end?(size_t)(end-p):strlen(p);
  if(!len||(len==1&&p[0]=='.')||(len==2&&!memcmp(p,"..",2)))return 0;
  for(size_t i=0;i<len;++i)if((unsigned char)p[i]<32||p[i]=='\\'||p[i]==':')return 0;
  if(!end)break;
  p=end+1;
 }
 return !strstr(s,"/Mod-Exit-");
}
static char digit(unsigned n){return "0123456789abcdef"[n&15];}
static int unhex(char c){return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:-1;}
int project_recovery_encode(const char*name,const char*relative,char*out,size_t size){
 if(!name||!relative||!out||!valid_name(name)||!valid_relative(relative))return 0;
 size_t a=strlen(name),b=strlen(relative);if(size<8+2*(a+b))return 0;
 char*p=out;memcpy(p,"AMR1\n",5);p+=5;
 for(size_t i=0;i<a;++i){*p++=digit((unsigned char)name[i]>>4);*p++=digit((unsigned char)name[i]);}*p++='\n';
 for(size_t i=0;i<b;++i){*p++=digit((unsigned char)relative[i]>>4);*p++=digit((unsigned char)relative[i]);}*p++=0;return 1;
}
static int decode_part(const char*s,size_t n,char*out,size_t size){
 if(n%2||n/2>=size)return 0;
 for(size_t i=0;i<n;i+=2){int a=unhex(s[i]),b=unhex(s[i+1]);if(a<0||b<0||!(a|b))return 0;out[i/2]=(char)(a*16+b);}out[n/2]=0;return 1;
}
int project_recovery_decode(const char*p,char*name,size_t namesz,char*relative,size_t pathsz){
 if(!p||strnlen(p,PROJECT_RECOVERY_BYTES)>=PROJECT_RECOVERY_BYTES||strncmp(p,"AMR1\n",5))return 0;
 const char*line=strchr(p+5,'\n');if(!line)return 0;
 if(!decode_part(p+5,(size_t)(line-p-5),name,namesz)||!decode_part(line+1,strlen(line+1),relative,pathsz))return 0;
 if(!valid_name(name)||!valid_relative(relative))return 0;
 if(*relative){const char*file=strrchr(relative,'/');size_t n=strlen(file+1)-4;if(strlen(name)!=n||memcmp(file+1,name,n))return 0;}
 return 1;
}
static int storage_root(char*out,size_t size){
 int fd=open(PROJECT_RECOVERY_STORAGE,O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK);if(fd<0)return 0;
 struct stat st;ssize_t n=fstat(fd,&st)||!S_ISREG(st.st_mode)?-1:read(fd,out,size-1);close(fd);
 if(n<=0)return 0;
 while(n>0&&(out[n-1]=='\n'||out[n-1]=='\r'))--n;
 out[n]=0;
 char resolved[PATH_MAX];return n>7&&!strncmp(out,"/media/",7)&&strlen(out)==(size_t)n&&realpath(out,resolved)&&!strcmp(out,resolved);
}
static void*current_things(uintptr_t image,void*saver){
 void*p=F(saver,0x90,void*);return p&&F(p,0,uintptr_t)==image+0x6ecc648?p:NULL;
}
static int copy_string(const char*p,char*out,size_t size){if(!p||strnlen(p,size)>=size)return 0;memcpy(out,p,strlen(p)+1);return 1;}
static int saved_identity(const char*path,char*payload,size_t size){
 char root[1024],relative[1024]="",name[129];
 if(!path||strnlen(path,1200)>=1200)return 0;
 const char*leaf=strrchr(path,'/');if(!leaf)return 0;++leaf;
 size_t n=strlen(leaf);if(n<=4||n-4>=sizeof(name)||strcmp(leaf+n-4,".xpj"))return 0;
 memcpy(name,leaf,n-4);name[n-4]=0;
 if(!valid_name(name)||!strncmp(name,"Mod-Exit-",9)||!strcmp(name,"Temp-Project"))return 0;
 int internal=!strncmp(path,"/data/UserContent/Projects/",27);
 if(storage_root(root,sizeof(root))&&!strncmp(path,root,strlen(root))&&path[strlen(root)]=='/'&&valid_relative(path+strlen(root)+1))
  snprintf(relative,sizeof(relative),"%s",path+strlen(root)+1);
 else if(!internal)return 0;
 return project_recovery_encode(name,relative,payload,size);
}
void project_recovery_saved(const char*path){
 char payload[PROJECT_RECOVERY_BYTES];if(!saved_identity(path,payload,sizeof(payload)))return;
 pthread_mutex_lock(&mutex);memcpy(captured,payload,strlen(payload)+1);named_save=1;pending[0]=0;++generation;pthread_mutex_unlock(&mutex);
 fprintf(stderr,"PROJECT_RECOVERY NAMED_SAVE_CAPTURED path=%s\n",path);
}
void project_recovery_capture(uintptr_t image,void*saver){
 /* Save Project may write a named copy while CurrentThings still points at
  * Temp-Project or the old recovered identity. A confirmed explicit target
  * wins until a different project is loaded or New Project is created. */
 pthread_mutex_lock(&mutex);int saved=named_save&&captured[0];pthread_mutex_unlock(&mutex);
 if(saved)return;
 char root[1024],path[1200],name[129]="Recovered Project",relative[1024]="",payload[PROJECT_RECOVERY_BYTES];
 void*things=current_things(image,saver);
 if(things&&copy_string(F(things,0x9b0,const char*),path,sizeof(path))){
  /* Native shutdown autosaves to Internal Temp-Project before our UI save
   * starts. Keep the identity observed on the UI owner before that handoff.
   * Successful project loads and New Project invalidate the previous cache. */
  if(!strcmp(path,"/data/TempProjects/Temp-Project.xpj")){
   pthread_mutex_lock(&mutex);int remembered=captured[0]!=0;pthread_mutex_unlock(&mutex);
   if(remembered)return;
  }
  if(strstr(path,"/Mod-Exit-Last.xpj")){
   pthread_mutex_lock(&mutex);int remembered=pending[0]!=0;
   if(remembered)memcpy(captured,pending,sizeof(captured));
   pthread_mutex_unlock(&mutex);if(remembered)return;
  }
  const char*file=strrchr(path,'/');file=file?file+1:path;size_t n=strlen(file);
  if(n>4&&!strcmp(file+n-4,".xpj")&&n-4<sizeof(name)){
   memcpy(name,file,n-4);name[n-4]=0;
   if(storage_root(root,sizeof(root))&&!strncmp(path,root,strlen(root))&&path[strlen(root)]=='/'){
    const char*r=path+strlen(root)+1;if(valid_relative(r))snprintf(relative,sizeof(relative),"%s",r);
   }
  }
 }
 /* Legacy recovery files have no reliable original identity: never guess it. */
 if(!strncmp(name,"Mod-Exit-",9)||!strcmp(name,"Temp-Project"))snprintf(name,sizeof(name),"Recovered Project");
 if(!project_recovery_encode(name,relative,payload,sizeof(payload)))project_recovery_encode("Recovered Project","",payload,sizeof(payload));
 pthread_mutex_lock(&mutex);memcpy(captured,payload,strlen(payload)+1);pthread_mutex_unlock(&mutex);
}
/* Native SaveProject queues serialization on a different worker. Publish
 * an immutable copy until its retained write futures finish. The UI is
 * blocked during this operation; no manual Save can overlap the window.
 * A native temporary autosave may also retain the same recovery identity. */
void project_recovery_begin_copy(void){pthread_mutex_lock(&mutex);memcpy(copy_payload,captured,sizeof(copy_payload));copy_active=1;pthread_mutex_unlock(&mutex);}
void project_recovery_end_copy(void){pthread_mutex_lock(&mutex);copy_active=0;pthread_mutex_unlock(&mutex);}
const char*project_recovery_copy_payload(void){
 pthread_mutex_lock(&mutex);int active=copy_active;
 if(active)memcpy(copying,copy_payload,sizeof(copying));
 pthread_mutex_unlock(&mutex);
 return active&&copying[0]?copying:NULL;
}
void project_recovery_loaded(const char*payload){
 char name[129],relative[1024];int valid=project_recovery_decode(payload,name,sizeof(name),relative,sizeof(relative));
 pthread_mutex_lock(&mutex);captured[0]=pending[0]=0;named_save=0;if(valid)memcpy(pending,payload,strlen(payload)+1);++generation;pthread_mutex_unlock(&mutex);
}
void project_recovery_reset(void){project_recovery_loaded(NULL);notice[0]=0;notice_until=0;first_save=save_prompt=save_seeded=0;}
const char*project_recovery_notice(void){return milliseconds()<notice_until?notice:NULL;}
void project_recovery_tick(uintptr_t image,void*services){
 char payload[PROJECT_RECOVERY_BYTES],name[129],relative[1024],root[1024],target[1200];unsigned revision;
 pthread_mutex_lock(&mutex);revision=generation;memcpy(payload,pending,sizeof(payload));pthread_mutex_unlock(&mutex);
 if(!services)return;
 if(revision!=applied&&!payload[0]){applied=revision;waiting_since=0;notice_until=0;first_save=save_prompt=save_seeded=0;}
 /* Called on the queued UI owner. Never change identity while a load dialog
  * is active or before the native full-project load has set the recovery path. */
 if(FN(0x10f87a0,void*(*)(int))(0)){waiting_since=0;return;}
 void*lock=(char*)services+0x38;if(FN(0xb51828,int(*)(void*))(lock))return;
 uintptr_t key=image+0x6e0d598;void**entry=FN(0x143ad50,void**(*)(void*,const uintptr_t*))(services,&key);
 void*saver=entry?*entry:NULL;FN(0xb51808,void(*)(void*))(lock);
 if(!saver||F(saver,0,uintptr_t)!=image+0x6d8cc00)return;
 void*things=current_things(image,saver);if(!things)return;
 char path[1200];if(!copy_string(F(things,0x9b0,const char*),path,sizeof(path)))return;
 /* Cache real saved projects on the UI thread, never the native
  * temporary autosave path or the recovery file's technical identity. */
 if(!strncmp(path,"/data/UserContent/Projects/",27)||
    (storage_root(root,sizeof(root))&&!strncmp(path,root,strlen(root))&&path[strlen(root)]=='/'&&valid_relative(path+strlen(root)+1)))
  project_recovery_capture(image,saver);
 if(revision==applied||!payload[0])return;
 const char*file=strrchr(path,'/');if(!file||strcmp(file+1,"Mod-Exit-Last.xpj")){waiting_since=0;return;}
 if(!project_recovery_decode(payload,name,sizeof(name),relative,sizeof(relative))||!storage_root(root,sizeof(root)))return;
 char recovery_path[1200];snprintf(recovery_path,sizeof(recovery_path),"%s/Projects/Mod-Exit-Last.xpj",root);
 if(strcmp(path,recovery_path))return;
 if(waiting_revision!=revision||!waiting_since){waiting_revision=revision;waiting_since=milliseconds();return;}
 if(milliseconds()-waiting_since<2500)return;
 int written=relative[0]?snprintf(target,sizeof(target),"%s/%s",root,relative):snprintf(target,sizeof(target),"%s/Projects/%s.xpj",root,name);
 if(written>=(int)sizeof(target))return;
 /* Validate the destination folder, not the source file: the original may have
  * been renamed/deleted. A missing/linked folder falls back to SD Projects. */
 char parent[1200],resolved[PATH_MAX];snprintf(parent,sizeof(parent),"%s",target);char*slash=strrchr(parent,'/');if(!slash)return;*slash=0;
 if(!realpath(parent,resolved)||strcmp(parent,resolved)){
  if(snprintf(target,sizeof(target),"%s/Projects/%s.xpj",root,name)>=(int)sizeof(target))return;
  snprintf(parent,sizeof(parent),"%s/Projects",root);
  if(!realpath(parent,resolved)||strcmp(parent,resolved))return;
 }
 struct stat destination;
 if(lstat(target,&destination)){if(errno!=ENOENT)return;}
 else if(!S_ISREG(destination.st_mode))return;
 uint64_t value=0;FN(0xf535c0,void(*)(uint64_t*,const char*))(&value,target);
 /* Native String/File property setter notifies the UI. Preserve +9b8/+a08:
  * it is the recovery data directory used by samples. Do not call290cd00,
  * which would also redirect that directory to the original project. */
 FN(0x13bf450,void(*)(void*,const uint64_t*))((char*)things+0x960,&value);
 /* Match290cd00's basename-without-extension display property. It stores
  * a project name, while +960 stores a File; do not put a path in the name. */
 FN(0xf0a290,void(*)(uint64_t*))(&value);value=0;
 FN(0xf535c0,void(*)(uint64_t*,const char*))(&value,name);
 FN(0x13bf450,void(*)(void*,const uint64_t*))((char*)things+0x908,&value);
 FN(0xf0a290,void(*)(uint64_t*))(&value);
 /* preferences+0xa8 is the native autosave destination, not the Save
  * editor default. Leave Temp-Project there: pointing it at the original
  * would let native background/shutdown saves overwrite it without consent.
  * Seed the visible Save editor separately when that screen opens. */
 applied=revision;first_save=1;save_prompt=save_seeded=0;snprintf(save_name,sizeof(save_name),"%s",name);snprintf(notice,sizeof(notice),"Restored: %s",name);notice_until=milliseconds()+5000;
#ifdef MPC_EARLY_STARTUP
 startup_project_changed();
#endif
 fprintf(stderr,"PROJECT_RECOVERY IDENTITY_RESTORED native_data_directory_preserved\n");
}
static void paint_text(uintptr_t image,void*g,const char*s,int x,int y,int w,int h){
 uint64_t text=0,font=0;FN(0xf535c0,void(*)(uint64_t*,const char*))(&text,s);
 FN(0x1082810,void(*)(uint64_t*,int,float))(&font,0,12.f);FN(0x102dca0,void(*)(void*,const uint64_t*))(g,&font);
 FN(0x10817b0,void(*)(void*,const uint64_t*,int,int,int,int,int,int,float))(g,&text,x,y,w,h,0x24,1,.7f);
 FN(0x1070d60,void(*)(uint64_t*))(&font);FN(0xf0a290,void(*)(uint64_t*))(&text);
}
enum save_screen_kind { SAVE_OTHER, SAVE_PROJECT, SAVE_SAMPLE };
static enum save_screen_kind save_kind(uintptr_t image,void *p){
 if(!p)return SAVE_OTHER;
 uintptr_t type=F(p,0,uintptr_t)-image;
 if(type==0x6e1faa0)return SAVE_PROJECT;
 if(type==0x6e22e18)return SAVE_SAMPLE;
 return SAVE_OTHER;
}
static int save_screen_visible(uintptr_t image,void *p){
 return save_kind(image,p)!=SAVE_OTHER&&FN(0x112fd50,int(*)(void*))(p);
}
static unsigned storage_offset(uintptr_t image,void *p){
 return save_kind(image,p)==SAVE_SAMPLE?0x11b8:0x11a0;
}
int project_recovery_save_external(uintptr_t image,void *p){
 if(!save_screen_visible(image,p))return 0;
 unsigned storage=storage_offset(image,p);
 /* The native setter separates the requested position from the effective
  * destination and clears External when the card is unavailable. */
 if(!F(p,storage+1,unsigned char)){
  uintptr_t setter=storage==0x11b8?0x1683280:0x16480e0;
  FN(setter,void(*)(void*,float))(p,1.f);
 }
 return F(p,storage+1,unsigned char)!=0;
}
int project_recovery_save_k3_external(uintptr_t image,void *p){
 return save_screen_visible(image,p)&&!FN(0x112f6b0,int(*)(void*))(p);
}
int project_recovery_save_control(uintptr_t image,void *p,unsigned cc){
 if(cc!=0x12||!save_screen_visible(image,p))return 0;
 {int busy=FN(0x112f6b0,int(*)(void*))(p);static unsigned lines;
  if(lines<40){++lines;fprintf(stderr,"SAVE_K3 busy=%d external_before=%u\n",busy,(unsigned)F(p,storage_offset(image,p)+1,unsigned char));}
  if(busy)return 0;}
 project_recovery_save_external(image,p);
 return 1;
}
const char*phone_native_erase_label(void) __attribute__((weak));
void project_recovery_save_paint(uintptr_t image,void*p,void*g){
 if(!save_screen_visible(image,p))return;
 save_surface=p;
 /* SaveProjectScreen caches its editor/storage separately from Things.
  * Seed its native controls once, then preserve subsequent user edits. */
 unsigned storage=storage_offset(image,p);
 {static unsigned last=~0u;unsigned now=(unsigned)save_kind(image,p)|(F(p,storage,unsigned char)<<4)|(F(p,storage+1,unsigned char)<<12);
  if(now!=last){last=now;fprintf(stderr,"SAVE_SCREEN kind=%u seeded=%u external=%u busy=%d\n",(unsigned)save_kind(image,p),
   (unsigned)F(p,storage,unsigned char),(unsigned)F(p,storage+1,unsigned char),FN(0x112f6b0,int(*)(void*))(p));}}
 /* Seed External once per screen opening, even if the native flag byte is
  * already set: the K3 cell otherwise keeps showing "Internal". */
 {static void *seeded_screen;static unsigned seeded_frames;
  if(seeded_screen!=p){seeded_screen=p;seeded_frames=0;}
  if(seeded_frames<3){++seeded_frames;
   if(!F(p,storage+1,unsigned char)||seeded_frames==1){
    uintptr_t setter=storage==0x11b8?0x1683280:0x16480e0;
    FN(setter,void(*)(void*,float))(p,1.f);
    FN(0x1149880,void(*)(void*,uint64_t,uint64_t))(p,0,F(p,0x28,uint64_t));}}}
 int external=F(p,storage+1,unsigned char)!=0;
 int project=save_kind(image,p)==SAVE_PROJECT;
 if(project&&first_save&&!save_seeded&&F(p,0x9f8,uintptr_t)==image+0x6e18e20){
  save_seeded=1;uint64_t value=0;FN(0xf535c0,void(*)(uint64_t*,const char*))(&value,save_name);
  FN(0x11c4ec0,void(*)(void*,const uint64_t*,int))((char*)p+0x9f8,&value,1);
  FN(0x15bdb10,void(*)(void*))((char*)p+0x9f8);
  FN(0xf0a290,void(*)(uint64_t*))(&value);
 }
 if(!external){
  FN(0x102d5e0,void(*)(void*,uint32_t))(g,0xff000000);
  FN(0x102e000,void(*)(void*,int,int,int,int))(g,0,170,320,27);
  FN(0x102e000,void(*)(void*,int,int,int,int))(g,214,0,106,30);
  FN(0x102d5e0,void(*)(void*,uint32_t))(g,0xff808080);
  paint_text(image,g,"Save",214,4,106,23);
  FN(0x102d5e0,void(*)(void*,uint32_t))(g,0xffffc247);
  paint_text(image,g,"Insert SD card to save",3,172,314,23);
  return;
 }
 {const char*clear=phone_native_erase_label?phone_native_erase_label():0;
  if(clear){ /* pad input: the B2 Erase key reads Clear, then Sure? until confirmed */
   FN(0x102d5e0,void(*)(void*,uint32_t))(g,0xff000000);
   FN(0x102e000,void(*)(void*,int,int,int,int))(g,107,0,107,30);
   FN(0x102d5e0,void(*)(void*,uint32_t))(g,0xffffffff);
   paint_text(image,g,clear,107,4,107,23);}}
 if(!project||!first_save||!save_prompt)return;
 FN(0x102d5e0,void(*)(void*,uint32_t))(g,0xff000000);
 FN(0x102e000,void(*)(void*,int,int,int,int))(g,0,0,106,30);
 FN(0x102e000,void(*)(void*,int,int,int,int))(g,214,0,106,30);
 FN(0x102e000,void(*)(void*,int,int,int,int))(g,0,170,320,27);
 FN(0x102d5e0,void(*)(void*,uint32_t))(g,0xffffc247);
 paint_text(image,g,"Review",0,4,106,23);paint_text(image,g,"Save",214,4,106,23);
 paint_text(image,g,"Save recovered project?",3,170,314,13);
 FN(0x102d5e0,void(*)(void*,uint32_t))(g,0xffffffff);
 paint_text(image,g,"Existing project may be replaced.",3,184,314,13);
}
int project_recovery_save_button(uintptr_t image,void*p,void*event){
 unsigned code=F(event,8,unsigned char),down=F(event,9,unsigned char)!=0;if(code>=112)return 0;
 if(save_owned[code]){if(!down)save_owned[code]=0;return 1;}
 if(!down||p!=save_surface||!save_screen_visible(image,p)||FN(0x112f6b0,int(*)(void*))(p))return 0;
 int external_ok=1;
 if(code==0x0e){external_ok=project_recovery_save_external(image,p);fprintf(stderr,"SAVE_BUTTON code=0e external_ok=%d kind=%u\n",external_ok,(unsigned)save_kind(image,p));}
 if(code==0x0e&&!external_ok){
  save_owned[code]=1;
  FN(0x1149880,void(*)(void*,uint64_t,uint64_t))(p,0,F(p,0x28,uint64_t));return 1;
 }
 if(save_kind(image,p)!=SAVE_PROJECT||!first_save)return 0;
 if(code==0x0e){
  if(save_prompt){save_prompt=0;first_save=0;return 0;} /* forward this real down and its matching release */
  save_prompt=1;save_owned[code]=1;
 }else if(code==0x0c&&save_prompt){save_prompt=0;save_owned[code]=1;}
 else if(code==0x0c){save_seeded=0;return 0;} /* Native Cancel; seed again on the next entry. */
 else return 0;
 FN(0x1149880,void(*)(void*,uint64_t,uint64_t))(p,0,F(p,0x28,uint64_t));return 1;
}
