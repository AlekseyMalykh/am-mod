#define _GNU_SOURCE
#include "src/temp_workspace_store.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static void put(int dir,const char*name,const char*text){int fd=openat(dir,name,O_CREAT|O_WRONLY|O_TRUNC,0600);assert(fd>=0);size_t n=strlen(text);assert(write(fd,text,n)==(ssize_t)n&&!close(fd));}
static void has(int dir,const char*name,const char*text){char b[100]={0};int fd=openat(dir,name,O_RDONLY);assert(fd>=0);ssize_t n=read(fd,b,sizeof(b)-1);assert(n>=0&&!close(fd)&&!strcmp(b,text));}
int main(int argc,char**argv){
 assert(argc==2);int root=open(argv[1],O_RDONLY|O_DIRECTORY);assert(root>=0);
 assert(!mkdirat(root,"Samples",0700)&&!mkdirat(root,"Seed",0700));
 int samples=openat(root,"Samples",O_RDONLY|O_DIRECTORY),seed=openat(root,"Seed",O_RDONLY|O_DIRECTORY);assert(samples>=0&&seed>=0);
 put(seed,"Temp-Project.xpj","original project");assert(!mkdirat(seed,"Temp-Project_[ProjectData]",0700));
 int data=openat(seed,"Temp-Project_[ProjectData]",O_RDONLY|O_DIRECTORY);assert(data>=0);put(data,"Sample 001.wav","original audio");close(data);
 int ready=temp_workspace_store(samples,seed);assert(ready>=0);has(ready,"Temp-Project.xpj","original project");
 data=openat(ready,"Temp-Project_[ProjectData]",O_RDONLY|O_DIRECTORY);assert(data>=0);has(data,"Sample 001.wav","original audio");close(data);
 put(ready,"Temp-Project.xpj","modified mod project");close(ready);put(seed,"Temp-Project.xpj","new stock project");
 ready=temp_workspace_store(samples,seed);assert(ready>=0);has(ready,"Temp-Project.xpj","modified mod project");has(seed,"Temp-Project.xpj","new stock project");close(ready);
 assert(!mkdirat(root,"Fresh",0700));int fresh=openat(root,"Fresh",O_RDONLY|O_DIRECTORY);assert(fresh>=0);ready=temp_workspace_store(fresh,-1);assert(ready>=0);struct stat st;assert(fstatat(ready,"Temp-Project.xpj",&st,0)&&errno==ENOENT);close(ready);close(fresh);
 /* Symlinks and special files cannot be seeded as a completed cache. This
  * branch is skipped on FAT, whose filesystem cannot create a symlink. */
 assert(!mkdirat(root,"Rejected",0700));int rejected=openat(root,"Rejected",O_RDONLY|O_DIRECTORY);assert(rejected>=0);
 if(!symlinkat("/outside",seed,"bad-link")){
  assert(temp_workspace_store(rejected,seed)<0);
  assert(fstatat(rejected,".am-mod-workspace/temp-projects-v1",&st,0)&&errno==ENOENT);
  assert(!unlinkat(seed,"bad-link",0));
  assert(!mkdirat(root,"Linked",0700));int linked=openat(root,"Linked",O_RDONLY|O_DIRECTORY);assert(linked>=0);
  assert(!symlinkat("../Seed",linked,".am-mod-workspace"));assert(temp_workspace_store(linked,seed)<0);close(linked);
 }
 close(rejected);close(samples);close(seed);close(root);
 puts("PASS transactional first seed, nested audio/space names, no original writes, existing cache never overwritten, empty device boot and no completed cache on failed/link seed");return 0;
}
