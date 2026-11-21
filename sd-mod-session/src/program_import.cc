/* Pinned firmware 1.3.0.12. The full-MPC ProgramLoader helper is deliberately
 * avoided: its engine offsets do not describe AC50Engine. */
#include "program_import.h"
#include "program_pgm.h"
#include "program_manifest.h"
#include <memory>
#include <dirent.h>
extern "C" {
#include "midi_file_native.h"
void step_layer_sample(void*,int,uintptr_t*,uintptr_t);
int batch_copy_import_program_submit(uintptr_t,void*,void*,const unsigned*,const unsigned*,unsigned);
}
#include <cstdio>
#include <cstring>
#include <strings.h>
#include <cstdlib>
#include <climits>
#include <string>
#include <vector>
#include <sys/stat.h>
#define F(p,o,T) (*reinterpret_cast<T*>(reinterpret_cast<char*>(p)+(o)))
static uintptr_t image;
#define FN(o,T) reinterpret_cast<T>(image+(o))
static void *reader,*source,*target,*context;
static bool xml,loading,close_requested;
static unsigned used[128],layer_count;
/* MPCProgramFileXML reads only the native routing-limit value at +28.
 * AC50 has four engine output channels (analogue stereo plus USB stereo);
 * the full-MPC preferences owner supplying this value does not exist here. */
struct XMLRoutingLimits {unsigned char reserved[28];unsigned output_channels;};
static const XMLRoutingLimits routing_limits={{},4};
/* XML reader borrows a progress interface: deleting destructor occupies slot
 * 1, report(double) slot 2. No full-MPC UI services enter the AC50 reader. */
