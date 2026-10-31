#ifndef MPC_RESAMPLE_OUTPUT_FILE_H
#define MPC_RESAMPLE_OUTPUT_FILE_H
#include "resample_file_probe.h"
#include <string>
#include <vector>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
/* Own one CLOSED native stem through completion, even if native cleanup unlinks
 * its name. Only the exact job's sibling -Track-...wav may replace its empty
 * requested WAV. The selected pad is still assigned by the original callback. */
class ResampleOutputFile {
 int fd=-1;struct stat identity{};std::string requested;
 static bool sane(const std::string&p){return p.size()<4096&&!p.empty()&&p[0]=='/'&&p.find("/../")==std::string::npos&&p.find("/./")==std::string::npos;}
 static ResampleFileProbe probe_fd(int source){
  int copy=dup(source);if(copy<0)return ResampleFileProbe{};
  if(lseek(copy,0,SEEK_SET)<0){close(copy);return ResampleFileProbe{};}
  FILE*f=fdopen(copy,"rb");if(!f){close(copy);return ResampleFileProbe{};}
  auto p=resample_file_probe_stream(f);fclose(f);return p;
 }
public:
 ResampleOutputFile()=default;ResampleOutputFile(const ResampleOutputFile&)=delete;
 ResampleOutputFile&operator=(const ResampleOutputFile&)=delete;
 ~ResampleOutputFile(){reset();}
 void reset(){if(fd>=0)close(fd);fd=-1;requested.clear();}
 bool capture(const std::string&want,const std::string&actual,unsigned rate,uint64_t minimum){
  reset();if(!sane(want)||!sane(actual)||want.size()<4||want.compare(want.size()-4,4,".wav")||actual.size()<4||actual.compare(actual.size()-4,4,".wav"))return false;
  std::string prefix=want.substr(0,want.size()-4)+"-Track-";
  if(actual!=want&&(actual.compare(0,prefix.size(),prefix)||actual.find('/',prefix.size())!=std::string::npos))return false;
  fd=open(actual.c_str(),O_RDONLY|O_CLOEXEC|O_NOFOLLOW);
  if(fd<0)return false;
  auto p=probe_fd(fd);
  if(fstat(fd,&identity)||!S_ISREG(identity.st_mode)||identity.st_size<=0||(uint64_t)identity.st_size>UINT32_MAX||!p.valid||p.peak==0||p.rate!=rate||!p.channels||!p.bits||(uint64_t)p.bytes/(p.channels*(p.bits/8))<minimum){reset();return false;}
  requested=want;return true;
 }
 bool pending()const{return fd>=0;}
 bool commit(const std::string&want){
  if(fd<0||want!=requested)return false;
  struct End{ResampleOutputFile*self;~End(){self->reset();}}end{this};
  struct stat now{},dst{};
  if(fstat(fd,&now)||now.st_dev!=identity.st_dev||now.st_ino!=identity.st_ino||now.st_size!=identity.st_size)return false;
  /* Never follow a new symlink or replace another kind of filesystem node. */
  if(lstat(want.c_str(),&dst)||!S_ISREG(dst.st_mode)||dst.st_nlink!=1)return false;
  auto existing=resample_file_probe(want.c_str());
  if(existing.valid&&existing.peak>0)return true;
  auto slash=want.find_last_of('/');std::string pattern=want.substr(0,slash+1)+".mpc-resample-XXXXXX";
  std::vector<char> name(pattern.begin(),pattern.end());name.push_back(0);int out=mkstemp(name.data());if(out<0)return false;
  struct Temp{int fd;const char*name;~Temp(){if(fd>=0)close(fd);unlink(name);}}temp{out,name.data()};
  if(lseek(fd,0,SEEK_SET)<0)return false;
  uint64_t left=(uint64_t)identity.st_size;char block[65536];
  while(left){size_t n=left<sizeof(block)?(size_t)left:sizeof(block);ssize_t got;
   do{got=read(fd,block,n);}while(got<0&&errno==EINTR);if(got<=0)return false;
   size_t at=0;while(at<(size_t)got){ssize_t wrote;do{wrote=write(out,block+at,(size_t)got-at);}while(wrote<0&&errno==EINTR);if(wrote<=0)return false;at+=(size_t)wrote;}left-=(uint64_t)got;
  }
  if(fsync(out))return false;
  if(close(out)){temp.fd=-1;return false;}temp.fd=-1;
  auto p=resample_file_probe(name.data());
  if(!p.valid||p.peak==0||fstat(fd,&now)||now.st_size!=identity.st_size)return false;
  if(lstat(want.c_str(),&now)||now.st_dev!=dst.st_dev||now.st_ino!=dst.st_ino||now.st_size!=dst.st_size||!S_ISREG(now.st_mode))return false;
  return rename(name.data(),want.c_str())==0;
 }
};
#endif
