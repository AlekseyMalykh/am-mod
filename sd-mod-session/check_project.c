#define _GNU_SOURCE
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#define P(p,o) (*(void **)((unsigned char *)(p)+(o)))
static unsigned char *base;
static int row,command,clears,new_project,refreshes;
static uintptr_t captured_project;
static void request_project(void*p){captured_project=(uintptr_t)p;}
static uintptr_t guard=0x12345678;
static void *alloc(size_t n){void *p=malloc(n);assert(p);return p;}
static void refresh(void *p){assert(p);refreshes++;}
static int selected(void *p,int fallback){assert(p&&fallback==0);return row;}
static void dispatch(void *p,int cmd){assert(p);command=cmd;}
static void clear(void *p,int val,int cmd){assert(p&&val==0&&cmd==0x30f6);clears++;}
static void newproj(void *p){assert(p);new_project++;}
static void stub(size_t off,void *fn){uint32_t op[]={0x58000050,0xd61f0200};memcpy(base+off,op,8);memcpy(base+off+8,&fn,8);}
static void load(FILE *f,size_t o,size_t va,size_t n){assert(fseek(f,o,SEEK_SET)==0);assert(fread(base+va,1,n,f)==n);}
static uint32_t branch(size_t a,size_t b){return 0x14000000|(((b-a)/4)&0x3ffffff);}
static void initrow(unsigned char *r,const char *title){
 memset(r,0,128);*(int *)r=-1;
 P(r,0x68)=r+0x58;P(r,0x70)=r+0x58;
 unsigned char *n=calloc(1,48);assert(n);
 P(r,0x60)=P(r,0x68)=P(r,0x70)=n;*(size_t *)(r+0x78)=1;
 P(n,8)=r+0x58;
 void (*str)(void *,const char *)=(void *)(base+0xf535c0);
 str(n+0x20,"text");str(n+0x28,title);
}
static const char *title(unsigned char *r){return P(P(r,0x60),0x28);}
static void run(uintptr_t address,const char *stock,const char *payload){
 refreshes=0;
 base=mmap((void *)address,0x7400000,7,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);assert(base==(void *)address);
 FILE *f=fopen(stock,"rb");assert(f);load(f,0,0,0x6d3aff8);fclose(f);
 f=fopen(payload,"rb");assert(f);size_t n=fread(base+0x6d3b400,1,0xc00,f);
#ifdef TEST_PHONE
 assert(n>280&&n<512);
#else
 assert(n==280);
#endif
fclose(f);
 // Actual firmware string constructors/refcounts, vector growth/move, map deep
 // copy and destructors execute. Replace only allocation and external UI work.
 stub(0xb51418,alloc);stub(0xb51408,free);stub(0xb51c48,alloc);stub(0xb51c58,free);stub(0xb51428,memcpy);
 stub(0x117a270,refresh);stub(0x11227b0,selected);stub(0x1c0e5c0,dispatch);stub(0x1c0d990,clear);stub(0x1643060,newproj);
 P(base,0x7164398)=&guard;

#ifdef TEST_PHONE
 stub(TEST_MUSIC_BRIDGE,request_project);uint32_t b=branch(0x1643908,TEST_PROJECT_DISPATCH);
#else
 uint32_t b=branch(0x1643908,0x6d3b488);
#endif
memcpy(base+0x1643908,&b,4);
 __builtin___clear_cache((char *)base,(char *)base+0x7400000);
 void (*append)(void *,void *)=(void *)(base+0x3b71a60);
 void (*append_mod)(void *,void *)=(void *)(base+0x6d3b400);
 void (*destroy)(void *)=(void *)(base+0x15b90f0);
 void (*click)(void *)=(void *)(base+0x16438a0);

#ifdef TEST_PHONE
#ifdef TEST_LIGHTING_MENU
 const char *labels[]={"Load Project","Save Project","New Project","SD Card Access","Key / Scale","Pad Lighting","Mod Off"};const int rows=7;
#else
 const char *labels[]={"Load Project","Save Project","New Project","SD Card Access","Key / Scale","Mod Off"};const int rows=6;
#endif
#else
 const char *labels[]={"Load Project","Save Project","New Project","SD Card Access","Mod Off"};const int rows=5;
#endif

 for(int k=0;k<128;k++){
  unsigned char model[0x100]={0},list[0x100]={0},sources[4][128];P(list,0xc0)=model;P(list,0xb8)=model;
  for(int i=0;i<4;i++)initrow(sources[i],labels[i]);
  for(int i=0;i<3;i++)append(list,sources[i]);
  append_mod(list,sources[3]);
  unsigned char *begin=P(model,8),*end=P(model,16);assert(end-begin==128*rows);
  for(int i=0;i<rows;i++)assert(!strcmp(title(begin+128*i),labels[i]));
  assert(!strcmp(title(sources[3]),"SD Card Access"));
  assert(P(begin+3*128,0x60)!=P(begin+4*128,0x60)); // Independent map nodes.
  assert(refreshes==rows*(k+1));
#ifdef TEST_LIGHTING_MENU
  ((void(*)(void*,unsigned))(base+0x3b71d50))(list,6);
  assert(*(uint64_t*)(list+0xc8)==6&&end-begin==7*128);
#endif
  for(int i=0;i<rows;i++)destroy(begin+128*i);
  free(begin);
  for(int i=0;i<4;i++)destroy(sources[i]);
 }
 unsigned char screen[0x700]={0};P(screen,0x4c8)=screen;P(screen,0x108)=screen;void *context=screen;
 for(row=0;row<4;row++){
  command=clears=new_project=0;click(&context);
  if(row==0)assert(command==0x20a6&&clears==0);
  if(row==1)assert(command==0x30f7&&clears==1);
  if(row==2)assert(new_project==1&&command==0);
  if(row==3)assert(command==0x30f2&&clears==0);

#ifndef TEST_LIGHTING_MENU
 assert(access("/run/mpc-trial-app/mod-off.request",F_OK)!=0);
#endif

 }

#ifdef TEST_PHONE
 row=4;captured_project=0;click(&context);assert(captured_project==(uintptr_t)screen);
#ifdef TEST_LIGHTING_MENU
 row=5;captured_project=0;click(&context);assert(captured_project==((uintptr_t)screen|2));row=6;
#else
 row=5;
#endif
 captured_project=0;click(&context);assert(captured_project==((uintptr_t)screen|1));
#ifndef TEST_LIGHTING_MENU
 assert(access("/run/mpc-trial-app/mod-off.request",F_OK)!=0);
#endif

#else
 row=4;command=clears=new_project=0;click(&context);assert(!command&&!clears&&!new_project);

 f=fopen("/run/mpc-trial-app/mod-off.request","rb");assert(f);assert(fgetc(f)=='1'&&fgetc(f)==EOF);fclose(f);
 struct stat st;assert(stat("/run/mpc-trial-app/mod-off.request",&st)==0&&(st.st_mode&0777)==0600);
 assert(unlink("/run/mpc-trial-app/mod-off.request")==0);
 f=fopen("/run/mpc-trial-app/sentinel","wb");assert(f);fputs("UNCHANGED",f);fclose(f);
 assert(symlink("/run/mpc-trial-app/sentinel","/run/mpc-trial-app/mod-off.request")==0);
 click(&context);f=fopen("/run/mpc-trial-app/sentinel","rb");assert(f);char buf[16]={0};assert(fread(buf,1,16,f)==9);fclose(f);assert(!strcmp(buf,"UNCHANGED"));
 unlink("/run/mpc-trial-app/mod-off.request");unlink("/run/mpc-trial-app/sentinel");
 #endif

#ifdef TEST_PHONE
#ifdef TEST_LIGHTING_MENU
 printf("PASS native Project seven independently owned rows, Key4/Lighting5/Off6, six-row viewport, stock commands and256 lifecycle cycles across two PIE bases\n");
#else
 printf("PASS native Project six independently owned rows, Key4/Off5 tagged UI-only dispatch, stock commands retained,128 growth/destruction cycles at PIE %lx\n",(unsigned long)address);
#endif
#else
 printf("PASS Project native row ownership, 128 growth/destruction cycles, stock commands, actual RAM request syscall and no-follow at PIE %lx\n",(unsigned long)address);
#endif
 assert(munmap(base,0x7400000)==0);
}
#ifdef TEST_LIGHTING_MENU
int main(int argc,char **argv){assert(argc==3);run(0x10000000,argv[1],argv[2]);run(0x30000000,argv[1],argv[2]);return 0;}
#else
int main(int argc,char **argv){assert(argc==3);assert(mkdir("/run/mpc-trial-app",0700)==0);run(0x10000000,argv[1],argv[2]);run(0x30000000,argv[1],argv[2]);assert(rmdir("/run/mpc-trial-app")==0);return 0;}

#endif
