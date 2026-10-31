#define _GNU_SOURCE
#include "workflow_ui.h"
#include "phone_input.h"
#include "batch_copy.h"
#include "step_native.h"
#include "note_native.h"
#include "browser_tree.h"
#include "preview_gain.h"
#include "ui_caption.h"
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include <math.h>
#include <strings.h>
#include <ftw.h>
#include <glob.h>
#include <dirent.h>
#include <sys/stat.h>
#include "pad_palette.h"
#define FN(o,t) ((t)(base+(o)))
#define FIELD(p,o,t) (*(t*)((char*)(p)+(o)))
#define FILE_VPTR 0x6e1cfb8
#define COPY_VPTR 0x6e1c3d0
#ifndef MPC_EXPERIMENTAL_CHOP_EXPORT
#define MPC_EXPERIMENTAL_CHOP_EXPORT 0
#endif
#define WHITE 0xffffffffu
#define GREY 0xff929292u
#define AMBER 0xffffc247u
#define RED 0xffff4040u
#define CYAN 0xff39c1d6u
static uintptr_t base;
static int mono,mono_job,mono_confirm=-1;
static unsigned mono_source,mono_target,mono_bank;
static void *mono_root,*mono_program;
static struct batch_copy_progress mono_progress;
static char mono_error[80];
static void *current_program(void);
static void mono_paint(void*);
static void mono_palette(void);
static void *files,*copy_root,*chop_root;
static int chop_host;
static _Atomic(uintptr_t) chop_id;
static _Atomic unsigned dead_chop,chop_available,split_forwarded;
static void*batch_surface(void){return chop_host?chop_root:copy_root;}
static void batch_start(int);
static _Atomic(uintptr_t) copy_context,copy_services;
static _Atomic(uintptr_t) ui_services;
static unsigned service_reports;
static int chop_export,slice_page,slice_count,slice_focus;
static unsigned slice_sound;
static int batch,batch_page,batch_bank,batch_auto_bank,batch_row,batch_layout,batch_confirm,batch_events;
static unsigned source[BATCH_MAX_ITEMS],destination[BATCH_MAX_ITEMS],pinned[BATCH_MAX_ITEMS],batch_count;
static unsigned selected_sources[128],occupied[128];
static void *batch_program;
static char batch_message[64];
static int batch_job,batch_job_presented,batch_job_queued,batch_leaving;
static unsigned preview_source=128;
static int preview_private;
static void preview_stop(void);
static void preview_refresh(void);
extern void phone_release_copy_gesture(void);
static struct batch_copy_progress batch_progress;
static void batch_close(void);
static void batch_exit(void);
static void batch_exit_tick(void);
static int batch_control(void*);
static int batch_button(void*);
static void batch_pad(unsigned,int);
static void batch_paint(void*);
static void batch_palette(void);
static void batch_job_tick(void);
static void batch_job_paint(void*);
static _Atomic(uintptr_t) files_id,copy_id;
static _Atomic unsigned dead_files,dead_copy,active,custom,button_routes;
static _Atomic uint64_t epoch=1;
static struct phone_input query;
static int search,results,shift,selected,scanning,scan_row,scan_count,overflow;
static int search_scope=2,tree_scan;
static int search_storage; /*0 current folder/library,1 user Internal,2 SD. */
#ifndef WORKFLOW_INTERNAL_ROOT
#define WORKFLOW_INTERNAL_ROOT "/data"
#endif
#ifndef WORKFLOW_STORAGE_META
#define WORKFLOW_STORAGE_META "/run/mpc-trial-app/storage-meta"
#endif
static int library_root_found;
static char search_origin[1024],search_root[1024];
static _Atomic int fault;
#define MAX_FILES 4096
struct file_result {uint64_t file;unsigned char engaged;unsigned char reserved[7];};
static struct file_result entries[MAX_FILES];
static unsigned matches[MAX_FILES],entry_count,match_count;
static uint64_t feedback[16];
static _Atomic uint64_t palette[16];
static _Atomic uint64_t palette_generation=1;
static unsigned result_pad;
static int result_pad_ready;
enum { FILE_SAMPLES=0,FILE_PROJECTS=1 };
static int project_confirm,selected_audio,delete_after_locate;
static int delete_confirm;static char delete_dir[1024]; /* project folder (Mod-Exit-...) to delete */
static int search_preview_on;
static uint64_t search_preview_file;
static uint64_t project_candidate;
static void *constructed_files; /* FilesScreen built at start (autoload) */
static uint64_t locate_file,locate_parent,locate_deadline;
static int locating,locate_row;
static char browser_message[80];static uint64_t browser_message_until;
static void *volume_service;
static _Atomic unsigned preview_allowed=1,preview_range,preview_started;
static _Atomic(uintptr_t) preview_sample,preview_owner;
static _Atomic int64_t preview_first,preview_end;
static unsigned volume_percent=75;
static struct {void *parameter,*control;} master_bindings[8];
static unsigned master_binding_count;
static int volume_ready;
static _Atomic unsigned volume_owned;
static void locate_tick(void);
static char query_text[PHONE_MAX_CHARS*4+1];
static char scan_query[PHONE_MAX_CHARS*4+1];
static void scan_start(void);
static void search_preview_stop(void);
static void search_preview_update(void);
static int current_selection(struct file_result*);
static int selected_is_audio(void);
static void request_delete(void);
static void locate_result(void);
extern void workflow_optional(uintptr_t,void*,int,struct file_result*);
extern void workflow_directory(uintptr_t,void*,uint64_t*);
static uint64_t now_ms(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return(uint64_t)t.tv_sec*1000+t.tv_nsec/1000000;}
static int clamp(int n,int lo,int hi){return n<lo?lo:n>hi?hi:n;}
static int visible(void*p){return p && FN(0x112fd50,int(*)(void*))(p) && !FN(0x112f6b0,int(*)(void*))(p);}
static int chop_actions_visible(void){
 /* SampleModeScreen's real Split component, constructed at167397c. Chop
  * editing is controlled by ChopController, independently of 16 Levels
  * Slice playback mode. The original Split row is the authoritative gate. */
 return visible(chop_root)&&visible((char*)chop_root+0xa10);
}
static void repaint(void*p){if(p&&!(p==files&&atomic_load(&dead_files)))FN(0x1149880,void(*)(void*,uint64_t,uint64_t))(p,0,FIELD(p,0x28,uint64_t));}
static void release_files(void){browser_tree_close();tree_scan=0;for(unsigned i=0;i<entry_count;++i)if(entries[i].engaged)FN(0xf0a290,void(*)(uint64_t*))(&entries[i].file);entry_count=match_count=0;}
static void clear_project_confirm(void){project_confirm=0;if(project_candidate){FN(0xf0a290,void(*)(uint64_t*))(&project_candidate);project_candidate=0;}}
static void clear_locate(void){locating=0;delete_after_locate=0;if(locate_file)FN(0xf0a290,void(*)(uint64_t*))(&locate_file);if(locate_parent)FN(0xf0a290,void(*)(uint64_t*))(&locate_parent);locate_file=locate_parent=0;}
static void browser_say(const char*s){snprintf(browser_message,sizeof(browser_message),"%s",s);browser_message_until=now_ms()+2500;repaint(files);}
static void close_search(void){search_preview_stop();search_preview_on=0;search=results=scanning=0;clear_project_confirm();release_files();atomic_store(&custom,0);atomic_fetch_add(&epoch,1);atomic_fetch_add(&palette_generation,1);repaint(files);}
static int sample_browser(void){return files&&FIELD(files,0x1388,int)==FILE_SAMPLES;}
void workflow_registry_bind(void*s){
 if(!s||atomic_load(&ui_services)==(uintptr_t)s)return;
 atomic_store(&ui_services,(uintptr_t)s);
#if defined(MPC_RESAMPLE_TRIAL) && !defined(MPC_STOCK_RESAMPLE)
 extern void fx_native_registry_bind(void*);
 fx_native_registry_bind(s);
#endif
 fprintf(stderr,"WORKFLOW UI_REGISTRY_BOUND independent_of_Copy\n");
}
static void*ui_service(uintptr_t type){
 void*s=(void*)atomic_load(&ui_services);if(!s)return NULL;
 void*lock=(char*)s+0x38;if(FN(0xb51828,int(*)(void*))(lock))return NULL;
 void**entry=FN(0x143ad50,void**(*)(void*,const uintptr_t*))(s,&type);void*p=entry?*entry:NULL;
 FN(0xb51808,void(*)(void*))(lock);return p;
}
void *workflow_project_services(void){
 void *s=(void*)atomic_load(&ui_services);
 return s?s:(void*)atomic_load(&copy_services);
}
void workflow_master_volume_bind(void*p,void*c){
 /* Native1ba88b0 binds the physical control to a mixer parameter. There is
  * also a full-MPC caller; select AC50's exact GlobalControls+2ea8 later. */
 if(!p||!c)return;
 for(unsigned i=0;i<master_binding_count;++i)if(master_bindings[i].control==c){master_bindings[i].parameter=p;return;}
 if(master_binding_count<8){master_bindings[master_binding_count].parameter=p;master_bindings[master_binding_count++].control=c;}
}
static float master_gain(void){
 /* AC50 has raw, mapped and focused control surfaces. The native master
  * binds the RAW surface (149d100), not FileSelect's focused surface. */
 void*s=ui_service(base+0x6e08e40);void*c=s?FN(0x149d100,void*(*)(void*))(s):NULL;if(!c)return -1.f;
 for(unsigned i=0;i<master_binding_count;++i)if(master_bindings[i].control==(char*)c+0x2ea8){
  void*p=master_bindings[i].parameter;
  /*18c8b10 returns the channel0 gain parameter; curve1 is linear. Native
   *1ba8550 writes its actual amplitude to+4 before notifying Property+8. */
  float v=FIELD(p,4,float);if(FIELD(p,0x60,unsigned)!=1||!isfinite(v)||v<0.f||v>1.001f)return -1.f;
  return fminf(v,1.f);
 }return -1.f;
}
static void volume_bind(void*s){
 /*1501380 installs onSamplePreview with the exact manager/invoker below;
  * its inline reference at90 IS the live SampleFileAudition. It is not a
  * registered service in every AC50 dependency container. */
 void*p=NULL;
 if(s&&FIELD(s,0xa0,uintptr_t)==base+0x1500620&&FIELD(s,0xa8,uintptr_t)==base+0x1500560)p=FIELD(s,0x90,void*);
 if(p&&FIELD(p,0,uintptr_t)==base+0x6da3d18)volume_service=p;
 else volume_service=NULL;
 if(service_reports<8){++service_reports;fprintf(stderr,"PREVIEW_VOLUME callback_bound=%d master_bindings=%u\n",!!volume_service,master_binding_count);}
}
static void volume_refresh(void){
 if(!volume_service&&files)volume_bind(FIELD(files,0x120,void*));
 if(!volume_service){void*browsing=ui_service(base+0x6e0d438);if(browsing)volume_bind(browsing);}
 float gain=master_gain();
 void*p=volume_service?FIELD(volume_service,0x358,void*):NULL;
 void*i=p?FN(0x286bbe0,void*(*)(void*,int))(p,0x14):NULL;
 unsigned curve=i?FIELD(i,0x908,unsigned):~0u;
 volume_ready=volume_service&&gain>=0.f&&curve<=3;
 if(!volume_service)return;
 /* K3 is independent attenuation BEFORE the native output master. Keep
  * native Main Volume processing; do not apply that gain twice. A missing
  * master binding writes silence and prevents a Preview request. */
 float value=preview_normalized_gain(volume_ready?gain:0.f,volume_percent,curve);
 FN(0x13bb930,void(*)(void*,float))((char*)volume_service+0xc0,value);
}
int workflow_preview_worker(void*payload){
 void*p=FIELD(payload,8,void*);
 if(!atomic_load_explicit(&preview_allowed,memory_order_acquire))return 0;
 /* The queued native payload is80 bytes: ref,owner,flags/padding,56-byte
  * SliceData. Modify only this owned copy, never the source SliceProperties.
  * Bit2 means OneShot, bit0 enables looping; keep the private audition
  * instrument, volume and stock note setup. */
 FIELD(payload,0x10,unsigned)=(FIELD(payload,0x10,unsigned)|4u)&~1u;
 if(atomic_load(&preview_range)&&p==(void*)atomic_load(&preview_owner)){
  if(FIELD(p,0x3e0,uintptr_t)!=atomic_load(&preview_sample))return 0;
  int64_t first=atomic_load(&preview_first),end=atomic_load(&preview_end);
  if(first<0||end<=first)return 0;
  FIELD(payload,0x18,int64_t)=first;FIELD(payload,0x20,int64_t)=end;
  FIELD(payload,0x28,int64_t)=first;FIELD(payload,0x30,int64_t)=end;
  FIELD(payload,0x38,unsigned)=0; /* native fresh SliceData loop mode Off */
 }
 /* Run on the native audio-command worker. Kill the previous private voice
  * before starting another; no current-project pad or transport is touched. */
 void*program=FIELD(p,0x358,void*);
 if(!program)return 0;
 FN(0x286d8a0,void(*)(void*,unsigned,unsigned,unsigned,float))(program,0,FIELD(program,0x12f8,unsigned),1,0.f);
 return 1;
}
void workflow_preview_played(void*payload){
 if(atomic_load(&preview_allowed)&&atomic_load(&preview_range)
    &&FIELD(payload,8,uintptr_t)==atomic_load(&preview_owner))atomic_store(&preview_started,1);
}
int workflow_preview_prepare(void*s,int mode){
 if(mode!=FILE_SAMPLES)return 1;
 volume_bind(s);volume_refresh();
 if(!volume_ready){void*p=volume_service?FIELD(volume_service,0x358,void*):NULL;void*i=p?FN(0x286bbe0,void*(*)(void*,int))(p,0x14):NULL;fprintf(stderr,"PREVIEW BLOCKED volume=%d master_ready=%d instrument=%d curve=%u\n",!!volume_service,master_gain()>=0.f,!!i,i?FIELD(i,0x908,unsigned):~0u);browser_say("Preview not ready; sound blocked");return 0;}
 atomic_store(&preview_range,0);atomic_store_explicit(&preview_allowed,1,memory_order_release);
 fprintf(stderr,"PREVIEW READY level=%u master=%.4f\n",volume_percent,(double)master_gain());return 1;
}
static void volume_set(unsigned v){
 volume_percent=((v>127?127:v)*100+63)/127;volume_refresh();
 if(!volume_ready)browser_say("Preview not ready; sound blocked");
 repaint(files);
}
static const char* filename(const struct file_result*f){const char*p=(const char*)(uintptr_t)f->file;if(!p)return "";const char*n=strrchr(p,'/');return n?n+1:p;}
static int current_selection(struct file_result*out){
 memset(out,0,sizeof(*out));if(!files)return 0;
 if(search){if(!results||!match_count)return 0;FN(0xf187a0,void(*)(void*,const void*))(out,&entries[matches[selected]]);out->engaged=1;}
 else workflow_directory(base+0x15b7090,(char*)files+0xff0,&out->file);
 return out->engaged;
}
static int audio_path(const char*p){
 const char*e=strrchr(p,'.');return e&&(!strcasecmp(e,".wav")||!strcasecmp(e,".aif")||!strcasecmp(e,".aiff")||!strcasecmp(e,".mp3")||!strcasecmp(e,".flac")||!strcasecmp(e,".ogg"));
}
static int selected_is_audio(void){
 struct file_result f={0};if(!current_selection(&f))return 0;
 int ok=audio_path((char*)(uintptr_t)f.file)&&FN(0xf41ac0,int(*)(const void*))(&f);
 FN(0xf0a290,void(*)(uint64_t*))(&f.file);return ok;
}
/* A project is Name.xpj plus Name_[ProjectData]/ beside it. The stock Delete
 * (with its own question) removes the .xpj; once that file is gone we remove
 * its _[ProjectData] folder too. Nothing is removed while the .xpj exists. */
