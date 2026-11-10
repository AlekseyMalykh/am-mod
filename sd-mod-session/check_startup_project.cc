#define _GNU_SOURCE 1
#include <cassert>
#include <dlfcn.h>
#include <elf.h>
#include <stdexcept>
#include <string>
#include <vector>
#include <fstream>
static const char*metadata;
static const char*autosave_path;
static const char*temp_root;
static unsigned named_saved;static std::string named_target;
#define MPC_PROJECT_RECOVERY 1
#define STARTUP_STORAGE_META metadata
#define STARTUP_AUTOSAVE_PATH autosave_path
#define STARTUP_TEMP_ROOT temp_root
#include "src/startup_project.cc"
extern "C" void project_recovery_saved(const char*path){++named_saved;named_target=path;}
static void elf(const char*path,uintptr_t base){
 FILE*f=fopen(path,"rb");assert(f);Elf64_Ehdr h;assert(fread(&h,1,sizeof(h),f)==sizeof(h));
 auto*memory=static_cast<unsigned char*>(mmap(reinterpret_cast<void*>(base),0x7500000,7,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0));assert(memory==reinterpret_cast<void*>(base));
 Elf64_Phdr ph[16];assert(h.e_phnum<=16);fseek(f,h.e_phoff,SEEK_SET);assert(fread(ph,sizeof(*ph),h.e_phnum,f)==h.e_phnum);
 Elf64_Dyn*dynamic=nullptr;
 for(unsigned i=0;i<h.e_phnum;++i){if(ph[i].p_type==PT_LOAD){assert(ph[i].p_vaddr+ph[i].p_memsz<0x7500000);fseek(f,ph[i].p_offset,SEEK_SET);assert(fread(memory+ph[i].p_vaddr,1,ph[i].p_filesz,f)==ph[i].p_filesz);}if(ph[i].p_type==PT_DYNAMIC)dynamic=reinterpret_cast<Elf64_Dyn*>(memory+ph[i].p_vaddr);}
 assert(dynamic);uintptr_t rela=0,relasz=0,jmp=0,jmpsz=0,str=0,sym=0;
 for(auto*d=dynamic;d->d_tag;++d)switch(d->d_tag){case DT_RELA:rela=d->d_un.d_ptr;break;case DT_RELASZ:relasz=d->d_un.d_val;break;case DT_JMPREL:jmp=d->d_un.d_ptr;break;case DT_PLTRELSZ:jmpsz=d->d_un.d_val;break;case DT_STRTAB:str=d->d_un.d_ptr;break;case DT_SYMTAB:sym=d->d_un.d_ptr;break;}
 uintptr_t starts[]={rela,jmp},sizes[]={relasz,jmpsz};
 for(unsigned k=0;k<2;++k)for(size_t i=0;i<sizes[k]/sizeof(Elf64_Rela);++i){auto*r=reinterpret_cast<Elf64_Rela*>(memory+starts[k]+i*sizeof(Elf64_Rela));auto*out=reinterpret_cast<uintptr_t*>(memory+r->r_offset);if(ELF64_R_TYPE(r->r_info)==R_AARCH64_RELATIVE)*out=base+r->r_addend;else{auto*s=reinterpret_cast<Elf64_Sym*>(memory+sym+ELF64_R_SYM(r->r_info)*sizeof(Elf64_Sym));auto*fn=dlsym(RTLD_DEFAULT,reinterpret_cast<char*>(memory+str+s->st_name));if(fn)*out=reinterpret_cast<uintptr_t>(fn)+r->r_addend;}}
 fclose(f);static uintptr_t guard=0x12345;*reinterpret_cast<void**>(base+0x7164398)=&guard;
}
static unsigned calls;static std::string loaded;static bool throwing;
static std::string workspace;static bool remove_workspace;
static Errors receiver(void*saver,const uint64_t*file){
 assert(saver==reinterpret_cast<void*>(0x1234));++calls;loaded=reinterpret_cast<const char*>(*file);
 struct stat st;assert(!lstat(workspace.c_str(),&st)&&S_ISDIR(st.st_mode));
 if(throwing)throw std::runtime_error("native load exception");
 if(remove_workspace){assert(!rmdir(workspace.c_str()));assert(!rmdir(temp_root));}
 auto*value=new std::vector<std::string>{"native error result"};static_assert(sizeof(*value)==sizeof(Errors),"vector result ABI");
 Errors out;memcpy(static_cast<void*>(&out),static_cast<void*>(value),sizeof(out));::operator delete(value);return out;
}
static void invoke(const char*path){
 File file(path);using RealLoad=std::vector<std::string>(*)(void*,const uint64_t*);
 auto fn=*reinterpret_cast<RealLoad*>(image+0x6d8cc20);auto result=fn(reinterpret_cast<void*>(0x1234),&file.value);
 assert(result.size()==1&&result[0]=="native error result"); /* Real caller destroys the transferred vector. */
}
static unsigned save_calls,auto_calls;static bool save_ok=true;
static int save_current(void*,const uint64_t*file,bool a,bool b,bool c){
 assert(!a&&!b&&!c);assert(!strcmp(reinterpret_cast<const char*>(*file),autosave_path));++save_calls;
 if(!save_ok)return 0;
 std::ofstream f(autosave_path);f<<"current NEW project, not the previous SD project";return !!f;
}
static void auto_receiver(void*){++auto_calls;}
static unsigned explicit_calls;static bool explicit_ok=true;
static int explicit_save(void*p,const uint64_t*file,bool a,bool b,bool c){
 assert(p==(void*)0x1234&&file&&a&&!b&&c);++explicit_calls;return explicit_ok?7:0;
}
int main(int argc,char**argv){
 assert(argc==2);setbuf(stdout,nullptr);elf(argv[1],0x5000000000ULL);image=0x5000000000ULL;
 char dir[]="/media/am-startup-XXXXXX";assert(mkdtemp(dir));std::string root=std::string(dir)+"/MPC-Sample",projects=root+"/Projects";assert(!mkdir(root.c_str(),0700)&&!mkdir(projects.c_str(),0700));
 std::string meta=std::string(dir)+"/meta";metadata=meta.c_str();{std::ofstream f(meta);f<<root<<'\n';}
 std::string temp_folder=root+"/TempProjects";temp_root=temp_folder.c_str();workspace=temp_folder+"/Temp-Project_[ProjectData]";remove_workspace=true;
 auto create=[](const std::string&p){std::ofstream f(p);f<<"project fixture";};
 auto a=projects+"/A.xpj",b=projects+"/B.xpj";create(a);create(b);create(projects+"/Mod-Exit-saving-bad.xpj");
 struct timespec stamp[2]={{1000,0},{1000,0}};assert(!utimensat(AT_FDCWD,a.c_str(),stamp,0)&&!utimensat(AT_FDCWD,b.c_str(),stamp,0));
 assert(!symlink("B.xpj",(projects+"/Z.xpj").c_str()));
 assert(startup_project_install(image));assert(!strcmp(selected,b.c_str()));original=receiver;
 original_save=explicit_save;File explicit_file(b.c_str());
 auto save_method=*reinterpret_cast<Save*>(image+0x6d8cc18);
 assert(save_method((void*)0x1234,&explicit_file.value,true,false,true)==7&&explicit_calls==1&&named_saved==1&&named_target==b);
 explicit_ok=false;assert(!save_method((void*)0x1234,&explicit_file.value,true,false,true)&&explicit_calls==2&&named_saved==1);
 File internal("/data/UserContent/Projects/Internal.xpj");
 File linked((projects+"/Z.xpj").c_str());
 assert(!save_method((void*)0x1234,&internal.value,true,false,true));
 assert(!save_method((void*)0x1234,&linked.value,true,false,true));
 assert(explicit_calls==2&&named_saved==1);
 puts("PASS native Save vtable forwards exact arguments/result once and remembers only its successful named target");
 invoke("/data/TempProjects/Temp-Project.xpj");assert(calls==1&&loaded==b&&phase==3&&!startup_project_loading());
 invoke(a.c_str());assert(calls==2&&loaded==a);puts("PASS first native Temp load selects SD once; later manual Load unchanged; actual AArch64 nontrivial vector result and native File/String ownership retained");
 assert(!access(workspace.c_str(),F_OK));puts("PASS workspace exists before the native load and is recreated after its cleanup, for boot and later manual project loads");
 phase=1;throwing=true;try{invoke("/data/TempProjects/Temp-Project.xpj");assert(false);}catch(const std::runtime_error&){}throwing=false;assert(phase==3&&!startup_project_loading());
 phase=1;invoke(a.c_str());assert(loaded==a&&phase==3);puts("PASS unexpected initial path and native exceptions do not trigger a second load or leave the splash waiting forever");
 phase=1;startup_project_ready();assert(phase==3&&!startup_project_loading());
 phase=2;startup_project_ready();assert(phase==2&&startup_project_loading());
 phase=3;invoke(a.c_str());assert(loaded==a);
 puts("PASS empty native boot may finish at New Project without a Load; ready screen cannot interrupt an in-flight native Load or intercept a later manual Load");
 unlink((projects+"/Z.xpj").c_str());unlink(a.c_str());unlink(b.c_str());unlink((projects+"/Mod-Exit-saving-bad.xpj").c_str());
 selected[0]=0;choose();assert(!selected[0]);phase=1;invoke("/data/TempProjects/Temp-Project.xpj");assert(loaded=="/data/TempProjects/Temp-Project.xpj");
 /* Reproduce the device's ENOENT using the real firmware File::copyFileTo.
  * Then exercise New Project and the no-load boot path, including file writes
  * needed by render and subsequent copies, not merely a mkdir assertion. */
 std::string source=projects+"/source.wav",destination=workspace+"/assigned.wav";create(source);
 File from(source.c_str()),to(destination.c_str());
 auto copy_file=reinterpret_cast<int(*)(const uint64_t*,const uint64_t*)>(image+0xf5cc80);
 assert(!rmdir(workspace.c_str()));assert(!copy_file(&from.value,&to.value));
 startup_project_changed();assert(copy_file(&from.value,&to.value));
 {std::ifstream f(destination);std::string text;getline(f,text);assert(text=="project fixture");}
 auto render=workspace+"/render.wav";{std::ofstream f(render);f<<"render output";assert(f.good());}
 startup_project_ready();assert(!access(destination.c_str(),F_OK)&&!access(render.c_str(),F_OK)); /* Keep existing audio. */
 assert(!unlink(destination.c_str())&&!unlink(render.c_str())&&!unlink(source.c_str())&&!rmdir(workspace.c_str()));
 phase=1;workspace_checked=false;startup_project_ready();assert(phase==3&&!access(workspace.c_str(),F_OK));
 assert(!rmdir(workspace.c_str()));assert(!symlink(projects.c_str(),workspace.c_str()));assert(!prepare_workspace());assert(!unlink(workspace.c_str()));
 {std::ofstream f(workspace);f<<"blocked";}assert(!prepare_workspace());assert(!unlink(workspace.c_str()));
 assert(prepare_workspace());
 errno=EACCES;assert(prepare_workspace()&&errno==EACCES);
 puts("PASS native copy reproduces ENOENT before workspace initialization and succeeds after New Project; render output writable; no-load boot restores workspace; existing audio preserved; linked/file targets rejected; errno preserved");
 /* The installed native timer vtable now bootstraps only a missing canonical
  * temp file. Existing/native errors, external projects and prefs stay native. */
 std::string temp=root+"/Temp-Project.xpj";autosave_path=temp.c_str();
 alignas(8) unsigned char saver[0x100]={},things[0xb00]={},prefs[0x100]={};
 *reinterpret_cast<void**>(saver+0x90)=things;*reinterpret_cast<void**>(saver+0x50)=prefs;
 *reinterpret_cast<uintptr_t*>(things)=image+0x6ecc648;
 *reinterpret_cast<const char**>(things+0x9b0)=autosave_path;
 *reinterpret_cast<const char**>(prefs+0xa8)=autosave_path;
 uintptr_t target=image+0x1508bc0;uint32_t jump[]={0x58000050,0xd61f0200};
 memcpy(reinterpret_cast<void*>(target),jump,sizeof(jump));*reinterpret_cast<uintptr_t*>(target+8)=reinterpret_cast<uintptr_t>(save_current);
 __builtin___clear_cache(reinterpret_cast<char*>(target),reinterpret_cast<char*>(target+16));original_autosave=auto_receiver;
 auto timer=*reinterpret_cast<AutoSave*>(image+0x6d8cc10);assert(phase==3);
 timer(saver);assert(save_calls==1&&!auto_calls);{std::ifstream f(temp);std::string text;getline(f,text);assert(text=="current NEW project, not the previous SD project");}
 timer(saver);assert(save_calls==1&&auto_calls==1);assert(!strcmp(*reinterpret_cast<const char**>(prefs+0xa8),autosave_path));
 assert(!unlink(temp.c_str()));save_ok=false;timer(saver);assert(save_calls==2&&auto_calls==2);save_ok=true;
 *reinterpret_cast<const char**>(things+0x9b0)="/media/External/Original.xpj";timer(saver);assert(save_calls==2&&auto_calls==3);
 *reinterpret_cast<const char**>(things+0x9b0)=autosave_path;*reinterpret_cast<const char**>(prefs+0xa8)="/media/External/Original.xpj";timer(saver);assert(save_calls==2&&auto_calls==4);
 puts("PASS missing internal Temp XPJ is initialized through native SaveProject with current data; existing/external paths and failed writes retain native handling, autosave preferences unchanged");
 unlink(meta.c_str());assert(!rmdir(workspace.c_str())&&!rmdir(temp_root)&&!rmdir(projects.c_str())&&!rmdir(root.c_str())&&!rmdir(dir));
 puts("PASS deterministic newest-XPJ selection skips links and unfinished exit copies; empty SD preserves native recovery without extra autoload");
 return 0;
}
