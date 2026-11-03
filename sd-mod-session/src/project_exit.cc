#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "project_exit.h"
extern "C" {
#include "music_archive.h"
#ifdef MPC_PROJECT_RECOVERY
#include "project_recovery.h"
#endif
}
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <thread>
#include <chrono>
#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <ctime>
#include <ftw.h>
#define F(p,o,T) (*reinterpret_cast<T*>(reinterpret_cast<char*>(p)+(o)))
static std::atomic<int> state{0};
static char message[160], destination[1400];
static uintptr_t base;
extern "C" void project_exit_submit(void);
extern "C" { uintptr_t project_exit_submit_original; }
static thread_local bool copying;
static thread_local bool completion_seen;
static thread_local bool completion_failed;
struct Pending {void *shared,*control;};
static thread_local Pending pending[8];
static thread_local unsigned pending_count;
static void finish_futures();
static void release_futures();
#define FN(o,T) reinterpret_cast<T>(base+(o))
#ifndef PROJECT_EXIT_METADATA_PATH
#define PROJECT_EXIT_METADATA_PATH "/run/mpc-trial-app/storage-meta/save-path"
#endif
extern "C" int project_exit_status(void){return state.load(std::memory_order_acquire);}
extern "C" const char *project_exit_message(void){return message;}
extern "C" const char *project_exit_path(void){return destination;}
static void *management(void *services){
    if(!services)throw std::runtime_error("Project services unavailable");
    void *lock=static_cast<char*>(services)+0x38;
    if(FN(0xb51828,int(*)(void*))(lock))throw std::runtime_error("Project service lock failed");
    uintptr_t key=base+0x6e0d598;
    void *result=nullptr;
    try{
        void **entry=FN(0x143ad50,void**(*)(void*,const uintptr_t*))(services,&key);
        result=entry?*entry:nullptr;
    }catch(...){FN(0xb51808,void(*)(void*))(lock);throw;}
    FN(0xb51808,void(*)(void*))(lock);
    if(!result||F(result,0,uintptr_t)!=base+0x6d8cc00)
        throw std::runtime_error("Native project saver unavailable");
    return result;
}
static std::string projects_root(){
    int fd=open(PROJECT_EXIT_METADATA_PATH,O_RDONLY|O_CLOEXEC|O_NOFOLLOW);
    if(fd<0)throw std::runtime_error("SD save location unavailable");
    char path[1024];ssize_t n=read(fd,path,sizeof(path)-1);close(fd);
    if(n<=0)throw std::runtime_error("SD save location unreadable");
    while(n>0&&(path[n-1]=='\n'||path[n-1]=='\r'))--n;
    path[n]=0;
    if(n<18||std::strncmp(path,"/media/",7)||std::strstr(path,"/../")||
       std::strlen(path)!=(size_t)n)throw std::runtime_error("SD save location invalid");
    std::string dir=std::string(path)+"/Projects";
    char resolved[PATH_MAX];
    if(!realpath(dir.c_str(),resolved)||dir!=resolved)
        throw std::runtime_error("SD Projects location changed");
    return dir;
}
/* One recovery copy: Projects/Mod-Exit-Last.xpj. A new copy is written into a
 * staging folder first; only after it is saved and flushed does it replace
 * the previous one, so a failed save never loses the last good copy. */