struct XMLProgress {virtual ~XMLProgress(){};virtual void report(double){}};
static XMLProgress xml_progress;
static_assert(offsetof(XMLRoutingLimits,output_channels)==28,"XML routing-limit ABI");
static int fail(char*e,const char*s){std::snprintf(e,96,"%s",s);return 0;}
extern "C" void program_import_close(){
 if(loading){close_requested=true;return;}
 void*owned=reader;bool was_xml=xml;reader=source=target=context=nullptr;std::memset(used,0,sizeof used);layer_count=0;
 if(owned){if(was_xml)FN(0x1cbf200,void(*)(void*))(owned);else FN(0x1c862f0,void(*)(void*))(owned);::operator delete(owned);}
}
static void *core(){
 void*a=F(image,0x71e3dc8,void*);if(!a||F(a,0,uintptr_t)!=image+0x6e07288)return nullptr;
 a=F(a,0xd8,void*);if(!a||F(a,0,uintptr_t)!=image+0x6e0c430)return nullptr;
 a=F(a,0x148,void*);return a&&F(a,0,uintptr_t)==image+0x6e0cf38?a:nullptr;
}
static bool same_project(){
 void*t=context?F(context,0x78,void*):nullptr;
 return t&&F(t,0xb8,void*)==target;
}
extern "C" int program_import_plan(unsigned bank,unsigned*pads,unsigned*layers,char error[96]){
 if(pads)*pads=0;
 if(layers)*layers=layer_count;
 if(!source||bank>7||!same_project())return fail(error,"Project changed; reopen program");
 unsigned count=0;
 for(unsigned p=0;p<128;++p)if(used[p]){
  unsigned to=bank*16+p;if(to>=128)return fail(error,"Program exceeds bank H; choose earlier bank");
  void*i=FN(0x286bbe0,void*(*)(void*,int))(target,(int)to);
  if(!i||FN(0x2804760,int(*)(void*))(i)){std::snprintf(error,96,"Destination %c%02u is occupied",'A'+to/16,to%16+1);return 0;}
  ++count;
 }
 if(pads)*pads=count;
 if(!count)return fail(error,"Program contains no loaded samples");
 error[0]=0;return 1;
}
static bool companion(const char*program,const program_sample&s){
 std::string relative=*s.file?s.file:std::string(s.name)+".wav";
 for(char&c:relative)if(c=='\\')c='/';
 if(relative.empty()||relative[0]=='/'||relative.find(':')!=std::string::npos)return false;
 size_t at=0;while(at<relative.size()){size_t end=relative.find('/',at);std::string part=relative.substr(at,end-at);if(part==".."||part==".")return false;if(end==std::string::npos)break;at=end+1;}
 std::string dir(program);dir.resize(dir.rfind('/'));std::string path=dir+"/"+relative;
 char canonical[PATH_MAX];struct stat st;
 if(!realpath(path.c_str(),canonical)){
  size_t slash=path.rfind('/');std::string folder=path.substr(0,slash),name=path.substr(slash+1);DIR*d=opendir(folder.c_str());if(!d)return false;
  unsigned found=0;while(dirent*e=readdir(d))if(!strcasecmp(e->d_name,name.c_str())){path=folder+"/"+e->d_name;++found;}
  closedir(d);if(found!=1||!realpath(path.c_str(),canonical))return false;
 }
 return !std::strncmp(canonical,dir.c_str(),dir.size())&&canonical[dir.size()]=='/'&&!stat(canonical,&st)&&S_ISREG(st.st_mode)&&st.st_size>44;
}
static int open_program(uintptr_t b,const char*path,void*c,char error[96]){
 image=b;
 if(!b||!path||!c)return fail(error,"Program services unavailable");
 char root[1024],real[PATH_MAX];struct stat st;
 if(!am_midi_file_root(root,sizeof root)||!realpath(path,real)||std::strcmp(path,real)||std::strncmp(real,root,std::strlen(root))||real[std::strlen(root)]!='/'||stat(real,&st)||!S_ISREG(st.st_mode)||st.st_size<16||st.st_size>16*1024*1024)return fail(error,"Choose a program file on this card");
 const char*ext=std::strrchr(path,'.');xml=ext&&!strcasecmp(ext,".xpm");
 if(!ext||(!xml&&strcasecmp(ext,".pgm")))return fail(error,"Expected PGM or XPM");
 auto manifest=std::unique_ptr<program_manifest>(new program_manifest{});
 if(xml){
  std::vector<char> bytes((size_t)st.st_size);FILE*f=std::fopen(path,"rb");if(!f)return fail(error,"Could not open XPM");
  size_t n=std::fread(bytes.data(),1,bytes.size(),f);std::fclose(f);
  if(n!=bytes.size()||!program_xpm_read(bytes.data(),n,manifest.get(),error))return 0;
 }else{
  unsigned char bytes[AM_PGM_BYTES+1];FILE*f=std::fopen(path,"rb");if(!f)return fail(error,"Could not open program");
  size_t n=std::fread(bytes,1,sizeof bytes,f);std::fclose(f);am_pgm_program p;am_pgm_error e;
  if(am_pgm_read(bytes,n,&p,&e)!=AM_PGM_OK)return fail(error,"Supported PGM: MPC1000 PGM 1.00");
  for(unsigned pad=0;pad<64;++pad)for(unsigned l=0;l<4;++l)if(p.pads[pad].layers[l].name[0]){auto&s=manifest->samples[manifest->count++];s.pad=pad;s.layer=l;std::snprintf(s.name,sizeof s.name,"%s",p.pads[pad].layers[l].name);}
 }
 for(unsigned i=0;i<manifest->count;++i)if(!companion(path,manifest->samples[i])){std::snprintf(error,96,"Missing or invalid sample: %.60s",manifest->samples[i].name);return 0;}
 void*engine=core();if(!engine)return fail(error,"Native engine unavailable");
 void*t=F(c,0x78,void*);target=t?F(t,0xb8,void*):nullptr;context=c;
 if(!target)return fail(error,"Destination program unavailable");
 void*resolver=F(engine,0x560,void*),*queues=F(engine,0x1d0,void*),*factory=F(engine,0x558,void*);
 if(!resolver||F(resolver,0,uintptr_t)!=b+0x6d9c990||!queues||F(queues,0,uintptr_t)!=b+0x6dbfa08||!factory)return fail(error,"Unsupported program services");
 uint64_t file=0,empty=0;void*memory=nullptr;bool constructed=false;
 struct NativeError{int64_t code;std::string message;};static_assert(sizeof(NativeError)==40,"Native error ABI");
 try{
  FN(0xf535c0,void(*)(void*,const char*))(&file,path);FN(0xf1c620,void(*)(void*))(&empty);
  memory=::operator new(xml?0xc8:0x60);
  if(xml){
   /* XML's ProgramLoader base is at +48. Remaining services describe Drum
    * parameters; plugin/keygroup programs are rejected before pad access. */
   FN(0x1cbb210,void(*)(void*,void*,void*,void*,void*,void*,void*,void*))
    (memory,resolver,queues,F(engine,0x4b8,void*),&xml_progress,const_cast<XMLRoutingLimits*>(&routing_limits),factory,nullptr);
  }else FN(0x1c83510,void(*)(void*,void*,void*,void*))(memory,resolver,queues,factory);
  constructed=true;reader=memory;
  void*base=xml?static_cast<char*>(reader)+0x48:reader;
  auto errors=xml?FN(0x1cbc870,std::vector<NativeError>(*)(void*,const void*,const void*,int))(base,&file,&empty,0):FN(0x1c857e0,std::vector<NativeError>(*)(void*,const void*,const void*,int))(base,&file,&empty,0);
  source=F(base,0x50,void*);
  if(!errors.empty()){std::fprintf(stderr,"PROGRAM_IMPORT reader error=%lld %s\n",(long long)errors[0].code,errors[0].message.c_str());fail(error,"Program or samples could not be loaded");source=nullptr;}
  if(source&&!same_project()){fail(error,"Project changed during loading");source=nullptr;}
  if(source&&F(source,0,uintptr_t)!=F(target,0,uintptr_t)){fail(error,"Only Drum programs are supported");source=nullptr;}
  bool invalid_sample=false;
  if(source){
   for(unsigned p=0;p<128;++p){
    void*i=FN(0x286bbe0,void*(*)(void*,int))(source,(int)p);if(!i)continue;
    for(int l=0;l<4;++l){uintptr_t sample[2]={};step_layer_sample(i,l,sample,b+0x2803b90);
     if(sample[0]){if(FN(0x2941f80,int(*)(void*))((void*)sample[0])<=0){invalid_sample=true;fail(error,"Sample has no audio data");}
      else{used[p]=1;++layer_count;}}
     if(sample[1])FN(0x123bca0,void(*)(void*))((void*)sample[1]);
    }
   }
  }
  if(invalid_sample)source=nullptr;
  if(source&&layer_count!=manifest->count){source=nullptr;fail(error,"Not all declared sample layers were loaded");}
 }catch(...){source=nullptr;fail(error,"Program loader failed");if(memory&&!constructed)::operator delete(memory);}
 if(file)FN(0xf0a290,void(*)(void*))(&file);
 if(empty)FN(0xf0a290,void(*)(void*))(&empty);
 if(!source){program_import_close();return 0;}
 unsigned count=0;for(unsigned n:used)count+=n;
 if(!count){program_import_close();return fail(error,"Program contains no loaded samples");}
 std::fprintf(stderr,"PROGRAM_IMPORT prepared pads=%u layers=%u xml=%d\n",count,layer_count,xml);
 error[0]=0;return 1;
}
extern "C" int program_import_commit(unsigned bank,char error[96]){
 unsigned pads=0,layers=0;if(!program_import_plan(bank,&pads,&layers,error))return 0;
 unsigned from[128],to[128],n=0;for(unsigned p=0;p<128;++p)if(used[p]){from[n]=p;to[n++]=bank*16+p;}
 if(loading)return fail(error,"Program is already loading");
 loading=true;close_requested=false;
 int applied=batch_copy_import_program_submit(image,context,source,from,to,n);loading=false;
 if(!applied){if(close_requested)program_import_close();return fail(error,"Import failed; destinations were not replaced");}
 std::fprintf(stderr,"PROGRAM_IMPORT committed pads=%u bank=%u\n",n,bank);program_import_close();error[0]=0;return 1;
}

extern "C" int program_import_open(uintptr_t b,const char*path,void*c,char error[96]){
 if(loading)return fail(error,"Program is already loading");
 program_import_close();loading=true;close_requested=false;int result=0;
 try{result=open_program(b,path,c,error);}catch(...){fail(error,"Program preparation failed");}
 loading=false;
 if(!result||close_requested){program_import_close();if(result)fail(error,"Program loading was cancelled");return 0;}
 return result;
}