static char pending_xpj[1024],pending_data[1100];static uint64_t pending_since;
static int remove_entry(const char*p,const struct stat*s,int type,struct FTW*f){(void)s;(void)f;return type==FTW_DP?rmdir(p):unlink(p);}
static void project_cleanup_tick(void){
 if(!pending_xpj[0])return;
 if(access(pending_xpj,F_OK)==0){if(now_ms()-pending_since>10*60*1000){pending_xpj[0]=0;pending_data[0]=0;}return;}
 struct stat s;if(pending_data[0]&&!lstat(pending_data,&s)&&S_ISDIR(s.st_mode)){
  int r=nftw(pending_data,remove_entry,16,FTW_DEPTH|FTW_PHYS);
  fprintf(stderr,"BROWSER PROJECT_DATA_REMOVED rc=%d dir=%s\n",r,pending_data);
 }
 pending_xpj[0]=0;pending_data[0]=0;
}
static void remember_project(const char*xpj){
 size_t n=strlen(xpj);if(n<5||n>=sizeof(pending_xpj))return;
 snprintf(pending_xpj,sizeof(pending_xpj),"%s",xpj);
 snprintf(pending_data,sizeof(pending_data),"%.*s_[ProjectData]",(int)(n-4),xpj);
 pending_since=now_ms();
}
/* A project folder holds only Name.xpj files and their Name_[ProjectData]
 * folders (Mod-Exit-Last, old Mod-Exit-<date>). Anything else is left alone. */
static int project_folder(const char*dir){
 if(!dir||!strstr(dir,"/MPC-Sample/Projects/"))return 0;
 DIR*d=opendir(dir);if(!d)return 0;int xpj=0,other=0;struct dirent*e;
 while((e=readdir(d))){const char*n=e->d_name;if(!strcmp(n,".")||!strcmp(n,".."))continue;
  size_t l=strlen(n);
  if(l>4&&!strcasecmp(n+l-4,".xpj"))++xpj;
  else if(l>14&&!strcmp(n+l-14,"_[ProjectData]"))continue;
  else ++other;}
 closedir(d);return xpj>0&&!other;
}
/* A loose Name_[ProjectData] folder (no project beside it, or renamed by a
 * disk check, e.g. "test1_[ProjectData]-1") holding only audio files. */
static int data_folder(const char*dir){
 if(!dir||!strstr(dir,"/MPC-Sample/Projects/"))return 0;
 const char*leaf=strrchr(dir,'/');if(!leaf||!strstr(leaf,"_[ProjectData]"))return 0;
 DIR*d=opendir(dir);if(!d)return 0;int other=0;struct dirent*e;
 while((e=readdir(d))){const char*n=e->d_name;if(!strcmp(n,".")||!strcmp(n,".."))continue;if(!audio_path(n))++other;}
 closedir(d);return !other;
}
static void browse_to(const char*dir){
 void*m=FIELD(files,0xff0+0x188,void*);void*b=m?FIELD(m,0x30,void*):NULL;if(!b)return;
 uint64_t str=0,f=0;FN(0xf535c0,void(*)(void*,const char*))(&str,dir);FN(0xf75630,void(*)(void*,const void*))(&f,&str);
 FN(0x1db1020,void(*)(void*,const void*,int))(b,&f,1);
 FN(0xf0a290,void(*)(void*))(&f);FN(0xf0a290,void(*)(void*))(&str);
}
static void close_delete_confirm(void){delete_confirm=0;delete_dir[0]=0;atomic_store(&custom,0);atomic_fetch_add(&epoch,1);repaint(files);}
static void delete_project_folder(void){
 char dir[1024];snprintf(dir,sizeof(dir),"%s",delete_dir);
 int ok=project_folder(dir)||data_folder(dir);int r=ok?nftw(dir,remove_entry,16,FTW_DEPTH|FTW_PHYS):-1;
 fprintf(stderr,"BROWSER PROJECT_FOLDER_DELETED rc=%d dir=%s\n",r,dir);
 close_delete_confirm();
 browser_say(r==0?"Project folder deleted":"Could not delete the folder");
 char*slash=strrchr(dir,'/');if(slash&&slash>dir){*slash=0;browse_to(dir);}
}
static void request_delete(void){
 struct file_result f={0};if(!current_selection(&f))return;
 const char*p=(char*)(uintptr_t)f.file;
 /* Samples: audio files in User. Projects: a user's .xpj (never factory
  * content). Both go to the same stock Delete entry with its own question. */
 int project=!sample_browser();
 {struct stat st;if(project&&p&&!search&&!stat(p,&st)&&S_ISDIR(st.st_mode)){
   if(project_folder(p)||data_folder(p)){snprintf(delete_dir,sizeof(delete_dir),"%s",p);delete_confirm=1;atomic_store(&custom,1);atomic_fetch_add(&epoch,1);}
   else browser_say("Delete: not a project folder");
   FN(0xf0a290,void(*)(uint64_t*))(&f.file);repaint(files);return;}}
 int ok=p&&(project?(strstr(p,"/Projects/")&&browser_tree_accepts(p,1)):(strstr(p,"/Samples/User/")&&audio_path(p)))&&FN(0xf41ac0,int(*)(void*))(&f);
 fprintf(stderr,"BROWSER NATIVE_DELETE_%s file=%s\n",ok?"REQUEST":"REJECT",p?p:"");
 if(!ok){FN(0xf0a290,void(*)(uint64_t*))(&f.file);browser_say(project?"Delete: choose a project file":"Delete: choose an audio file in User");return;}
 if(search){FN(0xf0a290,void(*)(uint64_t*))(&f.file);locate_result();if(locating)delete_after_locate=1;return;}
 /* Pin the same selected File property used by native sample actions.
  *16056c0 snapshots this value (+160+50) before its modal confirmation;
  * stale asynchronous list notifications cannot choose the previous file. */
 void*s=FIELD(files,0x120,void*);FN(0x15001a0,void(*)(void*,const void*))((char*)s+0x160,&f);
 /* EXACT stock B3 Delete entry16056c0 ->14fc8f0, original confirmation.
  * No custom unlink of the chosen file and no second dialog; a project's
  * _[ProjectData] folder goes only after the stock question removed its .xpj. */
 if(project)remember_project(p);
 FN(0x16056c0,void(*)(void*))(files);
 FN(0xf0a290,void(*)(uint64_t*))(&f.file);repaint(files);
}
static unsigned fold(unsigned c){return c>='A'&&c<='Z'?c+32:c;}
static int contains(const char*s,const char*q){if(!*q)return 1;for(;*s;++s){size_t i=0;while(q[i]&&s[i]&&fold((unsigned char)q[i])==fold((unsigned char)s[i]))++i;if(!q[i])return 1;}return 0;}
static int storage_search_root(char*out,size_t capacity){
 if(search_storage==1){snprintf(out,capacity,"%s",WORKFLOW_INTERNAL_ROOT);return 1;}
 if(search_storage==2){
  char meta[1024],saved[1024];snprintf(meta,sizeof(meta),"%s/save-path",WORKFLOW_STORAGE_META);
  FILE*f=fopen(meta,"r");if(!f)return 0;
  char*line=fgets(saved,sizeof(saved),f);fclose(f);if(!line)return 0;
  saved[strcspn(saved,"\r\n")]=0;size_t n=strlen(saved);
  if(saved[0]!='/'||n<11||strcmp(saved+n-11,"/MPC-Sample"))return 0;
  int written=snprintf(out,capacity,"%s/%s",saved,sample_browser()?"Samples":"Projects");
  return written>0&&(size_t)written<capacity;
 }
 snprintf(out,capacity,"%s",search_origin);return 1;
}
static void filter(void){phone_utf8(&query,query_text,sizeof(query_text));if(search&&strcmp(scan_query,query_text)&&(scanning||overflow||!contains(query_text,scan_query)))scan_start();match_count=0;for(unsigned i=0;i<entry_count;++i)if(contains(filename(&entries[i]),query_text))matches[match_count++]=i;selected=clamp(selected,0,match_count?(int)match_count-1:0);}
static void scan_start(void){
 release_files();scanning=0;fault=0;scan_row=scan_count=overflow=library_root_found=0;void*m=FIELD(files,0xff0+0x188,void*);
 phone_utf8(&query,scan_query,sizeof(scan_query));
 if(!m){fault=1;return;}void*b=FIELD(m,0x30,void*);if(!b){fault=1;return;}
 if(!storage_search_root(search_root,sizeof(search_root))){fault=1;browser_say("Selected storage unavailable");return;}
 if(search_scope||search_storage){
  if(search_storage)library_root_found=1;
  else if(search_scope==2){const char*section=sample_browser()?"/Samples":"/Projects";char*last=NULL;for(char*p=search_root;(p=strstr(p,section));++p)if(p[strlen(section)]=='/'||!p[strlen(section)])last=p;if(last){last[strlen(section)]=0;library_root_found=1;}}
  if(browser_tree_start(search_root,!sample_browser())){scanning=tree_scan=1;fprintf(stderr,"SEARCH BEGIN type=%s scope=%d root=%s\n",sample_browser()?"samples":"projects",search_scope,search_root);return;}
  fprintf(stderr,"SEARCH ROOT_FAILED root=%s errno=%d\n",search_root,browser_tree_error());fault=1;return;
 }
 int n=FN(0x11018d0,int(*)(void*))((char*)b+0x458);
 uintptr_t begin=FIELD(m,0x40,uintptr_t),end=FIELD(m,0x48,uintptr_t);
 if(n<0||n>100000||end<begin||(end-begin)%8||end-begin>100000*8){fault=1;return;}
 scan_count=n+(int)((end-begin)/8);
 scanning=1;
}
static void start_search(void){search_preview_stop();search_preview_on=0;search=1;results=0;selected=fault=0;result_pad_ready=0;void*t=note_native_context();void*p=t?FIELD(t,0xb8,void*):NULL;void*i=t?FIELD(t,0x60,void*):NULL;if(p&&i){int n=FN(0x286bc20,int(*)(void*,void*))(p,i);if(n>=0&&n<128){result_pad=(unsigned)n;result_pad_ready=1;}}void*m=FIELD(files,0xff0+0x188,void*);void*b=m?FIELD(m,0x30,void*):NULL;uint64_t directory=0;
 /* Model+38 is its last asynchronous listing cache, not necessarily the
  * displayed folder. Use BrowserLogic::GetCurrentDirectory, as native row
  * lookup14f88f0 does, with the nontrivial File return in x8. */
 if(b)workflow_directory(base+0x1db08c0,b,&directory);
 const char*folder=(const char*)(uintptr_t)directory;snprintf(search_origin,sizeof(search_origin),"%s",folder?folder:"");if(directory)FN(0xf0a290,void(*)(uint64_t*))(&directory);
 /* Factory /content/Projects contains kit-style project presets. Search the
  * verified SD user Projects by default when starting a query there. */
 if(!sample_browser()&&!strncmp(search_origin,"/content",8)&&!search_storage)search_storage=2;
 phone_init(&query,64);phone_enable(&query,1);memset(feedback,0,sizeof(feedback));scan_start();atomic_store(&custom,1);atomic_fetch_add(&epoch,1);atomic_fetch_add(&palette_generation,1);repaint(files);}
