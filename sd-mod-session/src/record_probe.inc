/* Opt-in diagnostics only. Runs on the existing model owner, never audio.
 * Hash the actual retained native layer stream, not a selected UI waveform.
 * No Program/SamplePool mutations and no recording-file writes. */
#include "pad_sample_source.h"
#include <dlfcn.h>
#include <dirent.h>
#include <fcntl.h>
#include <algorithm>
namespace {
std::atomic<unsigned> probe_id{0},probe_pad{128},probe_state{0};
struct ProbeHash {
 void *ctx=nullptr;
 int (*update)(void*,const void*,size_t)=nullptr;
 int (*finish)(void*,unsigned char*,unsigned*)=nullptr;
 void (*release)(void*)=nullptr;
 ProbeHash(){
  static void*lib=dlopen("libcrypto.so.3",RTLD_NOW|RTLD_LOCAL);
  if(!lib)return;
  auto create=reinterpret_cast<void*(*)()>(dlsym(lib,"EVP_MD_CTX_new"));
  auto sha=reinterpret_cast<const void*(*)()>(dlsym(lib,"EVP_sha256"));
  auto init=reinterpret_cast<int(*)(void*,const void*,void*)>(dlsym(lib,"EVP_DigestInit_ex"));
  update=reinterpret_cast<int(*)(void*,const void*,size_t)>(dlsym(lib,"EVP_DigestUpdate"));
  finish=reinterpret_cast<int(*)(void*,unsigned char*,unsigned*)>(dlsym(lib,"EVP_DigestFinal_ex"));
  release=reinterpret_cast<void(*)(void*)>(dlsym(lib,"EVP_MD_CTX_free"));
  if(create&&sha&&init&&update&&finish&&release){ctx=create();if(ctx&&!init(ctx,sha(),nullptr)){release(ctx);ctx=nullptr;}}
 }
 ~ProbeHash(){if(ctx)release(ctx);}
 void add(const void*p,size_t n){if(ctx&&!update(ctx,p,n)){release(ctx);ctx=nullptr;}}
 void end(char out[65]){unsigned char bytes[32];unsigned n=0;std::strcpy(out,"unavailable");if(ctx&&finish(ctx,bytes,&n)&&n==32)for(unsigned i=0;i<32;++i)std::snprintf(out+i*2,3,"%02x",bytes[i]);}
};
void record_probe_snapshot(uintptr_t image,void*copy){
 unsigned id=probe_id.load(),pad=probe_pad.load();if(!id||pad>=128)return;
 void*things=FIELD(copy,0x78,void*);void*program=things?FIELD(things,0xb8,void*):nullptr;
 if(!program)return;
 void*instrument=reinterpret_cast<void*(*)(void*,int)>(image+0x286bbe0)(program,(int)pad);
 uintptr_t sample[2]={};if(instrument)pad_source_layer_sample(instrument,0,sample,image+0x2803b90);
 if(sample[0]&&sample[1]){
  pad_render_stream stream{};
  int opened=pad_sample_stream_open(image,sample,&stream);
  char digest[65]="not-opened";uint64_t frames_read=0;
  if(opened==PAD_RENDER_WORKING){
   ProbeHash hash;float pcm[512];
   /* Full short takes, capped at60s for diagnostic responsiveness. */
   uint64_t limit=std::min(stream.frames,(uint64_t)(stream.rate*60));
   while(frames_read<limit){size_t count=(size_t)std::min<uint64_t>(256,limit-frames_read);if(!stream.read(stream.user,frames_read,count,pcm))break;hash.add(pcm,count*2*sizeof(float));frames_read+=count;}
   hash.end(digest);stream.close(stream.user);
  }
  const char*name=FIELD(reinterpret_cast<void*>(sample[0]),0xa8,const char*);
  std::fprintf(stderr,"MIC_PAD id=%u state=%u pad=%u sample=%p name=\"%.160s\" frames=%llu read=%llu stereo_f32_sha256=%s\n",id,probe_state.load(),pad,reinterpret_cast<void*>(sample[0]),name?name:"",(unsigned long long)stream.frames,(unsigned long long)frames_read,digest);
 }else std::fprintf(stderr,"MIC_PAD id=%u state=%u pad=%u empty=1\n",id,probe_state.load(),pad);
 pad_assignment_release(image,sample);
 /* The stock recorder uses this fixed Temp project directory. Include
  * zero-size/preallocated next-take files: they must not be mistaken for
  * completed takes by analysis. Read-only and bounded to256 files. */
 const char*dir="/data/TempProjects/Temp-Project_[ProjectData]";DIR*d=opendir(dir);if(!d)return;
 unsigned count=0;dirent*e;
 while((e=readdir(d))&&count<256){size_t n=std::strlen(e->d_name);if(n<4||std::strcmp(e->d_name+n-4,".wav"))continue;
  char path[1024];if(std::snprintf(path,sizeof path,"%s/%s",dir,e->d_name)>=(int)sizeof path)continue;
  int fd=open(path,O_RDONLY|O_NOFOLLOW|O_CLOEXEC);if(fd<0)continue;struct stat before{},after{};
  if(fstat(fd,&before)||!S_ISREG(before.st_mode)||before.st_size>64*1024*1024){close(fd);continue;}
  ProbeHash hash;char buf[8192],digest[65];ssize_t got;uint64_t bytes=0;
  while((got=read(fd,buf,sizeof buf))>0){hash.add(buf,(size_t)got);bytes+=(uint64_t)got;}
  int st=fstat(fd,&after);close(fd);hash.end(digest);
  std::fprintf(stderr,"MIC_FILE id=%u pad=%u path=\"%s\" bytes=%llu stable=%u sha256=%s\n",id,pad,path,(unsigned long long)bytes,got==0&&!st&&before.st_size==after.st_size&&before.st_mtime==after.st_mtime,digest);++count;
 }
 closedir(d);
}
}
