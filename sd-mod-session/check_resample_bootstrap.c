/* Execute the real preload constructor against the complete frozen clone
 * patch bundle. The native application main/audio driver is never started. */
#define main unused_fx_main
#include "check_fx_native.c"
#undef main
#include "src/resample_fast_native.h"
#include "build/phone_hooks.h"
#include <link.h>
/* Exercise the real BL at1500d44. Only its continuation is a test adapter:
 * return through x19 to this harness instead of entering the pad UI. */
extern int copy_from_loader(const uint64_t*,const uint64_t*,uintptr_t);
__asm__(".text\n.global copy_from_loader\n.type copy_from_loader,%function\n"
        "copy_from_loader:\nbti c\npaciasp\nstp x19,x30,[sp,#-16]!\n"
        "adr x19,1f\nbr x2\n1:\nbti j\nldp x19,x30,[sp],#16\nautiasp\nret\n"
        ".size copy_from_loader,.-copy_from_loader\n");
static void check_sample_copy(void){
 phase="real sample copy bridge";
 char dir[]="/tmp/am-native-copy-XXXXXX";assert(mkdtemp(dir));
 char src[256],dst[256],bad[256],missing[256];
 snprintf(src,sizeof(src),"%s/source.wav",dir);snprintf(dst,sizeof(dst),"%s/destination.wav",dir);
 snprintf(bad,sizeof(bad),"%s/no-parent/destination.wav",dir);snprintf(missing,sizeof(missing),"%s/missing.wav",dir);
 FILE*f=fopen(src,"wb");assert(f);assert(fwrite("WAVE-TRACE",1,10,f)==10);assert(!fclose(f));
 uint64_t source=0,destination=0,bad_target=0,missing_source=0;
 FN(0xf535c0,void(*)(uint64_t*,const char*))(&source,src);
 FN(0xf535c0,void(*)(uint64_t*,const char*))(&destination,dst);
 FN(0xf535c0,void(*)(uint64_t*,const char*))(&bad_target,bad);
 FN(0xf535c0,void(*)(uint64_t*,const char*))(&missing_source,missing);
 FILE*log=tmpfile();assert(log);fflush(stderr);int saved=dup(2);assert(saved>=0&&dup2(fileno(log),2)>=0);
 assert(FN(0xf5cc80,int(*)(const uint64_t*,const uint64_t*))(&source,&destination)==1);
 fflush(stderr);assert(ftell(log)==0); /* Ordinary copies do not produce traces. */
 uint32_t previous=*(uint32_t*)(base+0x1500d48);
 *(uint32_t*)(base+0x1500d48)=0xd61f0260; /* BR x19, fixture continuation. */
 __builtin___clear_cache((char*)(base+0x1500d48),(char*)(base+0x1500d4c));
 assert(copy_from_loader(&source,&destination,base+0x1500d44)==1);
 assert(copy_from_loader(&source,&bad_target,base+0x1500d44)==0);
 assert(copy_from_loader(&missing_source,&destination,base+0x1500d44)==0);
 *(uint32_t*)(base+0x1500d48)=previous;
 __builtin___clear_cache((char*)(base+0x1500d48),(char*)(base+0x1500d4c));
 fflush(stderr);assert(dup2(saved,2)>=0);close(saved);rewind(log);
 char line[4096];unsigned lines=0,ok=0,failed=0;
 while(fgets(line,sizeof(line),log)){assert(strstr(line,"SAMPLE_LOAD_COPY "));++lines;if(strstr(line,"result=1"))++ok;else ++failed;fputs(line,stdout);}
 assert(lines==3&&ok==1&&failed==2);fclose(log);
 f=fopen(dst,"rb");assert(f);char content[11]={0};assert(fread(content,1,10,f)==10&&!strcmp(content,"WAVE-TRACE"));assert(!fclose(f));
 FN(0xf0a290,void(*)(uint64_t*))(&source);FN(0xf0a290,void(*)(uint64_t*))(&destination);
 FN(0xf0a290,void(*)(uint64_t*))(&bad_target);FN(0xf0a290,void(*)(uint64_t*))(&missing_source);
 assert(!unlink(src)&&!unlink(dst)&&!rmdir(dir));
 puts("PASS actual firmware copy bridge and original loader BL preserve file bytes/success/failure and log correct paths; missing parent/source covered; other callers silent");
}
static int pretend_main;
int dl_iterate_phdr(int(*callback)(struct dl_phdr_info*,size_t,void*),void*data){
 if(!pretend_main){int(*real)(int(*)(struct dl_phdr_info*,size_t,void*),void*)=dlsym(RTLD_NEXT,"dl_iterate_phdr");assert(real);return real(callback,data);}
 ElfW(Phdr) segment={.p_type=PT_LOAD,.p_vaddr=0,.p_memsz=0x7500000};
 struct dl_phdr_info info={.dlpi_addr=base,.dlpi_name="",.dlpi_phdr=&segment,.dlpi_phnum=1};
 return callback(&info,sizeof(info),data);
}
int main(int argc,char**argv){
 assert(argc==4||argc==5);struct sigaction sa={.sa_sigaction=fault,.sa_flags=SA_SIGINFO};sigaction(SIGSEGV,&sa,NULL);sigaction(SIGABRT,&sa,NULL);load(argv[1]);phase="real preload bootstrap";setbuf(stdout,NULL);
 FILE*f=fopen(argv[3],"rb");assert(f);uint64_t offset,size;
 while(fread(&offset,8,1,f)==1){assert(fread(&size,8,1,f)==1&&offset<0x7500000&&size<=65536&&offset+size<=0x7500000);assert(fread((void*)(base+offset),1,size,f)==size);}assert(feof(f));fclose(f);
 __builtin___clear_cache((char*)base,(char*)(base+0x7500000));pretend_main=1;
 void*library=dlopen(argv[2],RTLD_NOW|RTLD_LOCAL);if(!library){fprintf(stderr,"%s\n",dlerror());return 1;}
 int(*ready)(uintptr_t)=dlsym(library,"resample_fast_native_ready");assert(ready&&ready(base));
 for(unsigned i=0;i<H_COUNT;++i){uintptr_t*slot=(uintptr_t*)(base+PHONE_TABLE_VA+64+i*16);assert(slot[0]==phone_hooks[i].entry&&slot[1]);}
 uint32_t insn=*(uint32_t*)(base+0x211a234);assert((insn&0xfc000000u)==0x14000000u&&insn!=0x1530904d);
 if(argc==5)check_sample_copy();
 printf("PASS real full preload constructor over all frozen clone patches installs all %u callbacks and fast renderer before native main/audio startup; Render readiness available without live instruction writes. No native app/audio main executed.\n",(unsigned)H_COUNT);return 0;
}
