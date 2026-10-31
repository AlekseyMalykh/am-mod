/* Execute the real preload constructor against the complete frozen clone
 * patch bundle. The native application main/audio driver is never started. */
#define main unused_fx_main
#include "check_fx_native.c"
#undef main
#include "src/resample_fast_native.h"
#include "build/phone_hooks.h"
#include <link.h>
static int pretend_main;
int dl_iterate_phdr(int(*callback)(struct dl_phdr_info*,size_t,void*),void*data){
 if(!pretend_main){int(*real)(int(*)(struct dl_phdr_info*,size_t,void*),void*)=dlsym(RTLD_NEXT,"dl_iterate_phdr");assert(real);return real(callback,data);}
 ElfW(Phdr) segment={.p_type=PT_LOAD,.p_vaddr=0,.p_memsz=0x7500000};
 struct dl_phdr_info info={.dlpi_addr=base,.dlpi_name="",.dlpi_phdr=&segment,.dlpi_phnum=1};
 return callback(&info,sizeof(info),data);
}
int main(int argc,char**argv){
 assert(argc==4);struct sigaction sa={.sa_sigaction=fault,.sa_flags=SA_SIGINFO};sigaction(SIGSEGV,&sa,NULL);sigaction(SIGABRT,&sa,NULL);load(argv[1]);phase="real preload bootstrap";setbuf(stdout,NULL);
 FILE*f=fopen(argv[3],"rb");assert(f);uint64_t offset,size;
 while(fread(&offset,8,1,f)==1){assert(fread(&size,8,1,f)==1&&offset<0x7500000&&size<=65536&&offset+size<=0x7500000);assert(fread((void*)(base+offset),1,size,f)==size);}assert(feof(f));fclose(f);
 __builtin___clear_cache((char*)base,(char*)(base+0x7500000));pretend_main=1;
 void*library=dlopen(argv[2],RTLD_NOW|RTLD_LOCAL);if(!library){fprintf(stderr,"%s\n",dlerror());return 1;}
 int(*ready)(uintptr_t)=dlsym(library,"resample_fast_native_ready");assert(ready&&ready(base));
 for(unsigned i=0;i<H_COUNT;++i){uintptr_t*slot=(uintptr_t*)(base+PHONE_TABLE_VA+64+i*16);assert(slot[0]==phone_hooks[i].entry&&slot[1]);}
 uint32_t insn=*(uint32_t*)(base+0x211a234);assert((insn&0xfc000000u)==0x14000000u&&insn!=0x1530904d);
 puts("PASS real full preload constructor over all frozen clone patches installs all50 callbacks and fast renderer before native main/audio startup; Render readiness available without live instruction writes. No native app/audio main executed.");return 0;
}