static void scan_tick(void){
 if(tree_scan){
  for(unsigned budget=16;scanning&&budget--;){char path[1024];int r=browser_tree_next(path,sizeof(path),16);if(r<0){overflow|=browser_tree_limited();scanning=0;fprintf(stderr,"SEARCH END files=%u limited=%d errno=%d root=%s\n",entry_count,overflow,browser_tree_error(),search_root);break;}if(!r)break;
   const char*name=strrchr(path,'/');if(!contains(name?name+1:path,scan_query))continue;
   if(entry_count==MAX_FILES){overflow=1;scanning=0;browser_tree_close();break;}
   uint64_t str=0;struct file_result f={0};FN(0xf535c0,void(*)(void*,const char*))(&str,path);FN(0xf75630,void(*)(void*,const void*))(&f.file,&str);FN(0xf0a290,void(*)(void*))(&str);f.engaged=1;entries[entry_count++]=f;
  }filter();return;
 }
 void*m=FIELD(files,0xff0+0x188,void*);
 for(unsigned budget=24;scanning&&budget--&&scan_row<scan_count;){
  struct file_result f={0};workflow_optional(base+0x14f88f0,m,scan_row++,&f);
  if(!f.engaged)continue;
  if(!FN(0xf41ac0,int(*)(void*))(&f)){FN(0xf0a290,void(*)(uint64_t*))(&f.file);continue;}
  if(!browser_tree_accepts(filename(&f),!sample_browser())){FN(0xf0a290,void(*)(uint64_t*))(&f.file);continue;}
  if(!contains(filename(&f),scan_query)){FN(0xf0a290,void(*)(uint64_t*))(&f.file);continue;}
  if(entry_count==MAX_FILES){overflow=1;scanning=0;FN(0xf0a290,void(*)(uint64_t*))(&f.file);break;}
  entries[entry_count++]=f;
 }
 if(scan_row>=scan_count)scanning=0;
 filter();
}
static void publish_palette(void){
 if(batch){batch_palette();return;}
 if(atomic_load(&dead_files))return;
 struct phone_input p=query;uint64_t now=now_ms();for(unsigned i=0;i<16;++i)if(feedback[i]>now)p.held|=1u<<i;
 for(unsigned i=0;i<16;++i){uint64_t light=search&&!results&&query.enabled?phone_light(&p,i+1):0;
  if(search&&results&&sample_browser()){unsigned bank=(unsigned)step_native_browser_bank(-1);if(bank<8){void*t=note_native_context();void*program=t?FIELD(t,0xb8,void*):NULL;void*instrument=program?FN(0x286bbe0,void*(*)(void*,int))(program,(int)(bank*16+i)):NULL;/* Shared pad palette: target white (red, blinking, when it would replace a sound), occupied dim white, empty off. */int used=instrument&&FN(0x2804760,int(*)(void*))(instrument),target=result_pad_ready&&result_pad==bank*16+i;uint32_t colour=target?(used?PAD_DANGER:PAD_SELECT):used?PAD_OCCUPIED:0;float power=target?(used&&!pad_blink_on(now_ms())?.25f:PAD_BRIGHT_SELECT):PAD_BRIGHT_OCCUPIED;light=pad_light(colour,power);}}
  if(atomic_exchange(&palette[i],light)!=light)atomic_fetch_add(&palette_generation,1);
 }
}
static void choose_result(void){
 if(!match_count||fault)return;
 if(!FN(0xf41ac0,int(*)(void*))(&entries[matches[selected]])){fault=1;return;}
 struct file_result chosen={0};FN(0xf187a0,void(*)(void*,const void*))(&chosen,&entries[matches[selected]]);chosen.engaged=1;
 void*service=FIELD(files,0x120,void*);int context=FIELD(files,0x1388,int);
 /* Exactly the stock primary action. Its callback retains its own File if
  * it starts an asynchronous load. No custom deserializer or path rewrite. */
 if(context==FILE_SAMPLES){void*t=note_native_context();if(!t||!result_pad_ready){FN(0xf0a290,void(*)(uint64_t*))(&chosen.file);browser_say("Choose a destination pad");return;}FN(0x28f9d60,void(*)(void*,int))(t,(int)result_pad);FN(0x14fc5a0,void(*)(void*,const void*))(service,&chosen);browser_say("Sample load requested; keep browsing");}
 else if(context==FILE_PROJECTS){clear_project_confirm();FN(0xf187a0,void(*)(void*,const void*))(&project_candidate,&chosen);project_confirm=1;}
 FN(0xf0a290,void(*)(uint64_t*))(&chosen.file);
}
static void search_preview_stop(void){
 atomic_store_explicit(&preview_allowed,0,memory_order_release);atomic_store(&preview_range,0);atomic_store(&preview_started,0);
 if(volume_service)FN(0x20beec0,void(*)(void*))(volume_service);
 if(search_preview_file){FN(0xf0a290,void(*)(uint64_t*))(&search_preview_file);search_preview_file=0;}
}
static void search_preview_update(void){
 if(!search_preview_on||!search||!results||!match_count||fault||!sample_browser()){
  if(search_preview_file)search_preview_stop();
  return;
 }
 const struct file_result*f=&entries[matches[selected]];
 if(search_preview_file&&FN(0xf5ee90,int(*)(const void*,const void*))(&search_preview_file,f))return;
 search_preview_stop();
 FN(0xf187a0,void(*)(void*,const void*))(&search_preview_file,f);
 if(!FN(0xf41ac0,int(*)(const void*))(f))return;
 void*s=FIELD(files,0x120,void*);int context=FILE_SAMPLES;
 if(!workflow_preview_prepare(s,context))return;
 FN(0x14fb1c0,void(*)(void*,const int*,const void*))(s,&context,f);
 fprintf(stderr,"SEARCH AUTO_PREVIEW file=%s\n",(char*)(uintptr_t)f->file);
}
static void preview_result(void){
 if(!sample_browser())return;
 search_preview_on=!search_preview_on;
 if(!search_preview_on)search_preview_stop();else search_preview_update();
 fprintf(stderr,"SEARCH PREVIEW_MODE on=%d\n",search_preview_on);
}
static void confirm_project(void){
 if(!project_confirm||!project_candidate)return;
 uint64_t chosen=0;FN(0xf187a0,void(*)(void*,const void*))(&chosen,&project_candidate);
 void*s=FIELD(files,0x120,void*);close_search();
 FN(0x14fc6a0,void(*)(void*,const void*,int))(s,&chosen,1);
 FN(0xf0a290,void(*)(uint64_t*))(&chosen);
}
static void locate_result(void){
 if(!match_count||fault)return;
 clear_locate();
 const struct file_result*f=&entries[matches[selected]];const char*path=(char*)(uintptr_t)f->file;
 const char*slash=strrchr(path,'/');if(!slash)return;size_t n=(size_t)(slash-path);if(n>=1024)return;
 char parent[1024];memcpy(parent,path,n);parent[n]=0;if(!n)strcpy(parent,"/");
 uint64_t str=0;FN(0xf535c0,void(*)(void*,const char*))(&str,parent);FN(0xf75630,void(*)(void*,const void*))(&locate_parent,&str);FN(0xf0a290,void(*)(void*))(&str);
 FN(0xf187a0,void(*)(void*,const void*))(&locate_file,f);
 void*m=FIELD(files,0xff0+0x188,void*);void*b=m?FIELD(m,0x30,void*):NULL;
 if(!b){clear_locate();browser_say("Browser not ready");return;}
 if(sample_browser()&&result_pad_ready){void*t=note_native_context();if(t)FN(0x28f9d60,void(*)(void*,int))(t,(int)result_pad);}
 close_search();locating=1;locate_row=0;locate_deadline=now_ms()+5000;
 /*14fc7e0 only changes selected-file properties. Set the actual browser
  * directory, then select the result when its native listing is ready. */
 FN(0x1db1020,void(*)(void*,const void*,int))(b,&locate_parent,1);
 browser_say("Opening file location...");
}
static void locate_tick(void){
 if(!locating)return;
 if(!visible(files)){clear_locate();return;}
 if(now_ms()>locate_deadline){clear_locate();browser_say("Folder opened; file not yet listed");return;}
 void*m=FIELD(files,0xff0+0x188,void*);if(!m)return;
 uint64_t folder=FIELD(m,0x38,uint64_t);if(!folder||!FN(0xf5ee90,int(*)(const void*,const void*))(&folder,&locate_parent))return;
 void*b=FIELD(m,0x30,void*);if(!b)return;uintptr_t begin=FIELD(m,0x40,uintptr_t),end=FIELD(m,0x48,uintptr_t);
 int count=FN(0x11018d0,int(*)(void*))((char*)b+0x458);if(count<0||count>100000||end<begin||(end-begin)%8||end-begin>100000*8)return;count+=(int)((end-begin)/8);
 for(unsigned budget=32;budget--&&locate_row<count;){int row=locate_row++;struct file_result f={0};workflow_optional(base+0x14f88f0,m,row,&f);if(!f.engaged)continue;
  int equal=FN(0xf5ee90,int(*)(const void*,const void*))(&f,&locate_file);FN(0xf0a290,void(*)(void*))(&f.file);
  if(equal){FN(0x3b71f20,void(*)(void*,int))((char*)files+0xff0,row);fprintf(stderr,"SEARCH LOCATED row=%d file=%s\n",row,(char*)(uintptr_t)locate_file);int remove=delete_after_locate;delete_after_locate=0;clear_locate();browser_message[0]=0;repaint(files);if(remove)request_delete();return;}
 }
 if(locate_row>=count)locate_row=0; /* Native listing arrives asynchronously. */
}
void workflow_init(uintptr_t b){base=b;}
int workflow_install(void(*fp)(void*,void*),void(*cp)(void*,void*)){
 uintptr_t*slots[2]={(uintptr_t*)(base+FILE_VPTR+0xd8),(uintptr_t*)(base+COPY_VPTR+0xd8)};
 void(*callbacks[2])(void*,void*)={fp,cp};
 for(unsigned i=0;i<2;++i)if(*slots[i]!=base+0x10e2e10)return 0;
 for(unsigned i=0;i<2;++i){void*page=(void*)((uintptr_t)slots[i]&~(uintptr_t)4095);if(mprotect(page,4096,PROT_READ|PROT_WRITE))return 0;*slots[i]=(uintptr_t)callbacks[i];if(mprotect(page,4096,PROT_READ))_exit(121);}return 1;
}
void workflow_destroyed(void*p){if(p==constructed_files)constructed_files=NULL;if((uintptr_t)p==atomic_load(&chop_id)){atomic_store(&dead_chop,1);atomic_store(&chop_available,0);}if((uintptr_t)p==atomic_load(&files_id)){atomic_store(&dead_files,1);atomic_store(&active,0);atomic_store(&custom,0);atomic_fetch_add(&epoch,1);}if((uintptr_t)p==atomic_load(&copy_id)){atomic_store(&dead_copy,1);atomic_store(&active,0);atomic_store(&custom,0);atomic_fetch_add(&epoch,1);}}
int workflow_active(void){return atomic_load(&active);}
int workflow_blocks_sample_controls(void){return atomic_load(&custom)||atomic_load(&button_routes)||atomic_load(&split_forwarded)||batch_job||batch_leaving;}
int workflow_jobs_busy(void){return batch_job||mono_job;}
uint64_t workflow_ticket(void){return atomic_load(&epoch);}
void workflow_fault(void){atomic_fetch_add(&epoch,1);fault=1;}
/* Open the last project when the mod starts: the newest .xpj on the card,
 * which is Mod-Exit-Last unless a project saved later exists (work done in
 * the stock firmware after leaving the mod). Uses the browser's own open
 * callback (BrowserLogic+0x30, invoker 0x1500840) of the FilesScreen built
 * at start, the same path as opening a project from search. */
