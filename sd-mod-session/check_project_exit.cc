/* Production worker and real host filesystem; native saver/DI/string calls are receivers.
 * Full native SaveProject and reload are exercised separately in the QEMU lab. */
#include <cassert>
#include <chrono>
#include <sys/mman.h>
#include <vector>
#include <fstream>
static const char *test_metadata();
#define PROJECT_EXIT_METADATA_PATH test_metadata()
#include "src/project_exit.cc"
static char metadata[1024], storage[1024], object[0x200], services[0x80];
static void *native_object=object;
static std::atomic<unsigned> saved{0};
static std::atomic<bool> native_returned{false};
static int failure;
static std::vector<std::string> folders;
extern "C" unsigned music_archive_saved_count(void){return saved.load();}
extern "C" int music_archive_status(void){return 1;}
static const char *test_metadata(){return metadata;}
static int lock_receiver(void *p){assert(p==services+0x38);return 0;}
static void *entry_receiver(void *s,const uintptr_t *type){assert(s==services&&*type==base+0x6e0d598);return &native_object;}
static void string_receiver(uint64_t *s,const char *text){*s=(uintptr_t)strdup(text);}
static void file_receiver(uint64_t *f,const uint64_t *s){*f=(uintptr_t)strdup((const char*)*s);}
static void destroy_receiver(uint64_t *s){free((void*)*s);*s=0;}
static void release_receiver(void *control){
    if(__atomic_sub_fetch(reinterpret_cast<unsigned*>(static_cast<char*>(control)+8),1,__ATOMIC_ACQ_REL)==0){
        free(F(control,24,void*));free(control);
    }
}
static int save_receiver(void *p,const uint64_t *file,bool samples,bool copy,bool native_options){
    assert(p==object&&samples&&copy&&native_options);
    std::string path=(const char*)*file;
    assert(path.find(std::string(storage)+"/MPC-Sample/Projects/Mod-Exit-")==0);
    assert(path.substr(path.size()-12)=="/Project.xpj");
    folders.push_back(path.substr(0,path.size()-12));
    if(failure==1)return 0;
    std::ofstream output(path);output<<"native project fixture";output.close();
    if(failure!=2)++saved;
    auto *control=static_cast<char*>(calloc(1,0x58));assert(control);
    void *shared=control+16;auto *result=static_cast<uintptr_t*>(calloc(2,sizeof(uintptr_t)));assert(result);
    result[1]=failure==3?1u:0u;
    F(control,0,uintptr_t)=base+0x6dbe4d8;F(control,8,unsigned)=1;
    F(shared,0,uintptr_t)=base+0x6dbe380;
    F(shared,8,void*)=result;
    void *future[2]={shared,control};native_returned=false;
    project_exit_capture_future(future);
    __atomic_fetch_add(reinterpret_cast<unsigned*>(control+8),1,__ATOMIC_RELAXED);
    std::thread completion([shared,control]{
        while(!native_returned.load())std::this_thread::sleep_for(std::chrono::milliseconds(1));
        assert(project_exit_status()==1);
        __atomic_store_n(reinterpret_cast<unsigned*>(static_cast<char*>(shared)+0x10),0x80000001u,__ATOMIC_RELEASE);
        release_receiver(control);
    });completion.detach();
    release_receiver(control);native_returned=true;
    return 1;
}
static void stub(uintptr_t off,void *fn){uint32_t words[]={0x58000050,0xd61f0200};memcpy((void*)(base+off),words,8);memcpy((void*)(base+off+8),&fn,8);}
static int wait_job(){
    for(unsigned n=0;n<2000&&project_exit_status()==1;++n)std::this_thread::sleep_for(std::chrono::milliseconds(2));
    assert(project_exit_status()!=1);return project_exit_status();
}
int main(){
    void *memory=mmap(nullptr,0x7500000,7,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(memory!=MAP_FAILED);base=(uintptr_t)memory;
    stub(0xb51828,(void*)lock_receiver);stub(0xb51808,(void*)lock_receiver);stub(0x143ad50,(void*)entry_receiver);
    stub(0xf535c0,(void*)string_receiver);stub(0xf75630,(void*)file_receiver);stub(0xf0a290,(void*)destroy_receiver);stub(0x1508bc0,(void*)save_receiver);
    stub(0x123bca0,(void*)release_receiver);
    __builtin___clear_cache((char*)memory,(char*)memory+0x7500000);F(object,0,uintptr_t)=base+0x6d8cc00;
    strcpy(storage,"/media/mpc-exit-lab-XXXXXX");assert(mkdtemp(storage));
    std::string root=std::string(storage)+"/MPC-Sample";assert(!mkdir(root.c_str(),0700));assert(!mkdir((root+"/Projects").c_str(),0700));
    strcpy(metadata,"/tmp/mpc-exit-meta-XXXXXX");int fd=mkstemp(metadata);assert(fd>=0);close(fd);
    {std::ofstream file(metadata);file<<root<<'\n';}
    std::string previous;
    for(unsigned n=0;n<2;++n){assert(project_exit_start(base,services));assert(wait_job()==2);std::string path=project_exit_path();assert(path!=previous);std::ifstream file(path);std::string contents;std::getline(file,contents);assert(contents=="native project fixture");previous=path;}
    failure=1;assert(project_exit_start(base,services));assert(wait_job()==3&&strstr(project_exit_message(),"copy"));
    failure=2;assert(project_exit_start(base,services));assert(wait_job()==3&&strstr(project_exit_message(),"settings"));
    failure=3;assert(project_exit_start(base,services));assert(wait_job()==3&&strstr(project_exit_message(),"copy"));
    assert(!unlink(metadata));assert(!symlink("/etc/passwd",metadata));assert(project_exit_start(base,services));assert(wait_job()==3&&strstr(project_exit_message(),"unavailable"));assert(!unlink(metadata));
    F(object,0,uintptr_t)=0;assert(!project_exit_start(base,services)&&project_exit_status()==3);
    for(auto &folder:folders){unlink((folder+"/Project.xpj").c_str());assert(!rmdir(folder.c_str()));}
    assert(!rmdir((root+"/Projects").c_str()));assert(!rmdir(root.c_str()));assert(!rmdir(storage));
    munmap(memory,0x7500000);
    puts("PASS production exit-copy worker: unique SD folders/no overwrite, native Save As options, serializer counter proof, reopened/flushed result, save/metadata/service failures stay errors; native saver is a receiver");
}
