#define _GNU_SOURCE
#include "temp_workspace_store.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static int directory(int parent,const char*name){
 if(mkdirat(parent,name,0700)&&errno!=EEXIST)return -1;
 return openat(parent,name,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
}
static int copy_tree(int from,int to,unsigned depth,unsigned*count){
 if(depth>8){errno=ELOOP;return -1;}
 int dupfd=openat(from,".",O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);if(dupfd<0)return -1;
 DIR*dir=fdopendir(dupfd);if(!dir){close(dupfd);return -1;}
 int result=0;struct dirent*entry;
 while((errno=0,entry=readdir(dir))){
  const char*name=entry->d_name;if(!strcmp(name,".")||!strcmp(name,".."))continue;
  if(++*count>10000){errno=E2BIG;result=-1;break;}
  struct stat st;if(fstatat(from,name,&st,AT_SYMLINK_NOFOLLOW)){result=-1;break;}
  if(!S_ISREG(st.st_mode)&&!S_ISDIR(st.st_mode)){errno=EINVAL;result=-1;break;}
  int source=openat(from,name,O_RDONLY|O_NOFOLLOW|O_CLOEXEC|(S_ISDIR(st.st_mode)?O_DIRECTORY:0));
  if(source<0){result=-1;break;}
  int destination=S_ISDIR(st.st_mode)?directory(to,name):openat(to,name,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);
  if(destination<0){close(source);result=-1;break;}
  if(S_ISDIR(st.st_mode))result=copy_tree(source,destination,depth+1,count);
  else{
   char buffer[65536];ssize_t bytes;
   for(;;){
    bytes=read(source,buffer,sizeof(buffer));if(bytes<0&&errno==EINTR)continue;
    if(bytes<=0){if(bytes<0)result=-1;break;}
    for(ssize_t offset=0;offset<bytes;){ssize_t written=write(destination,buffer+offset,(size_t)(bytes-offset));
     if(written<0&&errno==EINTR)continue;
     if(written<=0){if(!written)errno=EIO;result=-1;break;}offset+=written;}
    if(result)break;
   }
  }
  int error=errno;if(!result&&fsync(destination)){result=-1;error=errno;}
  if(close(destination)&&!result){result=-1;error=errno;}close(source);errno=error;
  if(result)break;
 }
 int error=errno;if(!result&&error)result=-1;
 closedir(dir);errno=error;return result;
}
int temp_workspace_store(int samples,int seed){
 int parent=directory(samples,".am-mod-workspace");if(parent<0)return -1;
 const char*final="temp-projects-v1";
 int ready=openat(parent,final,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
 if(ready>=0){close(parent);return ready;}
 if(errno!=ENOENT){int e=errno;close(parent);errno=e;return -1;}
 char staging[80];int stage=-1;
 for(unsigned i=0;i<100;++i){snprintf(staging,sizeof(staging),".seed-%ld-%u",(long)getpid(),i);
  if(!mkdirat(parent,staging,0700)){stage=openat(parent,staging,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);break;}
  if(errno!=EEXIST)break;
 }
 if(stage<0){int e=errno;close(parent);errno=e;return -1;}
 unsigned count=0;
 if((seed>=0&&copy_tree(seed,stage,0,&count))||fsync(stage)||renameat(parent,staging,parent,final)||fsync(parent)){
  /* Partial seeds remain hidden and are never mounted or used as projects.
   * Do not delete files after a failed copy; leave them available for recovery. */
  int e=errno;close(stage);close(parent);errno=e;return -1;
 }
 close(parent);return stage;
}
