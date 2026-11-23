#include "synth_accept_native.h"
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
static uintptr_t image;static am_synth_accept_tap tap;static am_synth_accept_end end;
typedef void(*Backend)(void*,uint64_t,int,void*,const void*);
static void accepted(void*owner,uint64_t offset,int route,void*out,const void*command,Backend original){
 void*cookie=tap(owner,offset,route,out,command);
 try{original(owner,offset,route,out,command);}catch(...){end(cookie,1);throw;}
 end(cookie,0);
}
extern "C" int am_synth_accept_native_install(uintptr_t base,am_synth_accept_tap callback,am_synth_accept_end finish){
 if(image)return image==base&&tap==callback&&end==finish;
 if(!base||!callback||!finish||sysconf(_SC_PAGESIZE)!=4096)return 0;
 uintptr_t site=base+0x1e1e350;uint32_t expected=0xd63f00c0; /* blr x6 */
 if(memcmp((void*)site,&expected,4))return 0;
 unsigned char*code=static_cast<unsigned char*>(MAP_FAILED);
 for(uintptr_t off=0x7800000;off<0x9600000;off+=0x10000){if(base+off-site>=0x8000000)break;
  code=static_cast<unsigned char*>(mmap((void*)(base+off),4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0));if(code!=MAP_FAILED)break;
 }
 if(code==MAP_FAILED)return 0;
 /* The original x0..x4 remain untouched; x5 captures the virtual target x6.
  * Tail branch preserves native BL return address and stack/PAC discipline. */
 const uint32_t words[]={0xd503245f,0xaa0603e5,0x58000050,0xd61f0200};
 memcpy(code,words,sizeof words);uintptr_t target=(uintptr_t)accepted;memcpy(code+16,&target,8);
 __builtin___clear_cache((char*)code,(char*)code+24);
 if(mprotect(code,4096,PROT_READ|PROT_EXEC)){munmap(code,4096);return 0;}
 uintptr_t page=site&~(uintptr_t)4095;
 if(mprotect((void*)page,4096,PROT_READ|PROT_WRITE)){munmap(code,4096);return 0;}
 tap=callback;end=finish;image=base;uint32_t branch=0x94000000u|((uint32_t)(((uintptr_t)code-site)/4)&0x3ffffffu);memcpy((void*)site,&branch,4);
 __builtin___clear_cache((char*)site,(char*)site+4);if(mprotect((void*)page,4096,PROT_READ|PROT_EXEC))_exit(121);
 return 1;
}
