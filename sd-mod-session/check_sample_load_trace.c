#define _GNU_SOURCE
#include "src/sample_load_trace.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static const uint64_t*want_source,*want_destination;static unsigned calls;static int answer,error;
static int original(const uint64_t*s,const uint64_t*d){assert(s==want_source&&d==want_destination);++calls;errno=error;return answer;}
int main(void){
 char folder[]="/tmp/am-load-trace-XXXXXX";assert(mkdtemp(folder));
 char source[256],destination[256];snprintf(source,sizeof(source),"%s/source.wav",folder);snprintf(destination,sizeof(destination),"%s/new.wav",folder);
 FILE*f=fopen(source,"wb");assert(f);assert(fwrite("WAVE",1,4,f)==4);fclose(f);
 uint64_t s=(uintptr_t)source,d=(uintptr_t)destination;want_source=&s;want_destination=&d;
 FILE*log=tmpfile();assert(log);int saved=dup(STDERR_FILENO);assert(saved>=0);assert(dup2(fileno(log),STDERR_FILENO)>=0);
 answer=0;error=EACCES;errno=123;assert(!sample_load_copy_trace(0x1000,0x1000+0x1500d48,&s,&d,original));assert(errno==EACCES&&calls==1);
 assert(access(destination,F_OK)&&errno==ENOENT); /* No repair/retry/output write. */
 answer=7;error=ENOSPC;assert(sample_load_copy_trace(0x1000,0x1000+0x1500d48,&s,&d,original)==7);assert(errno==ENOSPC&&calls==2);
 long length=ftell(log);answer=0;error=EIO;assert(!sample_load_copy_trace(0x1000,0x9999,&s,&d,original));assert(errno==EIO&&calls==3);assert(ftell(log)==length);
 for(unsigned i=0;i<100;++i)assert(!sample_load_copy_trace(0x1000,0x1000+0x1500d48,&s,&d,original));
 fflush(stderr);assert(dup2(saved,STDERR_FILENO)>=0);close(saved);rewind(log);
 char line[4096];unsigned failed=0,ok=0;while(fgets(line,sizeof(line),log)){assert(strstr(line,"source_bytes=4"));assert(strstr(line,"destination_stat=-1"));if(strstr(line,"result=0"))++failed;else ++ok;}
 assert(failed==32&&ok==1);fclose(log);assert(!unlink(source));assert(!rmdir(folder));
 puts("PASS copy tracing forwards exact arguments/result/errno once; metadata only; caller filtering and bounded logs; no files created or retries");return 0;
}
