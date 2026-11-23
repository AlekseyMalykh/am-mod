/* Read-only timing probe in the disposable VM; never changes app memory. */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
int main(int argc,char**argv){
 if(argc!=7&&argc!=8)return 2;
 int trace=argc==8;
 long pid=strtol(argv[1],NULL,10);uintptr_t address=strtoull(argv[2],NULL,16);
 int64_t length=strtoll(argv[3],NULL,10),lo=strtoll(argv[4],NULL,10),hi=strtoll(argv[5],NULL,10);
 unsigned timeout=(unsigned)strtoul(argv[6],NULL,10);
 if(pid<=1||length<=0||lo<0||hi<lo||hi>=length||!timeout||timeout>60)return 2;
 char path[80];snprintf(path,sizeof path,"/proc/%ld/mem",pid);int fd=open(path,O_RDONLY);if(fd<0){perror("open");return 3;}
 struct timespec first,now,delay={0,1000000};clock_gettime(CLOCK_MONOTONIC,&first);
 int64_t previous=INT64_MIN;
 for(;;){
  int64_t tick;if(pread(fd,&tick,sizeof tick,(off_t)address)!=(ssize_t)sizeof tick){perror("pread");return 4;}
  clock_gettime(CLOCK_MONOTONIC,&now);
  if(trace&&tick!=previous){printf("TICK %" PRId64 "\n",tick);fflush(stdout);previous=tick;}
  if(!trace&&tick>=0&&tick%length>=lo&&tick%length<=hi){printf("TICK %" PRId64 "\n",tick);fflush(stdout);close(fd);return 0;}
  if(now.tv_sec-first.tv_sec>timeout){if(trace){close(fd);return 0;}fprintf(stderr,"clock window timeout, last=%" PRId64 "\n",tick);return 5;}
  nanosleep(&delay,NULL);
 }
}
