#define _DEFAULT_SOURCE
#include "browser_tree.h"
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#define DEPTH 16
#define PATH_SIZE 1024
static struct {DIR*dir;char path[PATH_SIZE];} stack[DEPTH];
static unsigned depth,visited,directories;
static int project_files,limited,last_error;
static struct {dev_t dev;ino_t ino;} seen[4096];
void browser_tree_close(void){while(depth)closedir(stack[--depth].dir);}
int browser_tree_start(const char*root,int projects){
 browser_tree_close();visited=0;directories=1;limited=last_error=0;project_files=projects;
 if(!root||root[0]!='/'||strlen(root)>=PATH_SIZE){last_error=EINVAL;return 0;}
 struct stat s;if(stat(root,&s)){last_error=errno;return 0;}if(!S_ISDIR(s.st_mode)){last_error=ENOTDIR;return 0;}
 DIR*d=opendir(root);if(!d){last_error=errno;return 0;}
 seen[0].dev=s.st_dev;seen[0].ino=s.st_ino;
 strcpy(stack[0].path,root);stack[0].dir=d;depth=1;return 1;
}
int browser_tree_accepts(const char*name,int projects){
 const char*p=strrchr(name,'.');if(!p)return 0;
 char ext[12];size_t n=strlen(p);if(n>=sizeof(ext))return 0;
 for(size_t i=0;i<=n;++i)ext[i]=p[i]>='A'&&p[i]<='Z'?p[i]+32:p[i];
 if(projects)return !strcmp(ext,".xpj");
 return !strcmp(ext,".wav")||!strcmp(ext,".aif")||!strcmp(ext,".aiff")||!strcmp(ext,".mp3")||!strcmp(ext,".flac")||!strcmp(ext,".ogg");
}
int browser_tree_next(char*out,size_t cap,unsigned budget){
 struct timespec before,now;clock_gettime(CLOCK_MONOTONIC,&before);
 while(depth&&budget--){
  if(++visited>200000){limited=1;browser_tree_close();break;}
  errno=0;struct dirent*e=readdir(stack[depth-1].dir);
  if(!e){if(errno)last_error=errno;closedir(stack[--depth].dir);continue;}
  if(!strcmp(e->d_name,".")||!strcmp(e->d_name,"..")||!strcmp(e->d_name,"System Volume Information"))continue;
  char p[PATH_SIZE];int n=snprintf(p,sizeof(p),"%s/%s",stack[depth-1].path,e->d_name);
  if(n<0||(size_t)n>=sizeof(p)){limited=1;continue;}
  /* Libraries can contain linked directories. Follow read-only, with an
   * inode visited set to prevent cycles and duplicate traversals. */
  struct stat s;if(stat(p,&s)){last_error=errno;continue;}
  if(S_ISDIR(s.st_mode)){
   unsigned known=0;for(unsigned i=0;i<directories;++i)if(seen[i].dev==s.st_dev&&seen[i].ino==s.st_ino){known=1;break;}if(known)continue;
   if(depth==DEPTH||directories>=4096){limited=1;continue;}
   DIR*d=opendir(p);if(d){seen[directories].dev=s.st_dev;seen[directories].ino=s.st_ino;strcpy(stack[depth].path,p);stack[depth++].dir=d;++directories;}else last_error=errno;
  }else if(S_ISREG(s.st_mode)&&browser_tree_accepts(e->d_name,project_files)){
   if((size_t)n>=cap){limited=1;continue;}memcpy(out,p,(size_t)n+1);return 1;
  }
  clock_gettime(CLOCK_MONOTONIC,&now);
  if((now.tv_sec-before.tv_sec)*1000000000L+now.tv_nsec-before.tv_nsec>=2000000L)return 0;
 }
 return depth?0:-1;
}
int browser_tree_limited(void){return limited;}
int browser_tree_error(void){return last_error;}
