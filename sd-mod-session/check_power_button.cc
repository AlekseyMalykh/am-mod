#include <cassert>
#include <cstring>
#include <cstdio>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
static char request_path[1024];
#define MPC_POWER_REQUEST request_path
#include "src/power_control.cc"
static int answer,calls,starts,save_status=1,start_ok=1;
static int operation_busy;
static int wake_ok=1,wakes;
extern "C" int power_control_operation_busy(void){return operation_busy;}
extern "C" int power_control_wake(void){++wakes;return wake_ok;}
extern "C" void check_power_continuation(void);
extern "C" int check_power_question(void*environment,uintptr_t page){
    assert(environment==(void*)0x12345678&&page==base+0x7164000);++calls;return answer;
}
extern "C" void *workflow_project_services(void){return (void*)0x34567890;}
extern "C" int project_exit_start(uintptr_t b,void*s){assert(b==base&&s==(void*)0x34567890);++starts;return start_ok;}
extern "C" int project_exit_status(void){return save_status;}
extern "C" const char *project_exit_message(void){return "Save failed fixture";}
int main(){
    char folder[]="/tmp/mpc-power-button-XXXXXX";assert(mkdtemp(folder));
    std::snprintf(request_path,sizeof(request_path),"%s/request",folder);
    void*memory=mmap(nullptr,0x7500000,PROT_READ|PROT_WRITE|PROT_EXEC,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(memory!=MAP_FAILED);
    uintptr_t b=reinterpret_cast<uintptr_t>(memory);auto*entry=reinterpret_cast<uint32_t*>(b+0x14eb910);
    assert(!power_control_install(b));
    const uint32_t expected[]={0xd503233f,0xd106c3ff,0xaa0003e2,0xb002e3c1};std::memcpy(entry,expected,16);
    const uint32_t jump[]={0x58000050,0xd61f0200};std::memcpy(entry+4,jump,8);
    *reinterpret_cast<uintptr_t*>(entry+6)=reinterpret_cast<uintptr_t>(check_power_continuation);
    __builtin___clear_cache(reinterpret_cast<char*>(entry),reinterpret_cast<char*>(entry)+32);
    assert(power_control_install(b)&&power_control_install(b));
    auto ask=reinterpret_cast<int(*)(void*)>(entry);
    assert(!ask((void*)0x12345678)&&calls==1&&!power_control_busy()&&!starts&&access(request_path,F_OK));
    answer=1;wake_ok=0;assert(!ask((void*)0x12345678)&&calls==2&&!power_control_busy()&&!starts&&access(request_path,F_OK));
    wake_ok=1;assert(!ask((void*)0x12345678)&&calls==3&&power_control_busy()&&wakes==2);
    assert(!ask((void*)0x12345678)&&calls==3); // Repeated Power cannot re-enter.
    assert(power_control_take_stop()&&!power_control_take_stop());
    power_control_tick();assert(starts==1&&state==SAVING&&access(request_path,F_OK));
    power_control_tick();assert(state==SAVING&&access(request_path,F_OK));
    save_status=3;power_control_tick();assert(state==FAILED&&access(request_path,F_OK));
    assert(power_control_button(0x0c,1)&&power_control_busy());
    power_control_tick();assert(!power_control_busy());
    assert(power_control_button(0x0c,0)); // Release belongs to the closed modal.
    assert(!power_control_button(0x0c,0));
    operation_busy=1;ask((void*)0x12345678);assert(!power_control_take_stop());power_control_tick();assert(state==FAILED&&starts==1&&access(request_path,F_OK));
    power_control_button(0x0c,1);power_control_tick();operation_busy=0;
    start_ok=0;ask((void*)0x12345678);power_control_tick();assert(state==FAILED&&access(request_path,F_OK));
    power_control_button(0x0e,1);assert(state==FAILED);start_ok=1;save_status=2;
    power_control_tick();power_control_tick();assert(state==REQUESTED);
    struct stat st{};assert(!lstat(request_path,&st)&&S_ISREG(st.st_mode)&&(st.st_mode&0777)==0600&&st.st_size==1);
    char body=0;int fd=open(request_path,O_RDONLY);assert(fd>=0&&read(fd,&body,1)==1&&body=='1');close(fd);unlink(request_path);
    state=IDLE;std::string target=std::string(folder)+"/target";fd=open(target.c_str(),O_WRONLY|O_CREAT|O_EXCL,0600);assert(fd>=0&&write(fd,"x",1)==1);close(fd);
    assert(!symlink(target.c_str(),request_path));ask((void*)0x12345678);power_control_tick();power_control_tick();assert(state==FAILED);
    fd=open(target.c_str(),O_RDONLY);assert(fd>=0&&read(fd,&body,1)==1&&body=='x');close(fd);
    unlink(request_path);unlink(target.c_str());rmdir(folder);
    puts("PASS relocated native shutdown-question entry/PAC stack, No, Yes, repeated Power, Stop request, pending save, failed save and retry, durable-save-only request0600, symlink refusal; host poweroff not executed");
}