static int remove_entry(const char*p,const struct stat*,int type,struct FTW*){return type==FTW_DP?rmdir(p):unlink(p);}
static void remove_tree(const std::string&root,const std::string&path){
    if(path.compare(0,root.size()+10,root+"/Mod-Exit-"))return; /* only our own folders */
    struct stat st{};if(lstat(path.c_str(),&st)||!S_ISDIR(st.st_mode))return;
    nftw(path.c_str(),remove_entry,16,FTW_DEPTH|FTW_PHYS);
}
static void save(void *saver){
    uint64_t string=0,file=0;std::string root,staging;
    try{
        /* Date-time first so copies sort chronologically in the browser;
         * mkdtemp keeps the name unique without replacing older copies. */
        root=projects_root();
        auto folder=root+"/Mod-Exit-saving-XXXXXX";
        char unique[1200];
        if(folder.size()>=sizeof(unique))throw std::runtime_error("SD path too long");
        std::strcpy(unique,folder.c_str());
        if(!mkdtemp(unique))throw std::runtime_error("Cannot create SD recovery folder");
        staging=unique;
        std::snprintf(destination,sizeof(destination),"%s/Mod-Exit-Last.xpj",unique);
        unsigned serial=music_archive_saved_count();
        FN(0xf535c0,void(*)(uint64_t*,const char*))(&string,destination);
        FN(0xf75630,void(*)(uint64_t*,const uint64_t*))(&file,&string);
        /* Retain the final native write futures, but let SaveProject unwind
         * before waiting. Waiting inside its queue submission can retain
         * native locks required by the sample loader in a large project. */
        completion_seen=completion_failed=false;
        copying=true;
#ifdef MPC_PROJECT_RECOVERY
        project_recovery_begin_copy();
#endif
        int ok=FN(0x1508bc0,int(*)(void*,const uint64_t*,bool,bool,bool))
            (saver,&file,true,true,true);
        copying=false;
        finish_futures();
#ifdef MPC_PROJECT_RECOVERY
        project_recovery_end_copy();
#endif
        FN(0xf0a290,void(*)(uint64_t*))(&file);file=0;
        FN(0xf0a290,void(*)(uint64_t*))(&string);string=0;
        if(!ok||!completion_seen||completion_failed||music_archive_saved_count()==serial||music_archive_status()!=1)
            throw std::runtime_error("Project copy or mod settings save failed");
        int fd=open(destination,O_RDONLY|O_CLOEXEC|O_NOFOLLOW);
        if(fd<0)throw std::runtime_error("Saved project could not be reopened");
        struct stat st{};
        int stat_result=fstat(fd,&st),stat_error=errno;
        int valid=stat_result==0&&S_ISREG(st.st_mode)&&st.st_size>0;
        int flushed=valid?syncfs(fd):-1,flush_error=errno;
        std::fprintf(stderr,"MOD_EXIT FILE_CHECK stat=%d stat_errno=%d mode=%o bytes=%lld syncfs=%d sync_errno=%d\n",
            stat_result,stat_error,(unsigned)st.st_mode,(long long)st.st_size,flushed,flush_error);
        close(fd);
        if(!valid||flushed)throw std::runtime_error("Could not flush project copy to SD");
        /* Place the copy as Projects/Mod-Exit-Last.xpj + Mod-Exit-Last_[ProjectData]
         * so the browser lists it as one project (the stock browser hides
         * _[ProjectData] folders). The previous copy (and the older
         * Mod-Exit-Last folder layout) moves to Mod-Exit-Last-old first and is
         * removed only after the new one is in place. */
        const std::string xpj=root+"/Mod-Exit-Last.xpj",data=root+"/Mod-Exit-Last_[ProjectData]",
            legacy=root+"/Mod-Exit-Last",old=root+"/Mod-Exit-Last-old",
            new_xpj=staging+"/Mod-Exit-Last.xpj",new_data=staging+"/Mod-Exit-Last_[ProjectData]";
        remove_tree(root,old);
        if(mkdir(old.c_str(),0755))throw std::runtime_error("Could not replace the previous copy");
        struct stat entry{};
        int had_xpj=!lstat(xpj.c_str(),&entry),had_data=!lstat(data.c_str(),&entry),had_legacy=!lstat(legacy.c_str(),&entry)&&S_ISDIR(entry.st_mode);
        auto restore=[&]{
            if(had_xpj)rename((old+"/Mod-Exit-Last.xpj").c_str(),xpj.c_str());
            if(had_data)rename((old+"/data").c_str(),data.c_str());
            if(had_legacy)rename((old+"/legacy").c_str(),legacy.c_str());
        };
        if((had_xpj&&rename(xpj.c_str(),(old+"/Mod-Exit-Last.xpj").c_str()))||
           (had_data&&rename(data.c_str(),(old+"/data").c_str()))||
           (had_legacy&&rename(legacy.c_str(),(old+"/legacy").c_str()))){restore();throw std::runtime_error("Could not replace the previous copy");}
        int has_new_data=!lstat(new_data.c_str(),&entry);
        if(rename(new_xpj.c_str(),xpj.c_str())||(has_new_data&&rename(new_data.c_str(),data.c_str()))){
            rename(xpj.c_str(),new_xpj.c_str());restore();
            throw std::runtime_error("Could not replace the previous copy");
        }
        rmdir(staging.c_str());staging.clear();
        remove_tree(root,old);
        {int dir=open(root.c_str(),O_RDONLY|O_CLOEXEC|O_DIRECTORY);if(dir>=0){syncfs(dir);close(dir);}}
        std::snprintf(destination,sizeof(destination),"%s",xpj.c_str());
        std::snprintf(message,sizeof(message),"Saved copy: Mod-Exit-Last");
        std::fprintf(stderr,"MOD_EXIT COPY_READY path=%s\n",destination);
        state.store(2,std::memory_order_release);
    }catch(const std::exception&e){
        copying=false;
#ifdef MPC_PROJECT_RECOVERY
        project_recovery_end_copy();
#endif
        release_futures();
        if(!staging.empty())remove_tree(root,staging);
        if(file)FN(0xf0a290,void(*)(uint64_t*))(&file);
        if(string)FN(0xf0a290,void(*)(uint64_t*))(&string);
        std::snprintf(message,sizeof(message),"%s",e.what());
        std::fprintf(stderr,"MOD_EXIT COPY_FAILED reason=%s path=%s\n",message,destination);
        state.store(3,std::memory_order_release);
    }catch(...){
        copying=false;
#ifdef MPC_PROJECT_RECOVERY
        project_recovery_end_copy();
#endif
        release_futures();
        if(!staging.empty())remove_tree(root,staging);
        if(file)FN(0xf0a290,void(*)(uint64_t*))(&file);
        if(string)FN(0xf0a290,void(*)(uint64_t*))(&string);
        std::snprintf(message,sizeof(message),"Native project copy failed");
        state.store(3,std::memory_order_release);
    }
}
/* Native std::future returned at 2996280: two shared-state pointers, indirect
 * result in x8. Retain a shared-state owner without blocking this submission.
 * The native caller still owns its future and destroys it normally. */
