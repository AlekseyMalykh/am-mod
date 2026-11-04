#define _GNU_SOURCE
#include "temp_workspace_store.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <unistd.h>
#define ROOT "/run/mpc-clone-trial-v1/storage-meta/"
static void die(const char*why){fprintf(stderr,"TEMP_SD_PREPARE_FAILED %s errno=%d\n",why,errno);exit(1);}
static void meta(const char*name,char*out,size_t size){
 char path[200];snprintf(path,sizeof(path),ROOT "%s",name);
 int fd=open(path,O_RDONLY|O_NOFOLLOW|O_CLOEXEC);struct stat st;struct statfs fs;
 if(fd<0||fstat(fd,&st)||fstatfs(fd,&fs)||!S_ISREG(st.st_mode)||st.st_uid||fs.f_type!=0x01021994||st.st_size<=0||(size_t)st.st_size>=size)die("RAM metadata");
 ssize_t n=read(fd,out,size-1);if(n!=st.st_size||close(fd))die("metadata read");
 if(n&&out[n-1]=='\n')--n;
 out[n]=0;
}
static void identity(int fd,const char*name){
 char expected[100],actual[100];struct stat st;meta(name,expected,sizeof(expected));if(fstat(fd,&st))die("fstat");
 snprintf(actual,sizeof(actual),"%llu:%llu",(unsigned long long)st.st_dev,(unsigned long long)st.st_ino);
 if(strcmp(expected,actual)){fprintf(stderr,"TEMP_SD_IDENTITY name=%s expected=%s actual=%s\n",name,expected,actual);die("identity changed");}
}
int main(int argc,char**argv){
 (void)argv;if(argc!=1||geteuid())die("root/no arguments required");
 alarm(90); /* Finish or fail before the independent startup watchdog. */
 char save[PATH_MAX],resolved[PATH_MAX],fs_name[20];meta("save-path",save,sizeof(save));meta("sd-filesystem",fs_name,sizeof(fs_name));
 long magic=!strcmp(fs_name,"exfat")?0x2011bab0:!strcmp(fs_name,"vfat")?0x4d44:0;
 const char*slash=!strncmp(save,"/media/",7)?strchr(save+7,'/'):NULL;
 if(!magic||!slash||slash==save+7||strcmp(slash,"/MPC-Sample"))die("save path");
 for(const char*p=save+7;p<slash;++p)if(!((*p>='A'&&*p<='Z')||(*p>='a'&&*p<='z')||(*p>='0'&&*p<='9')||*p=='_'||*p=='-'||*p=='.'))die("path characters");
 if(!realpath(save,resolved)||strcmp(save,resolved))die("canonical save path");
 int root=open(save,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);if(root<0)die("save root");identity(root,"save-identity");
 int samples=openat(root,"Samples",O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);if(samples<0)die("samples");identity(samples,"save-Samples-identity");
 struct statfs fs;if(fstatfs(samples,&fs)||fs.f_type!=magic)die("SD filesystem");
 int original=open("/data",O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);if(original<0)die("original data");identity(original,"original-data-identity");
 int seed=openat(original,"TempProjects",O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);if(seed<0&&errno!=ENOENT)die("native seed directory");
 int result=temp_workspace_store(samples,seed);if(result<0)die("prepare cache");
 struct stat st;if(fstat(result,&st)||fstatfs(result,&fs)||fs.f_type!=magic)die("cache filesystem");
 char expected[PATH_MAX];int n=snprintf(expected,sizeof(expected),"%s/Samples/.am-mod-workspace/temp-projects-v1",save);
 if(n<0||n>=(int)sizeof(expected)||!realpath(expected,resolved)||strcmp(expected,resolved))die("cache canonical path");
 printf("%s\n",resolved);
 close(result);if(seed>=0)close(seed);close(original);close(samples);close(root);return 0;
}