static int autoload_done;static uint64_t autoload_after;
void workflow_files_constructed(void*p){constructed_files=p;}
static int newest_project(char*out,size_t size){
 const char*patterns[]={"/media/*/MPC-Sample/Projects/*.xpj","/media/*/MPC-Sample/Projects/*/*.xpj"};
 time_t best=0;out[0]=0;
 for(unsigned k=0;k<2;++k){glob_t g;if(glob(patterns[k],0,NULL,&g))continue;
  for(size_t i=0;i<g.gl_pathc;++i){const char*f=g.gl_pathv[i];struct stat s;
   if(strstr(f,"/Mod-Exit-saving-")||strstr(f,"/Mod-Exit-Last-old/"))continue;
   if(!stat(f,&s)&&S_ISREG(s.st_mode)&&s.st_size>0&&s.st_mtime>=best&&strlen(f)<size){best=s.st_mtime;snprintf(out,size,"%s",f);}}
  globfree(&g);}
 return out[0]!=0;
}
static void autoload_tick(void){
 if(autoload_done)return;
 if(!note_native_sample_visible()){autoload_after=0;return;}
 if(!autoload_after){autoload_after=now_ms()+2500;return;}
 if(now_ms()<autoload_after)return;
 autoload_done=1;
 void*f=constructed_files;void*s=f?FIELD(f,0x120,void*):NULL;
 if(!s||!FIELD(s,0x40,uintptr_t)||FIELD(s,0x48,uintptr_t)!=base+0x1500840){fprintf(stderr,"AUTOLOAD SKIPPED no_open_callback\n");return;}
 char path[1024];if(!newest_project(path,sizeof(path))){fprintf(stderr,"AUTOLOAD SKIPPED no_project\n");return;}
 uint64_t string=0,file=0;
 FN(0xf535c0,void(*)(uint64_t*,const char*))(&string,path);
 FN(0xf75630,void(*)(uint64_t*,const uint64_t*))(&file,&string);
 fprintf(stderr,"AUTOLOAD OPEN %s\n",path);
 FN(0x14fc6a0,void(*)(void*,const void*,int))(s,&file,1);
 FN(0xf0a290,void(*)(uint64_t*))(&file);FN(0xf0a290,void(*)(uint64_t*))(&string);
}
int workflow_tick(void){
 project_cleanup_tick();
 autoload_tick();
 if(mono){
  if(mono_job)batch_copy_poll(&mono_progress);
  if(atomic_load(&dead_chop)||current_program()!=mono_program||atomic_load(&fault)){
   if(mono_job&&mono_progress.phase<BATCH_JOB_DONE)return 1;
   if(mono_job)batch_copy_release();
   mono=mono_job=0;atomic_store(&custom,0);atomic_fetch_add(&epoch,1);
  }
  if(!mono_job&&(!visible(mono_root)||current_program()!=mono_program)){mono=0;atomic_fetch_add(&epoch,1);}
  if(mono){mono_palette();repaint(mono_root);atomic_store(&active,1);atomic_store(&custom,1);return 1;}
 }
 if(atomic_exchange(&dead_chop,0)){chop_root=NULL;if(chop_host)batch_close();}
 atomic_store(&chop_available,MPC_EXPERIMENTAL_CHOP_EXPORT&&chop_actions_visible());
 if(atomic_load(&chop_available)&&!batch){void*c=FIELD(chop_root,0x108,void*);if(c)shift=FIELD(c,0x22a0,unsigned char)!=0;}
 if(atomic_exchange(&dead_files,0)){clear_locate();files=NULL;close_search();}
 if(atomic_exchange(&dead_copy,0)){copy_root=NULL;batch_close();}
 if(batch_job)batch_job_tick();
 if(batch_leaving)batch_exit_tick();
 int v=visible(files);if(search&&!v)close_search();
 if(delete_confirm&&!v){delete_confirm=0;delete_dir[0]=0;atomic_store(&custom,0);}
 selected_audio=v&&(search&&results?(sample_browser()&&match_count>0):selected_is_audio());
 if(volume_service||(v&&sample_browser()))volume_refresh();
 if(locating)locate_tick();
 atomic_store(&volume_owned,(unsigned)(v&&selected_audio&&!shift));
 if(fault && atomic_load(&custom)&&!batch_job&&!batch_leaving){if(batch)batch_close();}
 if(batch&&!visible(batch_surface())&&!batch_job&&!batch_leaving)batch_close();
 if(batch){preview_refresh();batch_palette();repaint(batch_surface());}
 int copy_visible=visible(copy_root);
 atomic_store(&button_routes,(copy_visible?1u:0u)|(v?2u:0u));
 atomic_store(&active,v||copy_visible||(MPC_EXPERIMENTAL_CHOP_EXPORT&&visible(chop_root))||atomic_load(&split_forwarded)||batch_job||batch_leaving);
 if(visible(batch_surface())){void*c=FIELD(batch_surface(),chop_host?0x108:0x100,void*);if(c)shift=FIELD(c,0x22a0,unsigned char)!=0;}
 if(v){int next_shift=FIELD(FIELD(files,0x100,void*),0x22a0,unsigned char)!=0;if(next_shift!=shift){shift=next_shift;repaint(files);}
  if(search){if(scanning)scan_tick();phone_tick(&query,now_ms());search_preview_update();publish_palette();repaint(files);}}
 return files||copy_root||chop_root||batch_job||batch_leaving;
}
int workflow_owns(unsigned kind,unsigned code){
 if(kind==2&&(code==0x51||code==0x52))return 0; /* Global Stop / Play. */
 if(kind==1&&code==0x13)return 0;
 if(kind==2&&code==0x31)return 0;
 if(kind==2&&code==0x0e&&atomic_load(&split_forwarded))return 1;
 if(!atomic_load(&active))return 0;
 if(atomic_load(&custom))return 1;
 if(kind==1&&code==0x12&&atomic_load(&volume_owned))return 1;
 if(kind!=2)return 0;
 /* The Sample root remains visible in Tune/Play/Lights too. Only claim
  * B2/B3 for the exact Chop+Shift action or the visible Copy modal. */
 if(atomic_load(&chop_available)&&(code==0x0d||code==0x0e))return 1;
 /* This predicate also runs on native input threads. Publish UI visibility
  * there; never inspect Components, modal guards or browser objects here.
  * Claim both edges even if Shift is still waiting in the UI FIFO. */
 unsigned routes=atomic_load(&button_routes);
 if((routes&1u)&&code==0x0d)return 1;
 return (routes&2u)&&(code==0x0e||code==0x0d||code==0x09);
}
static void change_character(int delta){
 static const char alphabet[]=" abcdefghijklmnopqrstuvwxyz0123456789_.-#ABCDEFGHIJKLMNOPQRSTUVWXYZ";
 phone_native_boundary(&query);size_t pos=query.cursor;if(pos>=query.limit)return;
 char c=pos<query.length?(char)query.text[pos]:' ';const char*found=strchr(alphabet,c);int i=found?(int)(found-alphabet):0;
 i=(i+delta)%(int)(sizeof(alphabet)-1);if(i<0)i+=(int)sizeof(alphabet)-1;
 if(pos==query.length)++query.length;
 query.text[pos]=(unsigned char)alphabet[i];
}
int workflow_control(void*e,uint64_t ticket){
 if(FIELD(e,8,unsigned char)==0x13)return 0;
 if(ticket!=workflow_ticket())return 1;
 if(mono){if(!mono_job&&FIELD(e,8,unsigned char)==0x10){mono_bank=FIELD(e,9,unsigned char)*8/128;step_native_browser_bank((int)mono_bank);mono_confirm=-1;mono_palette();repaint(mono_root);}return 1;}
 if(batch)return batch_control(e);
 if(!visible(files))return 0;
 unsigned cc=FIELD(e,8,unsigned char),v=FIELD(e,9,unsigned char);int d=v>64?(int)v-128:(int)v;
 if(project_confirm||delete_confirm)return 1;
 if(cc==0x12&&selected_audio&&(!search||results)&&(!shift||(search&&results))){volume_set(v);return 1;}
 if(!search)return 0;
 if(results){if(cc==0x64||cc==0x11)selected=clamp(cc==0x64?selected+d:(int)(v*match_count/128),0,match_count?(int)match_count-1:0);}
 else if(cc==0x10)phone_enable(&query,v>=64);
 else if(cc==0x64){if(query.enabled)phone_move(&query,v>64?(int)v-128:(int)v);else change_character(d);}
 else if(cc==0x11){phone_native_boundary(&query);query.cursor=clamp(v*65/128,0,(int)query.length);}
 else if(cc==0x12&&!results){int storage=sample_browser()?(int)(v*3/128):v<64?1:2;if(storage!=search_storage){search_storage=storage;selected=0;scan_start();}}
 filter();search_preview_update();publish_palette();repaint(files);return 1;
}
int workflow_button(void*e,uint64_t ticket){
 if(FIELD(e,8,unsigned char)==0x51||FIELD(e,8,unsigned char)==0x52){if(FIELD(e,8,unsigned char)==0x51&&FIELD(e,9,unsigned char))preview_stop();return 0;}
 /* A physical Shift transition belongs to GlobalControls, not a particular
  * window generation. In particular, Shift-up queued before Batch opened
  * must not be dropped when batch_start/exit increments epoch. */
 if(FIELD(e,8,unsigned char)==0x31){shift=FIELD(e,9,unsigned char)!=0;query.shift=shift;if(files&&!atomic_load(&dead_files))repaint(files);return 0;}
 /* Pair the remapped Split release even after native Split changes view. */
 if(FIELD(e,8,unsigned char)==0x0e&&!FIELD(e,9,unsigned char)&&atomic_exchange(&split_forwarded,0)){FIELD(e,8,unsigned char)=0x0d;return 0;}
 if(ticket!=workflow_ticket())return 1;
 if(mono){
  unsigned code=FIELD(e,8,unsigned char),down=FIELD(e,9,unsigned char);
  if(!down)return 1;
  if(mono_job){if(mono_progress.phase>=BATCH_JOB_DONE&&code==0x0c){batch_copy_release();mono=mono_job=0;atomic_store(&custom,0);atomic_fetch_add(&epoch,1);repaint(mono_root);}return 1;}
  if(code==0x0c){mono=0;atomic_store(&custom,0);atomic_fetch_add(&epoch,1);repaint(mono_root);return 1;}
  if(code==0x23||code==0x36||code==0x37){mono_bank=(mono_bank+(code==0x37||(code==0x23&&shift)?7:1))%8;step_native_browser_bank((int)mono_bank);mono_confirm=-1;}
  if(code==0x0d||code==0x0e){
   unsigned channel=code==0x0e;
   void*t=FN(0x286bbe0,void*(*)(void*,int))(mono_program,(int)mono_target);
   int occupied_now=t&&FN(0x2804760,int(*)(void*))(t);
   if(occupied_now&&mono_confirm!=(int)channel){mono_confirm=(int)channel;snprintf(mono_error,sizeof(mono_error),"Replace %c%02u? Press %s again",'A'+mono_target/16,mono_target%16+1,channel?"Right":"Left");}
   else if(current_program()!=mono_program||!batch_mono_begin(base,(void*)atomic_load(&copy_context),(void*)atomic_load(&copy_services),mono_source,mono_target,channel))snprintf(mono_error,sizeof(mono_error),"Mono operation unavailable; Cancel and retry");
   else{mono_job=1;mono_progress=(struct batch_copy_progress){BATCH_JOB_PREPARE,0,1};mono_error[0]=0;}
  }
  mono_palette();repaint(mono_root);return 1;
 }
 if(MPC_EXPERIMENTAL_CHOP_EXPORT&&!batch&&atomic_load(&chop_available)&&shift){
  unsigned code=FIELD(e,8,unsigned char),down=FIELD(e,9,unsigned char);
  if(code==0x0d){if(down){chop_host=1;batch_start(1);if(!batch)chop_host=0;}return 1;}
  if(code==0x0e){if(down)atomic_store(&split_forwarded,1);FIELD(e,8,unsigned char)=0x0d;return 0;}
 }
 if(batch_leaving||batch_job||(batch||(visible(copy_root)&&FIELD(e,8,unsigned char)==0x0d)))return batch_button(e);
 if(!visible(files))return 0;
 unsigned code=FIELD(e,8,unsigned char),down=FIELD(e,9,unsigned char);if(code==0x31){shift=!!down;repaint(files);return 0;}
 if(delete_confirm){if(down){if(code==0x0c)close_delete_confirm();else if(code==0x0e)delete_project_folder();}return 1;}
 if(code==0x09&&(!search||results)){if(down)request_delete();return 1;}
 if(!search&&shift&&code==0x0d)return 1; /* Eject is reserved for stock after Mod Off. */
 if(!search){if(code==0x0e&&shift){if(down)start_search();return 1;}
  /* Stock toggles auto-preview but may leave the current one-shot voice
   * ringing. Stop that voice as well; if toggled On, native selection calls
   * workflow_preview_prepare again and explicitly re-arms playback. */
  if(code==0x0e&&down)search_preview_stop();
  return 0;}
 if(!down)return 1;
 if(project_confirm){if(code==0x0c){clear_project_confirm();repaint(files);}else if(code==0x0e)confirm_project();return 1;}
 if(code==0x0c)close_search();else if(code==0x0d){phone_commit(&query);results=!results;}
 else if(code==0x6f&&results){if(shift)locate_result();else choose_result();}
 else if(code==0x0e||code==0x6f){if(results){if(sample_browser())preview_result();else choose_result();}else{phone_commit(&query);results=1;}}
 else if(code==0x23&&results){int bank=step_native_browser_bank(-1);if(bank>=0&&bank<8)step_native_browser_bank((bank+(shift?7:1))%8);}
 else if(code==0x36||code==0x37){if(results)selected=clamp(selected+(code==0x37?-1:1),0,match_count?(int)match_count-1:0);else change_character(code==0x37?-1:1);}
 else if(code==0x09){phone_pad(&query,16,1,now_ms());phone_pad(&query,16,0,now_ms());}
 filter();search_preview_update();publish_palette();repaint(files);return 1;
}
void workflow_pad(unsigned pad,int down,uint64_t when,uint64_t ticket){if(ticket==workflow_ticket()&&mono){if(down&&!mono_job&&pad<16){mono_target=mono_bank*16+pad;mono_confirm=-1;mono_error[0]=0;mono_palette();repaint(mono_root);}return;}if(ticket==workflow_ticket()&&batch){batch_pad(pad,down);return;}if(ticket!=workflow_ticket()||!visible(files)||!search||project_confirm||pad>=16)return;if(results){if(down&&sample_browser()){int bank=step_native_browser_bank(-1);if(bank>=0&&bank<8){result_pad=(unsigned)bank*16+pad;result_pad_ready=1;search_preview_update();publish_palette();repaint(files);}}return;}if(!query.enabled)return;phone_pad(&query,pad+1,down,when);if(down)feedback[pad]=now_ms()+100;filter();publish_palette();repaint(files);}
int workflow_light(unsigned pad,uint64_t*out){if(pad>=16||!atomic_load(&custom))return 0;*out=atomic_load(&palette[pad]);return 1;}
uint64_t workflow_palette_generation(void){return atomic_load(&palette_generation);}
static void colour(void*g,uint32_t c){FN(0x102d5e0,void(*)(void*,uint32_t))(g,c);}
static void rect(void*g,int x,int y,int w,int h){FN(0x102e000,void(*)(void*,int,int,int,int))(g,x,y,w,h);}
static void text_align(void*g,const char*s,int x,int y,int w,int h,float size,int alignment){uint64_t str=0,font=0;FN(0xf535c0,void(*)(uint64_t*,const char*))(&str,s);FN(0x1082810,void(*)(uint64_t*,int,float))(&font,0,size);FN(0x102dca0,void(*)(void*,const uint64_t*))(g,&font);FN(0x10817b0,void(*)(void*,const uint64_t*,int,int,int,int,int,int,float))(g,&str,x,y,w,h,alignment,1,.75f);FN(0x1070d60,void(*)(uint64_t*))(&font);FN(0xf0a290,void(*)(uint64_t*))(&str);}
static void text(void*g,const char*s,int x,int y,int w,int h,float size){text_align(g,s,x,y,w,h,size,0x24);}
static void left_text(void*g,const char*s,int x,int y,int w,int h,float size){text_align(g,s,x,y,w,h,size,0x21);}
static void right_text(void*g,const char*s,int x,int y,int w,int h,float size){text_align(g,s,x,y,w,h,size,0x22);}
static void volume_paint(void*g){
 char value[24];
 colour(g,0xff000000);rect(g,213,198,107,42);
 colour(g,volume_ready?WHITE:RED);text(g,"Volume",213,201,107,13,12);
 if(volume_ready)snprintf(value,sizeof(value),"%u %%",volume_percent);else strcpy(value,"Not ready");
 text(g,value,213,215,107,19,18);
 colour(g,GREY);rect(g,220,235,93,4);
 if(volume_ready&&volume_percent){colour(g,WHITE);rect(g,220,235,(int)(93*volume_percent/100),4);}
}
static int storage_caption(const char*path,char*name,size_t namesize,char*address,size_t addresssize){
 if(!path)return 0;
 if(!strncmp(path,"/media/",7)&&path[7]){
  const char*slash=strchr(path+7,'/');size_t n=slash?(size_t)(slash-path-7):strlen(path+7);
  if(!n||n>64)return 0;
  snprintf(name,namesize,"SD: %.*s",(int)n,path+7);
  snprintf(address,addresssize,"SD:%.*s%s",(int)n,path+7,slash?slash:"/");return 1;
 }
 if(!strcmp(path,"/data")||!strncmp(path,"/data/",6)){
  snprintf(name,namesize,"Internal RAM");snprintf(address,addresssize,"RAM:%s",path[5]?path+5:"/");return 1;
 }return 0;
}
static void component_caption(void*root,size_t offset,void*g,const char*caption,float font){
 void*c=(char*)root+offset,*node=c;int x=0,y=0;
 for(unsigned i=0;node&&node!=root&&i<16;++i){x+=FIELD(node,0x20,int);y+=FIELD(node,0x24,int);node=FIELD(node,0x18,void*);}
 int w=FIELD(c,0x28,int),h=FIELD(c,0x2c,int);
 if(node!=root||x<0||y<5||w<60||h<12||h>42||x+w>320||y+h>198)return;
 colour(g,0xff000000);rect(g,x,y,w,h);colour(g,WHITE);text(g,caption,x+3,y,w-6,h,font);
}
static void storage_paint(void*p,void*g){
 void*m=FIELD(p,0xff0+0x188,void*),*b=m?FIELD(m,0x30,void*):NULL;uint64_t directory=0;
 if(b)workflow_directory(base+0x1db08c0,b,&directory);
 if(!directory)return;
 char name[80],address[512];
 /* The B2 storage tab (white text on black like B1/B3): in the mod both pages read "Internal" because the
  * card the mod runs from is mounted like internal storage. Name the page
  * after the folder actually open: /media/... is the SD card. */
 {const char*d=(const char*)(uintptr_t)directory;const char*tab=!strncmp(d,"/media/",7)?"External":"Internal";
  colour(g,0xff000000);rect(g,108,5,104,25);colour(g,WHITE);text(g,tab,107,6,106,22,14);}
 if(storage_caption((char*)(uintptr_t)directory,name,sizeof(name),address,sizeof(address))){
  component_caption(p,0x418,g,name,13);component_caption(p,0x11d0,g,address,11);
 }
 FN(0xf0a290,void(*)(uint64_t*))(&directory);
}
static void query_view(char*out,size_t size){
 phone_view(&query,20,out,size);
}
void workflow_files_paint(void*p,void*g){
 if(atomic_exchange(&dead_files,0)){clear_locate();files=NULL;close_search();}
 if(files!=p){clear_locate();if(files)close_search();files=p;atomic_store(&files_id,(uintptr_t)p);atomic_store(&dead_files,0);atomic_fetch_add(&epoch,1);}workflow_tick();if(!visible(p))return;
 if(delete_confirm){const char*leaf=strrchr(delete_dir,'/');
  colour(g,0xff000000);rect(g,0,5,320,235);colour(g,WHITE);text(g,"Cancel",0,6,106,23,14);colour(g,RED);text(g,"Delete",214,6,106,23,14);
  colour(g,AMBER);text(g,"Delete project folder?",8,60,304,24,19);colour(g,WHITE);text(g,leaf?leaf+1:delete_dir,8,94,304,30,15);
  text(g,"The folder and its samples are removed.",8,140,304,20,13);text(g,"This cannot be undone.",8,162,304,20,13);
  text(g,"B3: delete  |  B1: cancel",8,196,304,20,12);return;}
 if(!search){storage_paint(p,g);if(shift){colour(g,0xff000000);rect(g,107,6,213,25);colour(g,WHITE);text(g,"Search",214,6,106,23,14);}else if(selected_audio)volume_paint(g);if(browser_message[0]&&now_ms()<browser_message_until){colour(g,0xff000000);rect(g,0,181,320,15);colour(g,AMBER);text(g,browser_message,0,181,320,15,10);}return;}
 if(project_confirm){colour(g,0xff000000);rect(g,0,5,320,235);colour(g,WHITE);text(g,"Cancel",0,6,106,23,14);text(g,"Open",214,6,106,23,14);colour(g,AMBER);text(g,"Open project?",8,64,304,24,19);struct file_result candidate={project_candidate,1,{0}};text(g,filename(&candidate),8,98,304,38,15);colour(g,WHITE);text(g,"Unsaved changes may be lost.",8,150,304,20,13);text(g,"B3: confirm  |  B1: cancel",8,180,304,20,12);return;}
 colour(g,0xff000000);rect(g,0,5,320,235);colour(g,WHITE);
 text(g,"Cancel",0,6,106,23,15);text(g,results?"Query":"Results",107,6,106,23,15);text(g,results?(sample_browser()?(search_preview_on?"Preview On":"Preview Off"):"Open"):"Find",214,6,106,23,14);
 char line[96];const char*scope=search_storage==1?"Internal user library":search_storage==2?"SD user library":search_scope==0?"This folder":search_scope==1||!library_root_found?"Folder + subfolders":"Whole library";
 if(!results){
  colour(g,WHITE);left_text(g,sample_browser()?"FIND SAMPLES":"FIND PROJECTS",8,38,304,22,17);
  colour(g,GREY);left_text(g,scope,8,65,304,19,14);
  query_view(line,sizeof(line));colour(g,AMBER);left_text(g,line,8,98,304,33,24);
  colour(g,0xff373737);rect(g,8,135,304,2);
  snprintf(line,sizeof(line),"%u match%s",match_count,match_count==1?"":"es");colour(g,fault?RED:WHITE);left_text(g,fault?"Cannot read folder":line,8,146,304,24,18);
  colour(g,GREY);left_text(g,scanning?"Searching...":query.enabled?"Pads: type   Encoder: cursor":"Encoder: type",8,181,304,15,12);
 }else{
  ui_caption(line,sizeof(line),*query_text?query_text:"Results",19);colour(g,AMBER);left_text(g,line,8,37,222,25,19);
  colour(g,WHITE);right_text(g,sample_browser()?"SAMPLES":"PROJECTS",232,39,80,21,13);
  colour(g,fault?RED:GREY);left_text(g,fault?"Cannot read folder":scanning?"Searching...":overflow?"Search limit reached":scope,8,65,304,16,13);
  unsigned first=(unsigned)selected/3*3;
  for(unsigned i=first;i<match_count&&i<first+3;++i){int y=85+(int)(i-first)*26;int current=i==(unsigned)selected;
   if(current){colour(g,AMBER);rect(g,8,y,2,24);}
   ui_caption(line,sizeof(line),filename(&entries[matches[i]]),26);colour(g,current?AMBER:WHITE);left_text(g,line,16,y,296,24,18);
  }
  if(!match_count){colour(g,WHITE);left_text(g,"No matches",8,98,304,27,20);}
  if(match_count&&search_scope){const char*full=(char*)(uintptr_t)entries[matches[selected]].file;size_t rootlen=strlen(search_root);const char*relative=!strncmp(full,search_root,rootlen)&&full[rootlen]=='/'?full+rootlen+1:full;
   const char*slash=strrchr(relative,'/');char parent[512];size_t n=slash?(size_t)(slash-relative):0;if(n>=sizeof(parent))n=sizeof(parent)-1;memcpy(parent,relative,n);parent[n]=0;
   ui_caption(line,sizeof(line),n?parent:"This folder",38);colour(g,GREY);left_text(g,line,8,165,304,15,12);
  }
  colour(g,GREY);left_text(g,sample_browser()?"Click: load   S+click: folder":"Click: open   S+click: folder",8,182,304,14,12);
 }
 colour(g,WHITE);
 if(!results){text(g,"Input",0,201,106,13,12);text(g,query.enabled?"Pads":"Encoder",0,216,106,20,18);}
 else if(sample_browser()){text(g,"Load to",0,201,106,13,12);if(result_pad_ready)snprintf(line,sizeof(line),"%c%02u",'A'+result_pad/16,result_pad%16+1);else strcpy(line,"Pick pad");text(g,line,0,216,106,20,18);}
 text(g,results?"Result":"Cursor",107,201,106,13,12);snprintf(line,sizeof(line),"%u / %u",results?(match_count?(unsigned)selected+1:0):(unsigned)query.cursor+1,results?match_count:(unsigned)query.length+1);text(g,line,107,216,106,20,18);
 if(results&&selected_audio&&(!shift||results))volume_paint(g);
 else if(!results){text(g,"Storage",214,201,106,13,12);text(g,search_storage==1?"Internal":search_storage==2?"SD Card":"Current",214,216,106,20,16);}

}
/* Copy workflow is deliberately separate from file loading. */
void workflow_copy_paint(void*p,void*g){
 if(atomic_exchange(&dead_copy,0)){copy_root=NULL;batch_close();}
 if(copy_root!=p){batch_close();copy_root=p;atomic_store(&copy_id,(uintptr_t)p);atomic_store(&dead_copy,0);atomic_fetch_add(&epoch,1);}
 workflow_tick();if(!visible(p))return;
 if(batch){batch_paint(g);return;}colour(g,0xff000000);rect(g,107,6,106,25);colour(g,WHITE);text(g,"Batch",107,6,106,23,14);if(batch_message[0]){colour(g,0xff000000);rect(g,4,169,312,22);colour(g,RED);text(g,batch_message,4,169,312,22,12);}
}
/* Experimental host is the ordinary Chop screen, not Copy Pad. B1 Extract
 * stays native; B2 Batch replaces Split; B3 forwards Split's native B2. */
