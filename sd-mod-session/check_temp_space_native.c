#define main unused_fx_main
#include "check_fx_native.c"
#undef main
#include <sys/stat.h>
static unsigned errors;
static void space_error(void*a,void*b){(void)a;(void)b;++errors;}
int main(int argc,char**argv){
 assert(argc==5);load(argv[1]);phase="native copy space check";
 struct stat st;assert(!stat(argv[2],&st));int expected=atoi(argv[4]);
 uint64_t source=0,destination=0;
 FN(0xf535c0,void(*)(uint64_t*,const char*))(&source,argv[2]);
 FN(0xf535c0,void(*)(uint64_t*,const char*))(&destination,argv[3]);
 /* Keep the real space query/reserve comparison. Only its UI popup is an
  * adapter; no application UI or audio driver is initialized in this test. */
 uint32_t jump[]={0x58000050,0xd61f0200};void*handler=space_error;
 memcpy((void*)(base+0x1bfd7b0),jump,8);memcpy((void*)(base+0x1bfd7b8),&handler,8);
 __builtin___clear_cache((char*)(base+0x1bfd7b0),(char*)(base+0x1bfd7c0));
 unsigned char manager[0x180]={0};*(uint64_t*)(manager+0x130)=419430400;
 int64_t free_bytes=FN(0xf4c170,int64_t(*)(const uint64_t*))(&destination);
 int allowed=FN(0x1db36f0,int(*)(void*,int64_t,const uint64_t*,int))(manager,st.st_size,&destination,0);
 assert(allowed==expected&&errors==(unsigned)!expected);
 if(allowed){assert(FN(0xf5cc80,int(*)(const uint64_t*,const uint64_t*))(&source,&destination));struct stat target;assert(!stat(argv[3],&target)&&target.st_size==st.st_size);}
 printf("PASS native 400MiB reserve unchanged; required=%lld free=%lld allowed=%d; actual native copy=%s\n",(long long)st.st_size,(long long)free_bytes,allowed,allowed?"completed":"not attempted");
 FN(0xf0a290,void(*)(uint64_t*))(&source);FN(0xf0a290,void(*)(uint64_t*))(&destination);
 return 0;
}
