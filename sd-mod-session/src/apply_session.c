#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <unistd.h>
#include "patches.h"
static void die(const char *s) { fprintf(stderr,"SESSION_PATCH_FAILED: %s\n",s); exit(1); }
int main(int argc,char **argv) {
 (void)argv;
 if(argc!=1) die("no arguments accepted");
 int fd=open("/run/mpc-clone-trial-v1/MPC-Sample",O_RDWR|O_NOFOLLOW|O_CLOEXEC);
 if(fd<0) die("fixed RAM path");
 struct stat st; struct statfs fs;
 if(fstat(fd,&st)||fstatfs(fd,&fs)||!S_ISREG(st.st_mode)||st.st_size!=119243440||st.st_nlink!=1||st.st_uid!=0||fs.f_type!=0x01021994) die("identity/tmpfs");
 unsigned char buf[4096];
 for(size_t i=0;i<sizeof(patches)/sizeof(patches[0]);i++) {
  const struct patch *p=&patches[i];
  size_t checked=0;
  while(checked<p->size) {
   size_t n=p->size-checked;if(n>sizeof(buf))n=sizeof(buf);
   ssize_t got=pread(fd,buf,n,p->offset+(off_t)checked);
   if(got<0&&errno==EINTR)continue;
   if(got!=(ssize_t)n||memcmp(buf,p->before+checked,n))die("original bytes");
   checked+=n;
  }
 }
 for(size_t i=0;i<sizeof(patches)/sizeof(patches[0]);i++) {
  const struct patch *p=&patches[i]; size_t left=p->size; off_t off=p->offset; const unsigned char *b=p->after;
  while(left) { ssize_t n=pwrite(fd,b,left,off); if(n<0&&errno==EINTR)continue; if(n<=0)die("pwrite"); b+=n;left-=n;off+=n; }
 }
 if(fsync(fd)||close(fd))die("finish");
 puts("SESSION_RAM_PATCH_OK");return 0;
}