void workflow_sample_paint(void*p,void*g){
 if(!MPC_EXPERIMENTAL_CHOP_EXPORT)return;
 if(chop_root!=p){chop_root=p;atomic_store(&chop_id,(uintptr_t)p);atomic_store(&dead_chop,0);}
 /* Sample+100 retains the exact constructor service registry (16735e8).
  * FileSelectScreen's ctor also binds its registry, without ever needing
  * to enter Copy Pad. Native ThreadQueues RTTI is the pinned GOT57a0 key. */
 if(FIELD(p,0,uintptr_t)==base+0x6e212a0)workflow_registry_bind(FIELD(p,0x100,void*));
 workflow_tick();if(!visible(p))return;
 /* Mono is hosted by Sample, not by the Copy Pad modal. */
 if(mono&&p==mono_root){mono_paint(g);return;}
 if(batch&&chop_host){batch_paint(g);return;}
 if(!atomic_load(&chop_available)||!shift)return;
 colour(g,0xff000000);rect(g,107,6,213,25);colour(g,WHITE);
 text(g,"Batch",107,6,106,23,14);text(g,"Split",214,6,106,23,14);
 if(batch_message[0]){colour(g,0xff000000);rect(g,4,169,312,22);colour(g,RED);text(g,batch_message,4,169,312,22,12);}
}
void workflow_copy_context(void*p,void*services){atomic_store(&copy_services,(uintptr_t)services);atomic_store(&copy_context,(uintptr_t)p);}
void workflow_copy_context_dead(void*p){uintptr_t expected=(uintptr_t)p;if(atomic_compare_exchange_strong(&copy_context,&expected,0))atomic_store(&copy_services,0);}
static void pad_name(unsigned n,char*out,size_t cap){snprintf(out,cap,"%c%02u",'A'+n/16,n%16+1);}
static void batch_close(void){if(batch_job||batch_leaving)return;preview_stop();batch=batch_confirm=chop_export=0;chop_host=0;batch_message[0]=0;batch_copy_hold_native_exit(0);atomic_fetch_add(&palette_generation,1);atomic_store(&custom,0);atomic_fetch_add(&epoch,1);repaint(batch_surface());}
/* Do not only hide the overlay: native CopyPadMode owns the musical pads.
 * Its mode removal belongs on the same stock worker as normal Copy/Cancel. */
