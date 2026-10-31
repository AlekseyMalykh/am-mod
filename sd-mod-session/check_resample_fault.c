#define _GNU_SOURCE
#include "src/resample_fault.h"
#include <assert.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <ucontext.h>
#include <unistd.h>
static void old_handler(int n){assert(n==SIGSEGV);ssize_t written=write(2,"PREVIOUS_ACTION\n",16);(void)written;_exit(42);}
static void old_info(int n,siginfo_t*info,void*context){ucontext_t*u=context;assert(n==SIGSEGV&&(uintptr_t)info->si_addr==UINT64_C(0x180211a400)&&u->uc_mcontext.pc==UINT64_C(0x180211a400));old_handler(n);}
static void test(int custom){
 int stream[2];assert(!pipe(stream));pid_t child=fork();assert(child>=0);
 if(!child){
  struct rlimit limit={0,0};assert(!setrlimit(RLIMIT_CORE,&limit));close(stream[0]);assert(dup2(stream[1],2)==2);close(stream[1]);
  if(custom==1)signal(SIGSEGV,old_handler);
  if(custom==2){struct sigaction action={0};action.sa_flags=SA_SIGINFO;action.sa_sigaction=old_info;sigemptyset(&action.sa_mask);assert(!sigaction(SIGSEGV,&action,NULL));}
  assert(resample_fault_install(UINT64_C(0x1800000000))&&resample_fault_install(UINT64_C(0x1800000000)));resample_fault_stage(7);
  void*page=(void*)UINT64_C(0x180211a000);assert(mmap(page,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0)==page);
  uint32_t*entry=(void*)((char*)page+0x400);entry[0]=0xd503245f;entry[1]=0xd65f03c0;assert(!mprotect(page,4096,PROT_READ));
  ((void(*)(void*))entry)(NULL);_exit(99);
 }
 close(stream[1]);char text[2048]={0};size_t used=0;ssize_t n;
 while((n=read(stream[0],text+used,sizeof(text)-used-1))>0)used+=(size_t)n;
 close(stream[0]);int status;assert(waitpid(child,&status,0)==child);
 assert(strstr(text,"RESAMPLE_FAULT stage=0x0000000000000007")&&strstr(text,"native_pc=0x000000000211a400"));
 char*first=strstr(text,"RESAMPLE_FAULT");assert(!strstr(first+1,"RESAMPLE_FAULT"));
 if(custom)assert(WIFEXITED(status)&&WEXITSTATUS(status)==42&&strstr(text,"PREVIOUS_ACTION"));
 else assert(WIFSIGNALED(status)&&WTERMSIG(status)==SIGSEGV);
}
int main(void){test(0);test(1);test(2);puts("PASS async-safe AArch64 fault diagnostic records real execution-permission SIGSEGV PC/registers/stage; repeated install does not replace original action; default signal death, original handler and its exact siginfo/ucontext preserved.");return 0;}
