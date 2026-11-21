/* Select the SD project at the FIRST native full-project load. The native
 * loader still owns sample staging, events, effects, errors and Temp-Project
 * autosave. No project bytes or persistent preferences are rewritten here. */
#include "startup_project.h"
#include "internal_storage.h"
extern "C" int internal_storage_project_path(const char*) __attribute__((weak));
extern "C" int internal_storage_enabled(void) __attribute__((weak));
extern "C" int internal_storage_sample_path(const char*) __attribute__((weak));
#ifdef MPC_PROJECT_RECOVERY
#include "project_recovery.h"
#endif
#include <atomic>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cerrno>
#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>
#ifndef STARTUP_STORAGE_META
#define STARTUP_STORAGE_META "/run/mpc-trial-app/storage-meta/save-path"
#endif
#ifndef STARTUP_AUTOSAVE_PATH
#define STARTUP_AUTOSAVE_PATH "/data/TempProjects/Temp-Project.xpj"
#endif
#ifndef STARTUP_TEMP_ROOT
#define STARTUP_TEMP_ROOT "/data/TempProjects"
#endif
#ifndef STARTUP_USB_RESUME_PATH
#define STARTUP_USB_RESUME_PATH "/run/mpc-trial-app/usb-resume-project"
#endif
namespace {
uintptr_t image;
std::atomic<unsigned> phase{0}; /* 0 uninstalled, 1 waiting, 2 native load, 3 done */
std::atomic<bool> workspace_checked{false};
/* The stock initial Temp/New Project path prepares this directory. Selecting
 * an SD XPJ on the first load can bypass that preparation. Library import,
 * Copy Pad and render all write here, independently of the displayed XPJ.
 * Restore that shared workspace at project boundaries, not per-pad copies.
 * Never create/rename a user project, seed an XPJ, follow a final symlink,
 * clear existing audio, or alter the native project/auto-save identity. */
bool prepare_workspace(){
 int saved=errno,error=0,created=0;
 if(mkdir(STARTUP_TEMP_ROOT,0755)&&errno!=EEXIST)error=errno;
 int fd=error?-1:open(STARTUP_TEMP_ROOT,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
 if(!error&&fd<0)error=errno;
 if(fd>=0){
  const char*name="Temp-Project_[ProjectData]";
  if(!mkdirat(fd,name,0755))created=1;
  else if(errno!=EEXIST)error=errno;
  struct stat st;
  if(!error){if(fstatat(fd,name,&st,AT_SYMLINK_NOFOLLOW))error=errno;
   else if(!S_ISDIR(st.st_mode))error=ENOTDIR;}
  close(fd);
 }
 if(error)std::fprintf(stderr,"STARTUP_PROJECT TEMP_WORKSPACE_FAILED errno=%d\n",error);
 else if(created)std::fprintf(stderr,"STARTUP_PROJECT TEMP_WORKSPACE_CREATED path=%s/Temp-Project_[ProjectData]\n",STARTUP_TEMP_ROOT);
 errno=saved;return !error;
}
/* Stock Load Project leaves every audio file of the project in the shared
 * Temp-Project_[ProjectData] workspace; the recorder, Library import and render
 * choose the next free "Sample NNN" name by looking there. Loading the XPJ in
 * place leaves that folder empty, so the first take is named "Sample 001" again
 * and the native SamplePool hands back the older sample with the same name.
 * Add the missing audio files to the workspace as plain copies. Nothing already
 * there is touched, symlinks and sub-folders are skipped, the project folder is
 * only read. */
void stage_project_data(const char*xpj){
 size_t n=xpj?std::strlen(xpj):0;
 if(n<=4||std::strcmp(xpj+n-4,".xpj")||n+14>=PATH_MAX)return;
 char data[PATH_MAX];std::snprintf(data,sizeof(data),"%.*s_[ProjectData]",(int)(n-4),xpj);
 DIR*source=opendir(data);if(!source){std::fprintf(stderr,"STARTUP_PROJECT STAGE_DATA none path=%s errno=%d\n",data,errno);return;}
 int sfd=dirfd(source);
 int wfd=open(STARTUP_TEMP_ROOT "/Temp-Project_[ProjectData]",O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
 if(wfd<0){std::fprintf(stderr,"STARTUP_PROJECT STAGE_DATA workspace errno=%d\n",errno);closedir(source);return;}
 static char buffer[1<<20];
 unsigned copied=0,present=0,failed=0;unsigned long long bytes=0;
 while(dirent*entry=readdir(source)){
  const char*name=entry->d_name;
  if(!*name||name[0]=='.'||std::strchr(name,'/'))continue;
  struct stat st;
  if(fstatat(sfd,name,&st,AT_SYMLINK_NOFOLLOW)||!S_ISREG(st.st_mode))continue;
  int in=openat(sfd,name,O_RDONLY|O_NOFOLLOW|O_CLOEXEC);if(in<0){++failed;continue;}
  int out=openat(wfd,name,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0644);
  if(out<0){if(errno==EEXIST)++present;else ++failed;close(in);continue;}
  bool ok=true;
  for(;;){
   ssize_t got=read(in,buffer,sizeof(buffer));
   if(got<0){if(errno==EINTR)continue;ok=false;break;}
   if(!got)break;
   for(ssize_t done=0;done<got;){ssize_t put=write(out,buffer+done,(size_t)(got-done));if(put<0){if(errno==EINTR)continue;ok=false;break;}done+=put;}
   if(!ok)break;
   bytes+=(unsigned long long)got;
  }
  close(in);
  if(close(out))ok=false;
  if(ok)++copied;else{unlinkat(wfd,name,0);++failed;}
 }
 close(wfd);closedir(source);
 std::fprintf(stderr,"STARTUP_PROJECT STAGE_DATA copied=%u present=%u failed=%u bytes=%llu\n",copied,present,failed,bytes);
}
char selected[PATH_MAX];
/* Native std::vector<error> uses x8 for its nontrivial indirect result.
 * The native caller owns destruction. This wrapper transfers its three words
 * without substituting a different container implementation. */
struct Errors { uintptr_t begin,end,capacity; ~Errors(){} };
using Load=Errors(*)(void*,const uint64_t*);
Load original;
Errors native_load(void*saver,const uint64_t*file){
 workspace_checked.store(false);
 prepare_workspace();
 Errors result=original(saver,file);
 /* Native project cleanup may remove the old temporary workspace. */
 prepare_workspace();workspace_checked.store(true);
 return result;
}
using AutoSave=void(*)(void*);
AutoSave original_autosave;
bool card_root(char (&root)[PATH_MAX]){
 char canonical[PATH_MAX];struct stat st;
 int fd=open(STARTUP_STORAGE_META,O_RDONLY|O_NOFOLLOW|O_CLOEXEC|O_NONBLOCK);if(fd<0)return false;
 ssize_t n=fstat(fd,&st)||!S_ISREG(st.st_mode)?-1:read(fd,root,sizeof(root)-1);close(fd);
 if(n<=0)return false;
 while(n&&(root[n-1]=='\n'||root[n-1]=='\r'))--n;
 root[n]=0;
 if(n<8||std::strlen(root)!=(size_t)n||std::strncmp(root,"/media/",7)||!realpath(root,canonical)||std::strcmp(root,canonical))return false;
 return true;
}
bool project_save_destination(const char *target){
 if(!target||!*target)return false;
 if(!std::strcmp(target,STARTUP_AUTOSAVE_PATH))return true;
 if(internal_storage_project_path&&internal_storage_project_path(target))return true;
 char root[PATH_MAX],parent[PATH_MAX],canonical[PATH_MAX];
 if(!card_root(root))return false;
 const size_t root_length=std::strlen(root);
 if(std::strncmp(target,root,root_length)||std::strncmp(target+root_length,"/Projects/",10))return false;
 const char *leaf=std::strrchr(target,'/');
 if(!leaf||!leaf[1]||!std::strcmp(leaf+1,".")||!std::strcmp(leaf+1,".."))return false;
 const size_t length=static_cast<size_t>(leaf-target);
 if(length>=sizeof(parent))return false;
 std::memcpy(parent,target,length);parent[length]=0;
 if(!realpath(parent,canonical)||std::strcmp(parent,canonical))return false;
 struct stat st;
 return lstat(target,&st)?errno==ENOENT:S_ISREG(st.st_mode);
}
using Save=int(*)(void*,const uint64_t*,bool,bool,bool);
Save original_save;
struct File {
 uint64_t value=0;
 explicit File(const char*path){reinterpret_cast<void(*)(uint64_t*,const char*)>(image+0xf535c0)(&value,path);}
 ~File(){reinterpret_cast<void(*)(uint64_t*)>(image+0xf0a290)(&value);}
};
int save(void*saver,const uint64_t*file,bool a,bool b,bool c){
 char target[1200]={0};
 const char*incoming=file?reinterpret_cast<const char*>(*file):nullptr;
 if(incoming&&strnlen(incoming,sizeof(target))<sizeof(target))std::snprintf(target,sizeof(target),"%s",incoming);
#ifdef MPC_PROJECT_RECOVERY
 char restored[1200];int redirect=project_recovery_save_target(target,restored,sizeof(restored));
 if(redirect)std::snprintf(target,sizeof(target),"%s",restored);
#endif
 {struct stat seen;std::fprintf(stderr,"SAVE_TARGET path=%s exists=%d redirect=%d\n",target,lstat(target,&seen)==0,
#ifdef MPC_PROJECT_RECOVERY
   redirect
#else
   0
#endif
   );}
 if(!project_save_destination(target)){
  std::fprintf(stderr,"STARTUP_PROJECT SAVE_REQUIRES_PERSISTENT_STORAGE\n");
  return 0;
 }
 int result;
#ifdef MPC_PROJECT_RECOVERY
 if(redirect){File chosen(target);result=original_save(saver,&chosen.value,a,b,c);}
 else
#endif
 result=original_save(saver,file,a,b,c);
#ifdef MPC_PROJECT_RECOVERY
 if(result&&target[0])project_recovery_saved(target);
#endif
 return result;
}
void autosave(void*saver){
 /* Native New Project removes the old Temp XPJ. The next timer expects an
  * existing file before it will save, although its native SaveProject method
  * can create one. Bootstrap that one missing internal file from the CURRENT
  * project; never seed it by copying the previously loaded SD project. */
 if(phase.load()==3&&saver){
  auto*bytes=static_cast<char*>(saver);
  void*things=*reinterpret_cast<void**>(bytes+0x90);
  void*prefs=*reinterpret_cast<void**>(bytes+0x50);
  if(things&&prefs&&*reinterpret_cast<uintptr_t*>(things)==image+0x6ecc648){
   const char*current=*reinterpret_cast<const char**>(static_cast<char*>(things)+0x9b0);
   const char*destination=*reinterpret_cast<const char**>(static_cast<char*>(prefs)+0xa8);
   struct stat st;
   if(current&&destination&&!std::strcmp(current,STARTUP_AUTOSAVE_PATH)&&
      !std::strcmp(destination,STARTUP_AUTOSAVE_PATH)&&lstat(current,&st)&&errno==ENOENT){
    File target(current);
    int ok=reinterpret_cast<int(*)(void*,const uint64_t*,bool,bool,bool)>(image+0x1508bc0)
        (saver,&target.value,false,false,false);
    std::fprintf(stderr,"STARTUP_PROJECT TEMP_AUTOSAVE_INITIALIZED ok=%d\n",ok);
    if(ok)return; /* This is the timer's save, not an extra duplicate write. */
   }
  }
 }
 original_autosave(saver); /* Existing files and real write failures remain native. */
}
bool regular(const char*path,struct stat&st){
 char resolved[PATH_MAX];
 return !lstat(path,&st)&&S_ISREG(st.st_mode)&&st.st_size>0&&realpath(path,resolved)&&!std::strcmp(path,resolved);
}
void scan(const char*folder,unsigned depth,time_t&best){
 DIR*d=opendir(folder);if(!d)return;
 while(auto*e=readdir(d)){
  if(e->d_name[0]=='.'||!std::strncmp(e->d_name,"Mod-Exit-saving-",16)||!std::strcmp(e->d_name,"Mod-Exit-Last-old"))continue;
  char path[PATH_MAX];if(std::snprintf(path,sizeof(path),"%s/%s",folder,e->d_name)>=(int)sizeof(path))continue;
  struct stat st;if(lstat(path,&st))continue;
  if(depth&&S_ISDIR(st.st_mode)){scan(path,depth-1,best);continue;}
  size_t n=std::strlen(e->d_name);if(n<5||std::strcmp(e->d_name+n-4,".xpj")||!regular(path,st))continue;
  /* Stable tie break; never depend on filesystem enumeration order. */
  if(!selected[0]||st.st_mtime>best||(st.st_mtime==best&&std::strcmp(path,selected)>0)){
   best=st.st_mtime;std::snprintf(selected,sizeof(selected),"%s",path);
  }
 }
 closedir(d);
}
bool resume_picked;
void choose_sd(){
 char root[PATH_MAX],canonical[PATH_MAX];
 if(!card_root(root))return;
 /* USB Drive restarts the app after the card returns. Load the exact copy
  * made before handoff, even if the PC added a newer XPJ. Consume once; later
  * ordinary launches keep their usual newest-project selection. */
 const char*resume=STARTUP_USB_RESUME_PATH;
 int fd=open(resume,O_RDONLY|O_NOFOLLOW|O_CLOEXEC|O_NONBLOCK);
 if(fd>=0){
  char path[PATH_MAX]={0};struct stat st;ssize_t n=fstat(fd,&st)||!S_ISREG(st.st_mode)?-1:read(fd,path,sizeof(path)-1);close(fd);unlink(resume);
  size_t root_size=std::strlen(root);
  if(n>(ssize_t)(root_size+10)&&std::strlen(path)==(size_t)n&&!std::strncmp(path,root,root_size)&&!std::strncmp(path+root_size,"/Projects/",10)&&
     !std::strstr(path,"/../")&&realpath(path,canonical)&&!std::strcmp(path,canonical)&&!lstat(path,&st)&&regular(path,st)){
   std::snprintf(selected,sizeof(selected),"%s",path);std::fprintf(stderr,"STARTUP_PROJECT USB_RESUME_SELECTED\n");resume_picked=true;return;
  }
 }
 char projects[PATH_MAX];if(std::snprintf(projects,sizeof(projects),"%s/Projects",root)>=(int)sizeof(projects))return;
 if(!realpath(projects,canonical)||std::strcmp(projects,canonical))return;
 time_t best=0;scan(projects,1,best);
}
void choose(){
 selected[0]=0;choose_sd();
 if(resume_picked)return;
 /* The newest project wins regardless of which firmware saved it. Stock and
  * SampQOL keep projects on the real flash (visible read-only below our
  * overlay): Temp-Project.xpj and named Internal projects. Candidates: newest
  * SD project, newest Internal project, Temp-Project. Empty selection means the
  * native boot load opens Temp-Project. */
 time_t sd_time=0;char sd_path[PATH_MAX];sd_path[0]=0;
 if(selected[0]){struct stat st;if(!lstat(selected,&st))sd_time=st.st_mtime;std::snprintf(sd_path,sizeof(sd_path),"%s",selected);}
 selected[0]=0;time_t flash_time=0;scan("/data/UserContent/Projects",1,flash_time);
 char flash_path[PATH_MAX];std::snprintf(flash_path,sizeof(flash_path),"%s",selected);
 struct stat ts;time_t temp_time=regular(STARTUP_AUTOSAVE_PATH,ts)?ts.st_mtime:0;
 std::fprintf(stderr,"STARTUP_PROJECT CANDIDATES sd=%lld(%s) internal=%lld(%s) temp=%lld\n",(long long)sd_time,sd_path,(long long)flash_time,flash_path,(long long)temp_time);
 selected[0]=0;
 if(sd_path[0]&&sd_time>=flash_time&&sd_time>temp_time){std::snprintf(selected,sizeof(selected),"%s",sd_path);std::fprintf(stderr,"STARTUP_PROJECT PICK sd\n");}
 else if(flash_path[0]&&flash_time>=temp_time){std::snprintf(selected,sizeof(selected),"%s",flash_path);std::fprintf(stderr,"STARTUP_PROJECT PICK internal\n");}
 else std::fprintf(stderr,"STARTUP_PROJECT PICK temp\n");
}
Errors load(void*saver,const uint64_t*file){
 unsigned waiting=1;
 if(!phase.compare_exchange_strong(waiting,2))return native_load(saver,file);
 struct Finished { ~Finished(){phase.store(3);} } finished;
 const char*incoming=file?reinterpret_cast<const char*>(*file):nullptr;
 /* Only native boot recovery is replaced. Any unexpected first caller
  * passes through unchanged; later manual Load/New Project stay native. */
 bool boot=incoming&&!std::strcmp(incoming,"/data/TempProjects/Temp-Project.xpj");
 std::fprintf(stderr,"STARTUP_PROJECT FIRST_NATIVE_LOAD path=%s selected=%u\n",incoming?incoming:"(null)",boot&&selected[0]);
 if(!boot||!selected[0]){
  Errors result=native_load(saver,file);phase.store(3);
  std::fprintf(stderr,"STARTUP_PROJECT NATIVE_LOAD_FINISHED sd=0 errors=%u\n",result.begin!=result.end);return result;
 }
 File chosen(selected);
 std::fprintf(stderr,"STARTUP_PROJECT SINGLE_LOAD path=%s\n",selected);
 Errors result=native_load(saver,&chosen.value);phase.store(3);
 std::fprintf(stderr,"STARTUP_PROJECT NATIVE_LOAD_FINISHED sd=1 errors=%u\n",result.begin!=result.end);
 if(result.begin==result.end)stage_project_data(selected);
#ifdef MPC_PROJECT_RECOVERY
 if(result.begin==result.end)project_recovery_adopt_name(image,saver,selected);
#endif
 return result;
}
}
extern "C" int startup_song_export_ready(void){
 if(internal_storage_enabled&&internal_storage_enabled()&&internal_storage_sample_path)
  return internal_storage_sample_path("/data/UserContent/Samples/User/Export/.am-export-check.wav");
 char root[PATH_MAX],folder[PATH_MAX];
 if(!card_root(root)||std::snprintf(folder,sizeof(folder),"%s/Samples/Export",root)>=(int)sizeof(folder)){
  std::fprintf(stderr,"STARTUP_PROJECT SONG_EXPORT_NOT_READY reason=card_root\n");return 0;}
 const char *native="/data/UserContent/Samples/User/Export";
 struct stat card,alias;
 struct statvfs fs;
 int card_ok=!stat(folder,&card)&&S_ISDIR(card.st_mode);
 int alias_ok=!stat(native,&alias);
 int same=card_ok&&alias_ok&&card.st_dev==alias.st_dev&&card.st_ino==alias.st_ino;
 int writable=!statvfs(native,&fs)&&!(fs.f_flag&ST_RDONLY);
 if(!(card_ok&&alias_ok&&same&&writable))std::fprintf(stderr,"STARTUP_PROJECT SONG_EXPORT_NOT_READY card_dir=%d native=%d same_inode=%d writable=%d folder=%s\n",card_ok,alias_ok,same,writable,folder);
 return card_ok&&alias_ok&&same&&writable;
}
extern "C" int startup_song_export_external(const char*path){
 char root[PATH_MAX],prefix[PATH_MAX];
 if(!path||!card_root(root)||std::snprintf(prefix,sizeof(prefix),"%s/Samples/User/Export/",root)>=(int)sizeof(prefix))return 0;
 size_t n=std::strlen(prefix);
 if(std::strncmp(path,prefix,n)||std::strstr(path+n,".."))return 0;
 prefix[n-1]=0;struct stat st;struct statvfs fs;
 return !stat(prefix,&st)&&S_ISDIR(st.st_mode)&&!statvfs(prefix,&fs)&&!(fs.f_flag&ST_RDONLY);
}
extern "C" int startup_song_export_target(const char*path,char*out,size_t size){
 static const char internal[]="/data/UserContent/Samples/User/Export/";
 if(path&&out&&size&&!std::strncmp(path,internal,sizeof(internal)-1)&&internal_storage_enabled&&internal_storage_enabled()){
  if(!internal_storage_sample_path||!internal_storage_sample_path(path))return 0;
  return std::snprintf(out,size,"%s",path)<(int)size;
 }
 char root[PATH_MAX],dir[PATH_MAX];
 if(!path||!out||!size||!card_root(root))return 0;
 if(std::snprintf(dir,sizeof(dir),"%s/Samples/User/Export/",root)>=(int)sizeof(dir))return 0;
 const char*rest=nullptr;
 if(!std::strncmp(path,internal,sizeof(internal)-1)&&startup_song_export_ready())rest=path+sizeof(internal)-1;
 else if(!std::strncmp(path,dir,std::strlen(dir)))rest=path+std::strlen(dir);
 if(!rest||!*rest||std::strstr(rest,".."))return 0;
 char folder[PATH_MAX];std::snprintf(folder,sizeof(folder),"%s",dir);folder[std::strlen(folder)-1]=0;
 struct stat st;struct statvfs fs;
 if(stat(folder,&st)){if(mkdir(folder,0777)&&errno!=EEXIST)return 0;}
 if(stat(folder,&st)||!S_ISDIR(st.st_mode)||statvfs(folder,&fs)||(fs.f_flag&ST_RDONLY))return 0;
 return std::snprintf(out,size,"%s%s",dir,rest)<(int)size;
}
extern "C" int startup_song_export_native_file(const char*path,char*out,size_t size){
 static const char internal[]="/data/UserContent/Samples/User/Export/";
 if(path&&out&&size&&!std::strncmp(path,internal,sizeof(internal)-1)&&internal_storage_enabled&&internal_storage_enabled()){
  if(!internal_storage_sample_path||!internal_storage_sample_path(path))return 0;
  return std::snprintf(out,size,"%s",path)<(int)size;
 }
 char root[PATH_MAX];
 if(!path||!out||!size||!card_root(root))return 0;
 if(!std::strncmp(path,internal,sizeof(internal)-1))return std::snprintf(out,size,"%s/Samples/Export/%s",root,path+sizeof(internal)-1)<(int)size;
 return std::snprintf(out,size,"%s",path)<(int)size;
}
extern "C" int startup_project_loading(void){return phase.load()==1||phase.load()==2;}
extern "C" void startup_project_ready(void){
 unsigned waiting=1;
 if(phase.compare_exchange_strong(waiting,3))std::fprintf(stderr,"STARTUP_PROJECT NATIVE_NEW_PROJECT_READY no_extra_load\n");
 if(phase.load()==3&&!workspace_checked.exchange(true))prepare_workspace();
}
extern "C" void startup_project_changed(void){
 /* Screen action is synchronous on the UI owner; also verify once when its
  * busy dialog has closed, in case native cleanup finishes on a worker. */
 workspace_checked.store(false);prepare_workspace();
}
extern "C" int startup_project_install(uintptr_t b){
 auto*slot=reinterpret_cast<uintptr_t*>(b+0x6d8cc20);
 auto*auto_slot=reinterpret_cast<uintptr_t*>(b+0x6d8cc10);
 auto*save_slot=reinterpret_cast<uintptr_t*>(b+0x6d8cc18);
 if(*slot!=b+0x1506b00||*auto_slot!=b+0x1506580||*save_slot!=b+0x1508bc0)return 0;
 image=b;original=reinterpret_cast<Load>(*slot);choose();
 original_autosave=reinterpret_cast<AutoSave>(*auto_slot);
 original_save=reinterpret_cast<Save>(*save_slot);
 void*page=reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(slot)&~uintptr_t(4095));
 if(mprotect(page,4096,PROT_READ|PROT_WRITE))return 0;
 *slot=reinterpret_cast<uintptr_t>(load);
 *auto_slot=reinterpret_cast<uintptr_t>(autosave);
 *save_slot=reinterpret_cast<uintptr_t>(save);
 if(mprotect(page,4096,PROT_READ))_exit(121);
 phase.store(1);std::fprintf(stderr,"STARTUP_PROJECT FIRST_LOAD_INSTALLED selected=%u\n",selected[0]!=0);return 1;
}
