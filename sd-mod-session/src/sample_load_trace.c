#define _GNU_SOURCE
#include "sample_load_trace.h"
#include <errno.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>

static atomic_uint successes,failures;
/* File::copyFileTo at f5cc80, called at1500d44 while preparing a library
 * sample. The native error at1500f24 follows if this target does not exist.
 * Other native copies are forwarded without observation. Never retry, change
 * arguments, create directories or touch project preferences here. */
int sample_load_copy_trace(uintptr_t image,uintptr_t caller,const uint64_t*source,
                           const uint64_t*destination,sample_file_copy_fn original){
 int result=original(source,destination),saved=errno;
 if(caller==image+0x1500d48){
  unsigned index=atomic_fetch_add_explicit(result?&successes:&failures,1,memory_order_relaxed);
  if(index<(result?4u:32u)){
   const char*s=source?(const char*)(uintptr_t)*source:NULL;
   const char*d=destination?(const char*)(uintptr_t)*destination:NULL;
   char parent[1024]={0};
   if(d){size_t n=strnlen(d,sizeof(parent));if(n<sizeof(parent)){memcpy(parent,d,n+1);char*slash=strrchr(parent,'/');if(slash)slash[slash==parent?1:0]=0;else parent[0]=0;}}
   struct stat a={0},b={0},p={0};struct statvfs space={0};
   errno=EINVAL;int ar=s?stat(s,&a):-1,ae=ar?errno:0;
   errno=EINVAL;int br=d?stat(d,&b):-1,be=br?errno:0;
   errno=EINVAL;int pr=parent[0]?stat(parent,&p):-1,pe=pr?errno:0;
   int sr=parent[0]?statvfs(parent,&space):-1;
   fprintf(stderr,"SAMPLE_LOAD_COPY result=%d errno_after_native=%d source=\"%.600s\" destination=\"%.600s\" source_stat=%d source_errno=%d source_bytes=%lld destination_stat=%d destination_errno=%d parent=\"%.600s\" parent_stat=%d parent_errno=%d parent_mode=%o free_bytes=%llu\n",
     result,saved,s?s:"(null)",d?d:"(null)",ar,ae,(long long)a.st_size,br,be,parent,pr,pe,
     (unsigned)(p.st_mode&07777),sr?0ull:(unsigned long long)space.f_bavail*space.f_frsize);
  }
 }
 errno=saved;return result;
}
