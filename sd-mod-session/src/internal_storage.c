#define _GNU_SOURCE
#include "internal_storage.h"
#include <sys/stat.h>
#include <sys/vfs.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#ifndef AM_INTERNAL_META
#define AM_INTERNAL_META "/run/mpc-trial-app/storage-meta/internal-storage"
#endif
#ifndef AM_INTERNAL_PROJECTS
#define AM_INTERNAL_PROJECTS "/data/UserContent/Projects"
#endif
#ifndef AM_INTERNAL_SAMPLES
#define AM_INTERNAL_SAMPLES "/data/UserContent/Samples/User"
#endif
static int directory(const char *path,struct stat *st){
 char resolved[PATH_MAX];
 return !lstat(path,st)&&S_ISDIR(st->st_mode)&&realpath(path,resolved)&&!strcmp(path,resolved);
}
int internal_storage_enabled(void){
 int fd=open(AM_INTERNAL_META,O_RDONLY|O_NOFOLLOW|O_CLOEXEC|O_NONBLOCK);
 if(fd<0)return 0;
 struct stat meta,a,b;struct statfs fs;char actual[192]={0},expected[192];
 ssize_t n=-1;
 if(!fstat(fd,&meta)&&S_ISREG(meta.st_mode)&&meta.st_uid==geteuid()&&meta.st_size>0&&meta.st_size<(off_t)sizeof(actual)
    &&!fstatfs(fd,&fs)&&fs.f_type==0x01021994)n=read(fd,actual,sizeof(actual)-1);
 close(fd);
 if(n<=0||n!=meta.st_size||!directory(AM_INTERNAL_PROJECTS,&a)||!directory(AM_INTERNAL_SAMPLES,&b))return 0;
 int count=snprintf(expected,sizeof expected,"AM_INTERNAL1\n%llu:%llu\n%llu:%llu\n",
  (unsigned long long)a.st_dev,(unsigned long long)a.st_ino,(unsigned long long)b.st_dev,(unsigned long long)b.st_ino);
 return count>0&&count<(int)sizeof expected&&n==count&&!memcmp(actual,expected,(size_t)count);
}
static int destination(const char *path,const char *root,int project){
 if(!path||!internal_storage_enabled())return 0;
 size_t length=strnlen(path,PATH_MAX),prefix=strlen(root);
 if(length>=PATH_MAX||length<=prefix+1||strncmp(path,root,prefix)||path[prefix]!='/')return 0;
 if(project&&(length<4||strcmp(path+length-4,".xpj")))return 0;
 const char *part=path+prefix+1;
 while(*part){const char *end=strchr(part,'/');size_t n=end?(size_t)(end-part):strlen(part);
  if(!n||(n==1&&part[0]=='.')||(n==2&&part[0]=='.'&&part[1]=='.'))return 0;
  for(size_t i=0;i<n;++i)if((unsigned char)part[i]<32||part[i]=='\\'||part[i]==':')return 0;
  if(!end)break;
  part=end+1;
 }
 char parent[PATH_MAX],resolved[PATH_MAX];memcpy(parent,path,length+1);
 char *slash=strrchr(parent,'/');if(!slash||!slash[1])return 0;*slash=0;
 if(!realpath(parent,resolved)||strcmp(parent,resolved))return 0;
 struct stat st;
 if(!lstat(path,&st))return S_ISREG(st.st_mode)&&st.st_nlink==1;
 return errno==ENOENT;
}
int internal_storage_project_path(const char *path){return destination(path,AM_INTERNAL_PROJECTS,1);}
int internal_storage_sample_path(const char *path){return destination(path,AM_INTERNAL_SAMPLES,0);}
