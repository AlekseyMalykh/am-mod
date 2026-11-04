#define _GNU_SOURCE
/* Executes the pinned engine in an isolated QEMU process, never on the MPC.
 * No GUI/driver constructors or native app initializers are run. */
#include <assert.h>
#include <dlfcn.h>
#include <elf.h>
#include <math.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <ucontext.h>
#include <unistd.h>
static uintptr_t base;
static uintptr_t guard=0x1020304050607080ULL;
static const char *phase="load";
/* Optional test-only import observer. Native production images are untouched. */
extern void*check_native_symbol(const char*,void*) __attribute__((weak));
#define FN(o,t) ((t)(base+(o)))
static void fault(int signal,siginfo_t *info,void *context)
{
    ucontext_t *u=context;
    fprintf(stderr,"FAIL %s signal=%d native_pc=%lx address=%p\n",phase,signal,
        (unsigned long)(u->uc_mcontext.pc-base),info->si_addr);
    _exit(1);
}
static void load(const char *path)
{
    assert(dlopen("libstdc++.so.6",RTLD_NOW|RTLD_GLOBAL));
    assert(dlopen("libm.so.6",RTLD_NOW|RTLD_GLOBAL));
    dlopen("libmvec.so.1",RTLD_NOW|RTLD_GLOBAL);
    FILE *f=fopen(path,"rb");assert(f);Elf64_Ehdr h;
    assert(fread(&h,1,sizeof(h),f)==sizeof(h));
    assert(h.e_machine==EM_AARCH64 && h.e_type==ET_DYN && h.e_phnum<16);
    unsigned char *m=mmap((void*)0x1800000000ULL,0x7500000,7,
        MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
    assert(m==(void*)0x1800000000ULL);base=(uintptr_t)m;
    Elf64_Phdr ph[16];fseek(f,(long)h.e_phoff,SEEK_SET);
    assert(fread(ph,sizeof(*ph),h.e_phnum,f)==h.e_phnum);
    Elf64_Dyn *dynamic=NULL;
    for(unsigned i=0;i<h.e_phnum;++i) {
        if(ph[i].p_type==PT_LOAD){assert(ph[i].p_vaddr+ph[i].p_memsz<0x7500000);
            fseek(f,(long)ph[i].p_offset,SEEK_SET);
            assert(fread(m+ph[i].p_vaddr,1,ph[i].p_filesz,f)==ph[i].p_filesz);}
        if(ph[i].p_type==PT_DYNAMIC)dynamic=(void*)(m+ph[i].p_vaddr);
    }
    assert(dynamic);uintptr_t rela=0,relasz=0,jmp=0,jmpsz=0,str=0,sym=0;
    for(Elf64_Dyn *d=dynamic;d->d_tag;++d)switch(d->d_tag){
        case DT_RELA:rela=d->d_un.d_ptr;break;case DT_RELASZ:relasz=d->d_un.d_val;break;
        case DT_JMPREL:jmp=d->d_un.d_ptr;break;case DT_PLTRELSZ:jmpsz=d->d_un.d_val;break;
        case DT_STRTAB:str=d->d_un.d_ptr;break;case DT_SYMTAB:sym=d->d_un.d_ptr;break;
    }
    uintptr_t starts[]={rela,jmp},sizes[]={relasz,jmpsz};
    for(unsigned k=0;k<2;++k)for(size_t i=0;i<sizes[k]/sizeof(Elf64_Rela);++i){
        Elf64_Rela *r=(void*)(m+starts[k]+i*sizeof(*r));uintptr_t *out=(void*)(m+r->r_offset);
        if(ELF64_R_TYPE(r->r_info)==R_AARCH64_RELATIVE)*out=base+r->r_addend;
        else {Elf64_Sym *s=(void*)(m+sym+ELF64_R_SYM(r->r_info)*sizeof(*s));
            void *fn=dlsym(RTLD_DEFAULT,(char*)m+str+s->st_name);
            if(fn&&check_native_symbol)fn=check_native_symbol((char*)m+str+s->st_name,fn);
            if(fn)*out=(uintptr_t)fn+r->r_addend;}
    }
    fclose(f);*(void**)(m+0x7164398)=&guard;
    __builtin___clear_cache((char*)m,(char*)m+0x7500000);
}
int main(int argc,char **argv)
{
    assert(argc==2);struct sigaction sa={.sa_sigaction=fault,.sa_flags=SA_SIGINFO};
    sigaction(SIGSEGV,&sa,NULL);sigaction(SIGABRT,&sa,NULL);sigaction(SIGILL,&sa,NULL);
    setbuf(stdout,NULL);load(argv[1]);
    phase="native EQ factory";
    char name[128]={0};
    void *eq=FN(0x24c8290,void*(*)(int,char*))(81,name);
    assert(!eq && !strcmp(name,"Parametric EQ"));
    eq=FN(0x24c8290,void*(*)(int,char*))(81,NULL);assert(eq);
    uintptr_t *v=*(uintptr_t**)eq;
    printf("EQ factory id=81 vptr=%lx name=%s\n",(unsigned long)((uintptr_t)v-base),name);
    phase="native EQ parameter labels";
    for(int i=-1;i<12;++i){
        char label[128]={0};((void(*)(void*,int,char*))v[8])(eq,i,label);
        float value=i<0?0:((float(*)(void*,int))v[7])(eq,i);
        printf("PARAM %d %.9g %s\n",i,(double)value,label);
    }
    phase="native EQ prepare";
    ((void(*)(void*,float))v[2])(eq,48000.f);
    phase="native EQ stereo process";
    float data[2048]={0};data[0]=data[1]=0.5f;unsigned char quiet=0;
    ((void(*)(void*,float*,int,unsigned char*))v[5])(eq,data,1024,&quiet);
    double energy=0;for(int i=0;i<2048;++i){assert(isfinite(data[i]));energy+=data[i]*data[i];}
    printf("EQ impulse default energy=%.9g first=%.9g quiet=%u\n",energy,(double)data[0],quiet);
    assert(energy>0.0001 && energy<10);
    phase="native EQ parameter change";
    ((void(*)(void*,int,float))v[6])(eq,2,0.8f);
    assert(fabsf(((float(*)(void*,int))v[7])(eq,2)-0.8f)<0.00001f);
    ((void(*)(void*,float))v[2])(eq,48000.f);
    memset(data,0,sizeof(data));data[0]=data[1]=0.5f;quiet=0;
    ((void(*)(void*,float*,int,unsigned char*))v[5])(eq,data,1024,&quiet);
    double changed=0;for(int i=0;i<2048;++i){assert(isfinite(data[i]));changed+=data[i]*data[i];}
    printf("EQ changed energy=%.9g\n",changed);assert(fabs(changed-energy)>0.0001);
    phase="native EQ destruction";((void(*)(void*))v[1])(eq);
    puts("PASS native EQ creates, labels, prepares, processes stereo and changes audio");
    return 0;
}