static void batch_exit(void){
 if(batch_job||batch_leaving)return;
 preview_stop();if(preview_source<128)return;
 if(chop_host){void*p=chop_root;batch_close();repaint(p);return;}
 batch_copy_exit_reset();
 if(!batch_copy_exit_begin(base,(void*)atomic_load(&copy_context))){snprintf(batch_message,sizeof(batch_message),"Cannot close Copy; try again");repaint(batch_surface());return;}
 batch_leaving=1;atomic_fetch_add(&epoch,1);repaint(batch_surface());
}
static void batch_exit_tick(void){
 unsigned state=batch_copy_exit_poll();
 if(state==BATCH_EXIT_PENDING)return;
 batch_leaving=0;batch_copy_exit_reset();
 if(state==BATCH_EXIT_DONE){batch_close();void*c=copy_root?FIELD(copy_root,0x100,void*):NULL;fprintf(stderr,"BATCH UI_CAPTURE_RELEASED native_shift=%u native_erase=%u\n",c?FIELD(c,0x22a0,unsigned char):0,c?FIELD(c,0x2400,unsigned char):0);}
 else{snprintf(batch_message,sizeof(batch_message),"Cannot close Copy; try Cancel again");repaint(batch_surface());}
}
static void*current_program(void){
 void*c=(void*)atomic_load(&copy_context);if(!c)return NULL;
 uintptr_t v=FIELD(c,0,uintptr_t);
 /* 1518980 captures the base subobject at complete AC50CopyMode+48.
  * Real derived ctor15173c0 replaces its vptr at1517478 after that hook.
  * Fields stay relative to the subobject: do not subtract48 for CurrentThings. */
 if(v!=base+0x6d8cd88&&v!=base+0x6e0dfd8)return NULL;
 void*t=FIELD(c,0x78,void*);return t?FIELD(t,0xb8,void*):NULL;
}
int workflow_mono_open(void*root){
 if(mono||batch||search||!root||FIELD(root,0,uintptr_t)!=base+0x6e212a0)return 0;
 unsigned pad;if(!note_native_pad_index(&pad)||pad>=128)return 0;
 void*p=current_program();if(!p)return 0;
 mono_root=root;mono_program=p;mono_source=pad;mono_target=pad;mono_bank=pad/16;
 mono=1;mono_job=0;mono_confirm=-1;mono_error[0]=0;
 atomic_fetch_add(&epoch,1);atomic_store(&active,1);atomic_store(&custom,1);mono_palette();repaint(root);return 1;
}
static void mono_palette(void){
 if(current_program()!=mono_program)return;
 for(unsigned i=0;i<16;++i){unsigned n=mono_bank*16+i;void*p=FN(0x286bbe0,void*(*)(void*,int))(mono_program,(int)n);
  int used=p&&FN(0x2804760,int(*)(void*))(p);
  uint32_t rgb=used?PAD_OCCUPIED:0;float brightness=PAD_BRIGHT_OCCUPIED;
  if(n==mono_source){rgb=PAD_CONTENT;brightness=PAD_BRIGHT_CONTENT;}
  if(n==mono_target){int overwrite=used&&n!=mono_source;rgb=overwrite?PAD_DANGER:PAD_SELECT;brightness=overwrite&&!pad_blink_on(now_ms())?.25f:PAD_BRIGHT_SELECT;}
  atomic_store(&palette[i],pad_light(rgb,brightness));}
 atomic_fetch_add(&palette_generation,1);
}
static void mono_paint(void*g){
 char line[96];colour(g,0xff000000);rect(g,0,5,320,235);
 /* Header blocks, as in Chords: B1 Cancel/Close, B2 Left, B3 Right. */
 const char*heads[3]={mono_job?(mono_progress.phase>=BATCH_JOB_DONE?"Close":"Working..."):"Cancel",mono_job?"":"Left",mono_job?"":"Right"};
 for(int i=0;i<3;++i){int x=i*107;colour(g,0xff202020);rect(g,x+1,6,105,26);colour(g,WHITE);text(g,heads[i],x+1,7,105,24,15);}
 colour(g,WHITE);text(g,"Stereo > Mono",8,38,304,26,20);
 /* Source and target blocks; the target turns red when it will replace a sound. */
 void*tp=current_program()==mono_program?FN(0x286bbe0,void*(*)(void*,int))(mono_program,(int)mono_target):NULL;
 int overwrite=mono_target!=mono_source&&tp&&FN(0x2804760,int(*)(void*))(tp);
 const int bx[2]={8,166};const char*titles[2]={"Source","Target"};unsigned pads[2]={mono_source,mono_target};
 for(int i=0;i<2;++i){
  uint32_t edge=i==0?(PAD_CONTENT|0xff000000u):overwrite?RED:WHITE;
  colour(g,0xff1c1c1c);rect(g,bx[i],72,146,84);colour(g,edge);
  rect(g,bx[i],72,146,2);rect(g,bx[i],154,146,2);rect(g,bx[i],72,2,84);rect(g,bx[i]+144,72,2,84);
  colour(g,GREY);text(g,titles[i],bx[i],78,146,18,13);
  snprintf(line,sizeof(line),"%c%02u",'A'+pads[i]/16,pads[i]%16+1);colour(g,edge);text(g,line,bx[i],98,146,36,30);
  colour(g,GREY);text(g,i==0?"stereo":overwrite?"will be replaced":"mono copy",bx[i],134,146,16,12);
 }
 colour(g,WHITE);text(g,">",154,98,12,36,22);
 if(mono_job){colour(g,mono_progress.phase==BATCH_JOB_FAILED?RED:AMBER);text(g,mono_progress.phase==BATCH_JOB_DONE?"Mono sample ready":mono_progress.phase==BATCH_JOB_FAILED?"Conversion failed; check log":"Converting sample...",4,168,312,24,15);}
 else if(mono_error[0]){colour(g,RED);text(g,mono_error,4,168,312,24,13);}
 else{colour(g,GREY);text(g,"Pad or K1/Bank: target pad",4,168,312,24,13);}
}
int workflow_event_preview(void*program,const void*event,unsigned key,unsigned mode){
 void*c=(void*)atomic_load(&copy_context);
 if(!c||current_program()!=program)return 0;
 return batch_event_preview(base,c,program,event,key,mode);
}
void workflow_event_preview_stop(int cancel){batch_event_preview_stop(base,(void*)atomic_load(&copy_context),cancel);}
unsigned workflow_event_preview_pending(void){return batch_event_preview_pending();}
extern void step_layer_sample(void*,int,uintptr_t*,uintptr_t);
static int source_slices(void){
 void*c=(void*)atomic_load(&copy_context);void*t=chop_host?FIELD(chop_root,0x148,void*):c?FIELD(c,0x78,void*):NULL;
 void*i=t&&chop_host?FIELD(t,0x668,void*):NULL;if(!i&&t)i=FIELD(t,0x60,void*);
 if(!i||!FIELD(i,0x22e8,void*))return 0;
 int index=FN(0x286bc20,int(*)(void*,void*))(batch_program,i);if(index<0||index>=128)return 0;
 uintptr_t sample[2]={0};step_layer_sample(i,0,sample,base+0x2803b90);int n=0;
 if(sample[0]&&FIELD((void*)sample[0],0x130,void*)){
  void*d=FN(0x2941ff0,void*(*)(void*))((void*)sample[0]);uintptr_t begin=FIELD(d,0x60,uintptr_t),end=FIELD(d,0x68,uintptr_t);
  if(begin&&end>=begin&&(end-begin)%8==0&&end-begin<=128*8){n=(int)((end-begin)/8);slice_sound=(unsigned)index;}
 }
 if(sample[1])FN(0x123bca0,void(*)(void*))((void*)sample[1]);
 return n;
}
static void batch_start(int slices){
 batch_program=current_program();if(!batch_program){snprintf(batch_message,sizeof(batch_message),"Batch context unavailable");fprintf(stderr,"BATCH REJECT no_copy_program\n");repaint(batch_surface());return;}
 void*t=slices&&chop_host?FIELD(chop_root,0x148,void*):NULL;
 if(slices&&chop_host&&(!t||FIELD(t,0xb8,void*)!=batch_program)){snprintf(batch_message,sizeof(batch_message),"Project changed; reopen Chop");fprintf(stderr,"BATCH REJECT different_program\n");repaint(chop_root);return;}
 void*c=(void*)atomic_load(&copy_context),*mode=FIELD(c,0x98,void*);int mode_id=mode?FIELD(mode,0x80,int):-1;
 if(!(slices&&chop_host&&chop_actions_visible())&&mode_id!=0x20a7){snprintf(batch_message,sizeof(batch_message),"Batch is available for Sample pads");fprintf(stderr,"BATCH REJECT mode=%x chop=%d\n",mode_id,chop_host);repaint(batch_surface());return;}
 fault=0;batch=1;batch_page=batch_bank=batch_row=batch_layout=batch_confirm=batch_events=0;batch_count=0;chop_export=slices;slice_page=slice_focus=0;slice_count=slices?source_slices():0;
 if(slices&&slice_count<1){batch=chop_export=0;snprintf(batch_message,sizeof(batch_message),"Selected sound has no slices");fprintf(stderr,"BATCH REJECT no_slices mode=%x\n",mode_id);return;}
 fprintf(stderr,"BATCH OPEN kind=%s mode=%x slices=%d\n",slices?"chops":"pads",mode_id,slice_count);
 int native_bank=step_native_browser_bank(-1);if(native_bank>=0&&native_bank<8)batch_bank=native_bank;batch_auto_bank=batch_bank;batch_message[0]=0;
 memset(selected_sources,0,sizeof(selected_sources));memset(pinned,0,sizeof(pinned));
 for(unsigned i=0;i<128;++i){void*p=FN(0x286bbe0,void*(*)(void*,int))(batch_program,(int)i);occupied[i]=p&&FN(0x2804760,int(*)(void*))(p);}
 batch_copy_hold_native_exit(!chop_host);atomic_store(&custom,1);atomic_fetch_add(&epoch,1);if(!chop_host){phone_release_copy_gesture();shift=0;}batch_palette();repaint(batch_surface());
}
static void preview_stop(void){
 if(preview_source>=128)return;
 search_preview_stop();
 preview_source=128;preview_private=0;
}
static void preview_refresh(void){
 /* Native timer/worker20bc9c0 clears BoolProperty298 when the private
  * instrument has no voices. Do not mistake queued/not-started for ended. */
 if(preview_source<128&&preview_private&&atomic_load(&preview_started)&&volume_service
    &&!__atomic_load_n((unsigned char*)volume_service+0x2e8,__ATOMIC_ACQUIRE)){
  preview_source=128;preview_private=0;atomic_store(&preview_started,0);
 }
}
static void preview_start(void){
 if((!batch_page&&!chop_export)||(batch_page&&!batch_count))return;
 preview_stop();if(preview_source<128)return;
 {
  unsigned slice=chop_export?(batch_page?source[batch_row]:(unsigned)slice_focus):source[batch_row];
  unsigned sound=chop_export?slice_sound:source[batch_row];
  void*i=FN(0x286bbe0,void*(*)(void*,int))(batch_program,(int)sound);
  uintptr_t sample[2]={0};int ok=0;
  if(i&&FIELD(i,0x22e8,void*))step_layer_sample(i,0,sample,base+0x2803b90);
  if(sample[0]&&FIELD((void*)sample[0],0x130,void*)){
   void*d=FN(0x2941ff0,void*(*)(void*))((void*)sample[0]);
   uintptr_t begin=FIELD(d,0x60,uintptr_t),end=FIELD(d,0x68,uintptr_t);
   if(!chop_export||(begin&&end>=begin&&(end-begin)%8==0&&end-begin<=128*8&&slice<(end-begin)/8)){
    void*layer=FIELD(i,0x22e8,void*);
    void*range=chop_export?FIELD((void*)begin,slice*8,void*):(char*)layer+0x740;
    int64_t first=range?FIELD(range,0x78,int64_t):-1,last=range?FIELD(range,0xd0,int64_t):-1;
    if(first>=0&&last>first&&last<=FN(0x2941f80,int(*)(void*))((void*)sample[0])){
     volume_refresh();
     if(volume_ready){
      search_preview_stop();
      atomic_store(&preview_owner,(uintptr_t)volume_service);atomic_store(&preview_sample,sample[0]);
      atomic_store(&preview_first,first);atomic_store(&preview_end,last);
      atomic_store(&preview_range,1);atomic_store_explicit(&preview_allowed,1,memory_order_release);
      /* Native SampleFileAudition assigns ONLY its private instrument20 and
       * queues its own ordinary sample playback. No current-track audition,
       * source mutation, project command or new PCM/file is involved. */
      FN(0x20be470,void(*)(void*,const uintptr_t*,unsigned))(volume_service,sample,4);
      preview_source=slice;preview_private=1;ok=1;
      fprintf(stderr,"BATCH PRIVATE_PREVIEW kind=%s source=%u start=%lld end=%lld\n",chop_export?"chop":"copy",slice,(long long)first,(long long)last);
     }
    }
   }
  }
  if(sample[1])FN(0x123bca0,void(*)(void*))((void*)sample[1]);
  if(!ok)snprintf(batch_message,sizeof(batch_message),"Sample preview not ready");
  return;
 }
}
static void arrange(void){
 batch_message[0]=0;
 unsigned used[128]={0};for(unsigned i=0;i<batch_count;++i)if(pinned[i]&&destination[i]<128)used[destination[i]]=1;
 for(unsigned i=0;i<batch_count;++i)if(!pinned[i]){
  destination[i]=128;
  unsigned start=(unsigned)batch_auto_bank*16+(batch_layout==1?source[i]%16:0);
  unsigned limit=batch_layout==2?16:128;
  for(unsigned k=0;k<limit;++k){unsigned d=(start+k)%128;if(!occupied[d]&&!used[d]&&!(chop_export?d==slice_sound:selected_sources[d])){destination[i]=d;used[d]=1;break;}}
 }
 if(batch_layout==2)for(unsigned i=0;i<batch_count;++i)if(destination[i]>=128){snprintf(batch_message,sizeof(batch_message),"Not enough free pads in bank %c; change K1",'A'+batch_auto_bank);break;}
 batch_confirm=0;
}
static void table(void){batch_count=0;for(unsigned i=0;i<128;++i)if(selected_sources[i]){if(batch_count==BATCH_MAX_ITEMS){snprintf(batch_message,sizeof(batch_message),"Limit: 16 selected");return;}source[batch_count]=i;pinned[batch_count++]=0;}if(!batch_count){snprintf(batch_message,sizeof(batch_message),chop_export?"Select at least one slice":"Select at least one source pad");return;}batch_page=1;batch_row=0;arrange();}
static unsigned missing_targets(void){unsigned n=0;for(unsigned i=0;i<batch_count;++i)n+=destination[i]>=128;return n;}
static int review(unsigned*replace){
 *replace=0;if(current_program()!=batch_program||!batch_count)return 0;
 for(unsigned i=0;i<batch_count;++i){if(destination[i]>=128||destination[i]==(chop_export?slice_sound:source[i]))return 0;for(unsigned j=0;j<i;++j)if(destination[i]==destination[j])return 0;*replace+=occupied[destination[i]];}
 return 1;
}
static void target_pad(unsigned n){
 if(!batch_count||n>=128)return;
 unsigned own=chop_export?slice_sound:source[batch_row];
 if(n==own){snprintf(batch_message,sizeof(batch_message),"Cannot copy a pad to itself");return;}
 for(unsigned i=0;i<batch_count;++i)if((int)i!=batch_row&&pinned[i]&&destination[i]==n){snprintf(batch_message,sizeof(batch_message),"Target already assigned to another row");return;}
 destination[batch_row]=n;pinned[batch_row]=1;arrange();
}
static void batch_apply(void){
 preview_stop();if(preview_source<128)return;
 unsigned replace=0;if(!review(&replace)){if(missing_targets())snprintf(batch_message,sizeof(batch_message),"%u targets missing; choose free pads",missing_targets());else snprintf(batch_message,sizeof(batch_message),"Fix duplicate / same-pad target");return;}
 if(!batch_confirm){batch_confirm=1;if(!chop_export&&batch_events)snprintf(batch_message,sizeof(batch_message),"Copy %u + events in all seqs; overwrite %u?",batch_count,replace);else snprintf(batch_message,sizeof(batch_message),chop_export?"Extract %u; overwrite %u? B3 confirms":"Copy %u pads; overwrite %u? B3 confirms",batch_count,replace);return;}
 /* Paint the progress screen before scheduling any native worker task.
  * No sleep and no GUI access from the worker. Final acknowledgement stays
  * on screen even when all copies finish between two display refreshes. */
 batch_job=1;batch_job_presented=batch_job_queued=0;
 batch_progress=(struct batch_copy_progress){BATCH_JOB_PREPARE,0,batch_count};
 batch_confirm=0;atomic_fetch_add(&epoch,1);repaint(batch_surface());
}
static void batch_job_tick(void){
 if(!batch_job)return;
 if(!batch_job_queued){
  if(!batch_surface()){batch_job=0;batch_close();return;}
  if(!batch_job_presented)return;
  batch_job_queued=1;
  if(!review(&(unsigned){0})||!(chop_export?batch_chop_begin(base,(void*)atomic_load(&copy_context),(void*)atomic_load(&copy_services),slice_sound,source,destination,batch_count):batch_copy_begin_events(base,(void*)atomic_load(&copy_context),source,destination,batch_count,batch_events))){
   batch_progress.phase=BATCH_JOB_FAILED;return;
  }
 }
 if(batch_progress.phase<BATCH_JOB_DONE)batch_copy_poll(&batch_progress);
 if(!batch_surface()&&batch_progress.phase>=BATCH_JOB_DONE){batch_copy_release();batch_job=0;batch_close();}
}
static int batch_control(void*e){
 if(batch_job||batch_leaving)return 1;
 unsigned cc=FIELD(e,8,unsigned char),v=FIELD(e,9,unsigned char);int delta=v&0x40?-(int)(v&0x3f):(int)(v&0x3f);
 batch_confirm=0;batch_message[0]=0;
 if(cc==0x10||cc==0x11||cc==0x12||cc==0x64)preview_stop();
 if(!batch_page){if(chop_export&&cc==0x64){preview_stop();slice_focus=clamp(slice_focus+delta,0,slice_count-1);slice_page=slice_focus/16;}if(cc==0x10){if(chop_export){preview_stop();slice_page=clamp((int)(v*((slice_count+15)/16)/128),0,(slice_count-1)/16);slice_focus=clamp(slice_page*16,0,slice_count-1);}else{batch_bank=(int)(v*8/128);step_native_browser_bank(batch_bank);}}return 1;}
 if(cc==0x64){int row=clamp(batch_row+delta,0,(int)batch_count-1);if(row!=batch_row)preview_stop();batch_row=row;}
 else if(cc==0x10){int b=(int)(v*8/128);batch_bank=b;if(shift){unsigned old=destination[batch_row];target_pad((unsigned)b*16+(old<128?old%16:source[batch_row]%16));}else{batch_auto_bank=b;arrange();}}
 else if(cc==0x11){unsigned slot=v*17/128;if(!slot){pinned[batch_row]=0;arrange();}else target_pad((unsigned)batch_bank*16+slot-1);}
 else if(cc==0x12){if(shift&&!chop_export){batch_events=v>=64;snprintf(batch_message,sizeof(batch_message),batch_events?"Sound + events: each sequence keeps its pattern":"Sound only: sequence events unchanged");}else{batch_layout=v*3/128;arrange();}}
 step_native_browser_bank(batch_bank);
 return 1;
}
static int batch_button(void*e){
 unsigned code=FIELD(e,8,unsigned char),down=FIELD(e,9,unsigned char);
 if(code==0x51||code==0x52){if(code==0x51&&down)preview_stop();return 0;}
 /* Shift releases must reach GlobalControls even during Prepare/Done/Exit. */
 if(code==0x31){shift=!!down;return 0;}
 if(batch_leaving)return 1;
 if(code==0x6f){if(down&&!batch_job){preview_refresh();if(shift||preview_source<128)preview_stop();else preview_start();}repaint(batch_surface());return 1;}
 if(batch_job){
  if(down&&batch_progress.phase>=BATCH_JOB_DONE&&(code==0x0c||code==0x0e)){
   if(batch_copy_release()){batch_job=0;batch_exit();}
  }
  return 1;
 }
 if(!batch){if(code==0x0d){if(down)batch_start(0);return 1;}return 0;}
 if(!down)return 1;
 preview_stop();
 if(code==0x0c){batch_exit();return 1;}
 else if(code==0x0d){if(!batch_page)table();else{preview_stop();batch_page=0;batch_confirm=0;}}
 else if(code==0x0e){if(!batch_page)table();else batch_apply();}
 else if(chop_export&&!batch_page&&(code==0x36||code==0x37)){slice_page=clamp(slice_page+(code==0x37?-1:1),0,(slice_count-1)/16);}
 else if(batch_page&&(code==0x36||code==0x37)){preview_stop();batch_row=clamp(batch_row+(code==0x37?-1:1),0,(int)batch_count-1);}
 else if(code==0x23||code==0x36||code==0x37){batch_bank=(batch_bank+(code==0x37||(code==0x23&&shift)?7:1))%8;}
 else if(code==0x09&&batch_page){pinned[batch_row]=0;arrange();}
 else if(code!=0x31)return 1;
 step_native_browser_bank(batch_bank);batch_palette();repaint(batch_surface());return 1;
}
static void batch_pad(unsigned pad,int down){
 if(batch_job||batch_leaving||!down||pad>=16)return;
 preview_stop();
 if(batch_page){
  unsigned n=(unsigned)batch_bank*16+pad;batch_confirm=0;batch_message[0]=0;
  int row=-1;if(!chop_export)for(unsigned i=0;i<batch_count;++i)if(source[i]==n){row=(int)i;break;}
  if(row>=0){if(batch_row!=row)preview_stop();batch_row=row;}else target_pad(n);
  batch_palette();repaint(batch_surface());return;
 }
 unsigned n=(unsigned)(chop_export?slice_page:batch_bank)*16+pad;
 batch_message[0]=0;
 if(chop_export&&n<(unsigned)slice_count){preview_stop();slice_focus=(int)n;}
 if(chop_export?n<(unsigned)slice_count:occupied[n]){
  unsigned count=0;for(unsigned i=0;i<128;++i)count+=selected_sources[i];
  if(!selected_sources[n]&&count>=BATCH_MAX_ITEMS)snprintf(batch_message,sizeof(batch_message),"Selected 16 / 16; deselect one first");
  else selected_sources[n]^=1u;
 }
 batch_palette();repaint(batch_surface());
}
static void batch_palette(void){
 static int previous_page=-1,previous_bank=-1;if(previous_page!=batch_page||previous_bank!=batch_bank){atomic_fetch_add(&palette_generation,1);previous_page=batch_page;previous_bank=batch_bank;}
 for(unsigned i=0;i<16;++i){unsigned n=(unsigned)batch_bank*16+i;unsigned shown=chop_export&&!batch_page?(unsigned)slice_page*16+i:n;
  unsigned present=chop_export&&!batch_page?shown<(unsigned)slice_count:occupied[n];
  unsigned is_source=chop_export?(batch_page?n==slice_sound:selected_sources[shown]):selected_sources[shown];
  uint32_t c=is_source?PAD_CONTENT:present?PAD_OCCUPIED:0;float strength=is_source?PAD_BRIGHT_CONTENT:PAD_BRIGHT_OCCUPIED;
  /* Targets are white; a target that overwrites an occupied pad is red and the
   * current row's target blinks, so the warning never relies on hue alone. */
  int overwrite=batch_page&&present&&!is_source;
  if(batch_page)for(unsigned j=0;j<batch_count;++j)if(destination[j]==n){c=overwrite?PAD_DANGER:PAD_SELECT;strength=.45f;break;}
  if(batch_page&&batch_count&&destination[batch_row]==n){c=overwrite?PAD_DANGER:PAD_SELECT;strength=overwrite&&!pad_blink_on(now_ms())?.25f:PAD_BRIGHT_SELECT;}
  uint64_t light=pad_light(c,strength);if(atomic_exchange(&palette[i],light)!=light)atomic_fetch_add(&palette_generation,1);}
}
static void batch_paint(void*g){
 if(batch_leaving){colour(g,0xff000000);rect(g,0,5,320,235);colour(g,WHITE);text(g,"Closing Copy...",8,92,304,30,20);return;}
 if(batch_job){batch_job_paint(g);return;}
 colour(g,0xff000000);rect(g,0,5,320,235);colour(g,WHITE);
 text(g,"Cancel",0,6,106,23,15);text(g,batch_page?"Sources":"Targets",107,6,106,23,15);text(g,batch_confirm?"Confirm":batch_page?(missing_targets()?"No space":chop_export?"Extract":"Copy"):"Next",214,6,106,23,15);
 char line[96];colour(g,WHITE);left_text(g,chop_export?"EXTRACT CHOPS":batch_events?"COPY PADS + EVENTS":"COPY PADS",8,36,304,22,17);
 if(!batch_page){
  unsigned selected_count=0;for(unsigned i=0;i<128;++i)selected_count+=selected_sources[i];
  snprintf(line,sizeof(line),"%u / 16 selected",selected_count);colour(g,AMBER);left_text(g,line,8,70,304,33,25);
  colour(g,WHITE);if(chop_export)snprintf(line,sizeof(line),"%c%02u  |  Chop %d",'A'+slice_sound/16,slice_sound%16+1,slice_focus+1);else snprintf(line,sizeof(line),"Bank %c",'A'+batch_bank);left_text(g,line,8,112,304,24,18);
  colour(g,0xff373737);rect(g,8,143,304,1);colour(g,GREY);
  left_text(g,"Green: selected   Red: occupied",8,151,304,18,13);
  ui_caption(line,sizeof(line),batch_message[0]?batch_message:chop_export?"Click: play/stop   Enc: choose":"Unlit: free   Pads: select",38);colour(g,batch_message[0]?RED:GREY);left_text(g,line,8,177,304,18,13);
 }else{
  colour(g,GREY);left_text(g,chop_export?"CHOP":"SOURCE",8,63,80,15,12);left_text(g,"TARGET",119,63,84,15,12);left_text(g,"ASSIGN",230,63,82,15,12);
  unsigned first=(unsigned)batch_row/3*3;
  for(unsigned i=first;i<batch_count&&i<first+3;++i){char a[8],b[8];int y=84+(int)(i-first)*27,current=i==(unsigned)batch_row;
   if(chop_export)snprintf(a,sizeof(a),"C%03u",source[i]+1);else pad_name(source[i],a,sizeof(a));if(destination[i]<128)pad_name(destination[i],b,sizeof(b));else strcpy(b,"--");
   if(current){colour(g,AMBER);rect(g,3,y,2,25);}
   colour(g,current?AMBER:WHITE);left_text(g,a,8,y,76,25,20);text(g,">",88,y,23,25,18);
   int busy=destination[i]<128&&occupied[destination[i]];colour(g,busy?RED:current?AMBER:WHITE);snprintf(line,sizeof(line),"%s%s",b,busy?" !":"");left_text(g,line,119,y,98,25,20);
   colour(g,current?AMBER:GREY);left_text(g,pinned[i]?"Manual":"Auto",230,y+3,82,21,14);
  }
  unsigned missing=missing_targets(),replace=0;colour(g,GREY);
  if(missing){snprintf(line,sizeof(line),"%u targets missing",missing);colour(g,RED);}
  else if(batch_confirm){review(&replace);if(replace)snprintf(line,sizeof(line),"Overwrite %u pads? B3 confirms",replace);else snprintf(line,sizeof(line),"%s %u pads? B3 confirms",chop_export?"Extract":"Copy",batch_count);colour(g,RED);}
  else snprintf(line,sizeof(line),"Row %d/%u   View bank %c",batch_row+1,batch_count,'A'+batch_bank);
  left_text(g,line,8,168,304,16,13);
  const char*hint=batch_message[0]&&!batch_confirm?batch_message:"Click: play/stop   +/-: row";ui_caption(line,sizeof(line),hint,38);colour(g,missing?RED:GREY);left_text(g,line,8,184,304,13,12);
 }
 colour(g,WHITE);text(g,chop_export&&!batch_page?"Page":shift&&batch_page?"Row bank":batch_page?"Auto bank":"Bank",0,201,106,13,12);
 if(chop_export&&!batch_page)snprintf(line,sizeof(line),"%d / %d",slice_page+1,(slice_count+15)/16);else snprintf(line,sizeof(line),"%c",'A'+(batch_page&&!shift?batch_auto_bank:batch_bank));text(g,line,0,216,106,20,18);
 if(batch_page){text(g,"Target",107,201,106,13,12);char b[8];if(!pinned[batch_row])strcpy(b,"Auto");else if(destination[batch_row]<128)pad_name(destination[batch_row],b,sizeof(b));else strcpy(b,"--");text(g,b,107,216,106,20,18);text(g,shift&&!chop_export?"Copy":"Auto",214,201,106,13,12);text(g,shift&&!chop_export?(batch_events?"Snd+events":"Sound only"):batch_layout==0?"First free":batch_layout==1?"Same pad":"Bank free",214,216,106,20,16);}
 if(batch_page){colour(g,shift?RED:WHITE);text(g,"S",91,201,11,12,11);if(!chop_export)text(g,"S",304,201,11,12,11);}

}
static void batch_job_paint(void*g){
 unsigned phase=batch_progress.phase,done=batch_progress.done,total=batch_progress.total;
 int terminal=phase>=BATCH_JOB_DONE;char line[96];
 colour(g,0xff000000);rect(g,0,5,320,235);
 colour(g,WHITE);text(g,chop_export?"EXPORT CHOPS":batch_events?"COPY PADS + EVENTS":"COPY PADS",8,37,304,21,17);
 if(terminal)text(g,"Close",214,6,106,23,14);
 colour(g,phase==BATCH_JOB_FAILED?RED:AMBER);
 snprintf(line,sizeof(line),phase==BATCH_JOB_PREPARE?"Preparing %u / %u":phase==BATCH_JOB_ROLLBACK?"Restoring %u / %u":phase==BATCH_JOB_DONE?"Done: %u / %u":phase==BATCH_JOB_FAILED?"Operation failed":chop_export?"Extracting %u / %u":"Copying %u / %u",done,total);
 text(g,line,8,84,304,27,21);
 unsigned progress_units=phase==BATCH_JOB_DONE?total*2:phase==BATCH_JOB_PREPARE?done:total+done;
 unsigned pixels=total?280*progress_units/(total*2):0;if(pixels>280)pixels=280;
 colour(g,0xff373737);rect(g,20,130,280,10);
 colour(g,phase==BATCH_JOB_FAILED?RED:AMBER);if(pixels)rect(g,20,130,(int)pixels,10);
 colour(g,GREY);text(g,terminal?(phase==BATCH_JOB_DONE?"One Undo restores the whole batch":"Check the log before retrying"):"Sequential work; keep SD inserted",8,160,304,19,12);
 text(g,terminal?"B1 / B3: close":"Please wait",8,202,304,19,12);
 batch_job_presented=1;
}
