#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "src/splash_branding.h"
#include "src/am_mod_splash.inc"
int main(int argc,char**argv){
 assert(argc==2&&strlen(AM_MOD_VERSION)<16&&strlen(AM_MOD_REPO_LABEL)<40);
 unsigned pitch=1344;uint32_t*memory=malloc((size_t)pitch*240+32);assert(memory);
 memset(memory,0xcd,(size_t)pitch*240+32);unsigned n=0;
 for(unsigned i=0;i<sizeof(am_splash_runs)/sizeof(*am_splash_runs);++i)
  for(unsigned j=0;j<am_splash_runs[i].count;++j,++n)((uint32_t*)((char*)memory+(n/320)*pitch))[n%320]=am_splash_runs[i].argb;
 assert(n==320*240);uint32_t original=memory[0];am_splash_branding(memory,pitch);
 unsigned version=0,url=0;
 for(unsigned y=0;y<240;++y){uint32_t*row=(void*)((char*)memory+y*pitch);
  for(unsigned x=0;x<320;++x){if(y>=201&&y<209)version+=row[x]==0xffd4d8dfu;if(y>=222&&y<230)url+=row[x]==0xffa8b3c2u;}
  for(unsigned x=1280;x<pitch;++x)assert(((unsigned char*)row)[x]==0xcd);
 }
 assert(memory[0]==original&&version>20&&url>100);
 for(unsigned i=0;i<32;++i)assert(((unsigned char*)memory)[pitch*240+i]==0xcd);
 FILE*f=fopen(argv[1],"wb");assert(f);fprintf(f,"P6\n320 240\n255\n");
 for(unsigned y=0;y<240;++y)for(unsigned x=0;x<320;++x){uint32_t rgb=((uint32_t*)((char*)memory+y*pitch))[x];unsigned char p[]={rgb>>16,rgb>>8,rgb};assert(fwrite(p,1,3,f)==3);}
 assert(!fclose(f));free(memory);puts("PASS shared version and repository render in separate lower bands; logo and padded rows/guards preserved");return 0;
}
