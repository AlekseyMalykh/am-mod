/* Execute pinned native Scaled/Pickup/Jump processor; GUI/parameter storage
 * are fixtures. The math and Scaled state updates come from the stock ELF. */
#define _GNU_SOURCE
#include <assert.h>
#include <dlfcn.h>
#include <elf.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ucontext.h>
#include <sys/mman.h>
#include <unistd.h>
#include "src/phone_events.h"
#ifdef CHECK_SCALED_ENDPOINTS
#include "src/takeover_endpoints.h"
#endif
static uintptr_t base;
#define FIELD(p,o,t) (*(t*)((char*)(p)+(o)))
#define FN(o,t) ((t)(base+(o)))
static uintptr_t guard=0x123456;
static void stub(uintptr_t off,void*fn){uint32_t w[]={0x58000050,0xd61f0200};memcpy((void*)(base+off),w,8);memcpy((void*)(base+off+8),&fn,8);}
static void fault_signal(int sig,siginfo_t*i,void*ctx){ucontext_t*u=ctx;fprintf(stderr,"FAIL workflow %d native=%lx addr=%p lr=%lx x20=%lx x21=%lx caller=%lx saved21=%lx\n",sig,(unsigned long)(u->uc_mcontext.pc-base),i->si_addr,(unsigned long)(u->uc_mcontext.regs[30]-base),(unsigned long)u->uc_mcontext.regs[20],(unsigned long)u->uc_mcontext.regs[21],(unsigned long)(((uintptr_t*)u->uc_mcontext.regs[29])[1]-base),(unsigned long)((uintptr_t*)u->uc_mcontext.regs[29])[4]);_exit(1);}
static void load_elf(const char *path,uintptr_t address)
{
    FILE *f=fopen(path,"rb");assert(f);Elf64_Ehdr h;assert(fread(&h,1,sizeof(h),f)==sizeof(h));
    unsigned char *memory=mmap((void*)address,0x7500000,7,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
    assert(memory==(void*)address);base=address;
    Elf64_Phdr ph[16];assert(h.e_phnum<=16);fseek(f,(long)h.e_phoff,SEEK_SET);assert(fread(ph,sizeof(*ph),h.e_phnum,f)==h.e_phnum);
    Elf64_Dyn *dynamic=NULL;
    for(unsigned i=0;i<h.e_phnum;++i) if(ph[i].p_type==PT_LOAD) {
        assert(ph[i].p_vaddr+ph[i].p_memsz<0x7500000);fseek(f,(long)ph[i].p_offset,SEEK_SET);
        assert(fread(memory+ph[i].p_vaddr,1,ph[i].p_filesz,f)==ph[i].p_filesz);
    }
    for(unsigned i=0;i<h.e_phnum;++i) if(ph[i].p_type==PT_DYNAMIC) dynamic=(void*)(memory+ph[i].p_vaddr);
    assert(dynamic);uintptr_t rela=0,relasz=0,jmp=0,jmpsz=0,str=0,sym=0;
    for(Elf64_Dyn *d=dynamic;d->d_tag;++d) switch(d->d_tag) {
        case DT_RELA:rela=d->d_un.d_ptr;break;case DT_RELASZ:relasz=d->d_un.d_val;break;
        case DT_JMPREL:jmp=d->d_un.d_ptr;break;case DT_PLTRELSZ:jmpsz=d->d_un.d_val;break;
        case DT_STRTAB:str=d->d_un.d_ptr;break;case DT_SYMTAB:sym=d->d_un.d_ptr;break;
    }
    uintptr_t starts[2]={rela,jmp},sizes[2]={relasz,jmpsz};
    for(unsigned k=0;k<2;++k) for(size_t i=0;i<sizes[k]/sizeof(Elf64_Rela);++i) {
        Elf64_Rela *r=(void*)(memory+starts[k]+i*sizeof(*r));uintptr_t *out=(void*)(memory+r->r_offset);
        if(ELF64_R_TYPE(r->r_info)==R_AARCH64_RELATIVE) *out=base+r->r_addend;
        else {
            Elf64_Sym *s=(void*)(memory+sym+ELF64_R_SYM(r->r_info)*sizeof(*s));
            const char *name=(void*)(memory+str+s->st_name);void *fn=dlsym(RTLD_DEFAULT,name);
            if(fn) *out=(uintptr_t)fn+r->r_addend;
        }
    }
    fclose(f);FIELD((void*)base,0x7164398,void*)=&guard;
}

static float parameter=.45f;
static unsigned writes;
static int enabled=1;
static int enabled_fixture(void*p){(void)p;return enabled;}
static float value_fixture(void*p){(void)p;return parameter;}
static void set_fixture(void*p,float value){(void)p;parameter=value;++writes;}
static void boolean_fixture(void*p,int v){(void)p;(void)v;}
static void float_fixture(void*p,float v){(void)p;(void)v;}
static void timer_fixture(void*p,int a,int b){(void)p;(void)a;(void)b;}
static void noop_fixture(void*p){(void)p;}
static unsigned char binding[0x400],slider_fixture[0x400];
static float input_range[40];
static void setup(int mode){
 memset(binding,0,sizeof(binding));memset(slider_fixture,0,sizeof(slider_fixture));
 FIELD(binding,0x60,void*)=slider_fixture;FIELD(binding,0xdc,int)=mode;FIELD(binding,0x9b,unsigned char)=1;
 FIELD(binding,0x144,float)=-1;FIELD(binding,0x148,float)=-1;
 FIELD(slider_fixture,0x250,double)=0;FIELD(slider_fixture,0x258,double)=1;
 FIELD(slider_fixture,0x260,double)=0;FIELD(slider_fixture,0x268,double)=1;
 parameter=.45f;writes=0;
}
static void movement(unsigned v){FN(0x15f5690,void(*)(void*,float))(binding,(float)v/127.f);}
int main(int argc,char**argv){
 assert(argc==2);struct sigaction sa={0};sa.sa_sigaction=fault_signal;sa.sa_flags=SA_SIGINFO;sigaction(SIGSEGV,&sa,NULL);
 load_elf(argv[1],0x7100000000);
 stub(0x10f98d0,enabled_fixture);stub(0x3b7db10,value_fixture);stub(0x3b7d990,set_fixture);
 stub(0x13ae070,boolean_fixture);stub(0x13bbb20,float_fixture);stub(0xff9860,timer_fixture);stub(0xed9520,noop_fixture);
 input_range[1]=1;input_range[3]=1;FIELD((void*)base,0x71658f0,void*)=input_range;
 __builtin___clear_cache((char*)base,(char*)(base+0x7500000));
 for(int mode=0;mode<=2;++mode){setup(mode);movement(64);movement(127);printf("native mode=%d first+end parameter=%g writes=%u previous=%g scaled=%g\n",mode,parameter,writes,FIELD(binding,0x144,float),FIELD(binding,0x148,float));}
 setup(1);movement(127);printf("Scaled endpoint-only parameter=%g writes=%u\n",parameter,writes);
 const int modes[]={1,0,2};
 for(unsigned m=0;m<3;++m)for(unsigned end=0;end<=127;end+=127){int mode=modes[m];
  unsigned samples[4]={end?20u:100u,end?40u:80u,end?80u:40u,end};
  setup(mode);for(unsigned i=0;i<4;++i)movement(samples[i]);float direct=parameter;
  setup(mode);
  for(unsigned i=0;i<4;++i){struct phone_event e={0};e.kind=PHONE_CONTROLLER;e.native[8]=0x10;e.native[9]=(unsigned char)samples[i];assert(phone_events_push(&e));}
  struct phone_event e;while(phone_events_pop(&e))movement(e.native[9]);
  printf("mode=%d endpoint=%u direct=%g queued=%g writes=%u\n",mode,end,direct,parameter,writes);
  if(parameter-direct>0.000001f||direct-parameter>0.000001f){fprintf(stderr,"REPRODUCED native takeover mismatch mode=%d end=%u direct=%g queued=%g\n",mode,end,direct,parameter);return 1;}
 }
 const unsigned paths[][8]={{20,60,100,127,90,40,0,0},{100,60,20,0,40,90,127,127},{20,100,40,110,10,120,40,0}};
 for(unsigned m=0;m<3;++m)for(unsigned k=0;k<3;++k)for(unsigned path=0;path<3;++path)for(unsigned target=1;target<=9;target+=2){
  setup(modes[m]);parameter=(float)target/10;
  for(unsigned i=0;i<8;++i)movement(paths[path][i]);
  float direct=parameter;
  setup(modes[m]);parameter=(float)target/10;
  for(unsigned i=0;i<8;++i){struct phone_event e={0};e.kind=PHONE_CONTROLLER;e.native[8]=(unsigned char)(0x10+k);e.native[9]=(unsigned char)paths[path][i];assert(phone_events_push(&e));}
  struct phone_event e;while(phone_events_pop(&e))movement(e.native[9]);
  assert(parameter-direct<0.00001f&&direct-parameter<0.00001f);
 }
 puts("PASS actual native Pickup/Scaled/Jump: initial positions, both endpoints, direction changes, three knobs and five initial parameter values match direct native processing");
#ifdef CHECK_SCALED_ENDPOINTS
 float reference[3][3];
 const unsigned inside[][5]={{20,70,90,30,60},{1,126,64,10,125},{126,1,40,70,15}};
 for(int mode=0;mode<3;++mode)for(unsigned path=0;path<3;++path){setup(mode);for(unsigned i=0;i<5;++i)movement(inside[path][i]);reference[mode][path]=parameter;}
 for(unsigned end=0;end<=127;end+=127){setup(1);movement(end);assert(parameter==.45f&&!writes);}
 /* Installation is fail-closed and leaves a mismatched instruction intact. */
 uint32_t saved=FIELD((void*)base,0x15f5694,uint32_t);FIELD((void*)base,0x15f5694,uint32_t)=0xd503201f;
 assert(!takeover_endpoints_install(base)&&FIELD((void*)base,0x15f5694,uint32_t)==0xd503201f);
 FIELD((void*)base,0x15f5694,uint32_t)=saved;
 assert(takeover_endpoints_install(base)&&takeover_endpoints_install(base));
 assert(FIELD((void*)base,0x15f5690,uint32_t)==0xd503233f);
 for(unsigned initial=1;initial<10;initial+=2)for(unsigned end=0;end<=127;end+=127){
  setup(1);parameter=(float)initial/10;movement(end);assert(parameter==(end?1.f:0.f)&&writes==1);
  /* Same physical end, different parameter/selection; no movement history. */
  parameter=(float)initial/10;FIELD(binding,0x148,float)=parameter;movement(end);assert(parameter==(end?1.f:0.f));
  /* The following interior movement must retain ordinary Scaled semantics. */
  movement(end?100:20);assert(parameter>=0&&parameter<=1);
 }
 for(int mode=0;mode<3;++mode)for(unsigned path=0;path<3;++path){setup(mode);for(unsigned i=0;i<5;++i)movement(inside[path][i]);assert(parameter-reference[mode][path]<.00001f&&reference[mode][path]-parameter<.00001f);}
 /* Non-linear and quantised native ranges must also reach their limits. */
 const double ranges[][5]={{0,100,1,1,0},{-48,18,.1,.35,0},{-100,100,1,2,1}};
 for(unsigned r=0;r<3;++r)for(unsigned end=0;end<=127;end+=127){
  setup(1);
  for(unsigned field=0;field<4;++field)FIELD(slider_fixture,0x250+field*8,double)=ranges[r][field];
  FIELD(slider_fixture,0x270,unsigned char)=(unsigned char)ranges[r][4];
  movement(end);assert(parameter==(end?1.f:0.f)&&writes==1);
 }
 for(int mode=0;mode<=2;mode+=2){setup(mode);movement(127);assert(parameter==(mode==0?.45f:1.f));}
 enabled=0;setup(1);movement(127);assert(parameter==.45f&&!writes&&FIELD(binding,0x144,float)==-1);enabled=1;
 setup(1);FIELD(binding,0x9b,unsigned char)=0;stub(0x142fc50,noop_fixture);__builtin___clear_cache((char*)base,(char*)(base+0x7500000));movement(127);assert(parameter==.45f&&!writes);
 puts("PASS real native Scaled exact limits: first single event, stale value at unchanged end, subsequent interior motion; Pickup/Jump/interior/disabled/inactive unchanged; guarded BTI/PAC relay and idempotent install");
#endif
 return 0;
}