extern "C" void project_exit_capture_future(void *future){
    if(!copying)return;
    completion_seen=true;
    void *shared=F(future,0,void*),*control=F(future,8,void*);
    if(pending_count==8||!shared||!control||shared!=static_cast<char*>(control)+16||
       F(shared,0,uintptr_t)!=base+0x6dbe380||F(control,0,uintptr_t)!=base+0x6dbe4d8){completion_failed=true;return;}
    __atomic_fetch_add(reinterpret_cast<unsigned*>(static_cast<char*>(control)+8),1,__ATOMIC_RELAXED);
    pending[pending_count++]={shared,control};
}
static void release_futures(){
    for(unsigned i=0;i<pending_count;++i)FN(0x123bca0,void(*)(void*))(pending[i].control);
    pending_count=0;
}
static void finish_futures(){
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::minutes(5);
    for(unsigned i=0;i<pending_count;++i){
        void *shared=pending[i].shared;
        while((__atomic_load_n(reinterpret_cast<unsigned*>(static_cast<char*>(shared)+0x10),__ATOMIC_ACQUIRE)&0x7fffffffu)!=1u){
            if(std::chrono::steady_clock::now()>=deadline){completion_failed=true;release_futures();return;}
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        /* Ready also covers an exception in the future<void> task. */
        void *result=F(shared,8,void*);
        if(!result||F(result,8,uintptr_t))completion_failed=true;
        else std::fprintf(stderr,"MOD_EXIT NATIVE_WRITE_FINISHED\n");
    }
    release_futures();
}
extern "C" int project_exit_install(uintptr_t b){
    if(project_exit_submit_original)return base==b;
    if(!b||sysconf(_SC_PAGESIZE)!=4096)return 0;
    const uint32_t expected[]={0xd503233f,0xd10203ff,0xa9037bfd,0x9100c3fd};
    void *entry=reinterpret_cast<void*>(b+0x2996280);
    if(std::memcmp(entry,expected,sizeof(expected)))return 0;
    void *trampoline=mmap(nullptr,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if(trampoline==MAP_FAILED)return 0;
    uint32_t code[]={0xd503245f,expected[0],expected[1],expected[2],expected[3],0x58000070,0xd61f0200,0xd503201f};
    std::memcpy(trampoline,code,sizeof(code));F(trampoline,32,uintptr_t)=b+0x2996290;
    __builtin___clear_cache(static_cast<char*>(trampoline),static_cast<char*>(trampoline)+40);
    if(mprotect(trampoline,4096,PROT_READ|PROT_EXEC)){munmap(trampoline,4096);return 0;}
    void *page=reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(entry)&~uintptr_t(4095));
    if(mprotect(page,4096,PROT_READ|PROT_WRITE)){munmap(trampoline,4096);return 0;}
    base=b;project_exit_submit_original=reinterpret_cast<uintptr_t>(trampoline);
    const uint32_t jump[]={0x58000050,0xd61f0200};std::memcpy(entry,jump,8);F(entry,8,uintptr_t)=reinterpret_cast<uintptr_t>(project_exit_submit);
    __builtin___clear_cache(static_cast<char*>(entry),static_cast<char*>(entry)+16);
    if(mprotect(page,4096,PROT_READ|PROT_EXEC))_exit(121);
    return 1;
}
extern "C" int project_exit_start(uintptr_t b,void *services){
    if(state.load()==1)return 0;
    base=b;message[0]=destination[0]=0;
    try{
        void *saver=management(services);
#ifdef MPC_PROJECT_RECOVERY
        project_recovery_capture(b,saver);
#endif
        state.store(1,std::memory_order_release);
        std::thread(save,saver).detach();
        return 1;
    }catch(const std::exception&e){
        std::snprintf(message,sizeof(message),"%s",e.what());
        state.store(3,std::memory_order_release);return 0;
    }catch(...){
        std::snprintf(message,sizeof(message),"Could not start project copy");
        state.store(3,std::memory_order_release);return 0;
    }
}
