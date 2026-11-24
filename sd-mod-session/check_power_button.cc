#include <cassert>
#include <cstring>
#include <cstdio>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
static char request_path[1024];
static char boot_path[1024];
#define MPC_POWER_REQUEST request_path
#define MPC_BOOT_REQUEST boot_path
#include "src/power_control.cc"
static int answer,calls,starts,save_status=1,start_ok=1;
static int operation_busy;
static int wake_ok=1,wakes;
static int dialog_result=1,dialog_calls;
extern "C" int check_power_dispatch(void*,void*,QuestionDispatch,uintptr_t);
static void make_string(uint64_t*out,const char*s){*out=reinterpret_cast<uint64_t>(strdup(s));}
static void assign_string(void*out,const uint64_t*value){auto*slot=static_cast<uint64_t*>(out);free(reinterpret_cast<void*>(*slot));*slot=reinterpret_cast<uint64_t>(strdup(reinterpret_cast<const char*>(*value)));}
static void drop_string(uint64_t*slot){free(reinterpret_cast<void*>(*slot));*slot=0;}
static int dialog(void*,void*options){
    ++dialog_calls;char*o=static_cast<char*>(options);
    assert(*reinterpret_cast<int*>(o+96)==3);
    const char*labels[]={"Cancel","Bootloader","Power off"};
    const int ids[]={1,3,2};
    for(unsigned i=0;i<3;++i){assert(!strcmp(reinterpret_cast<const char*>(*reinterpret_cast<uint64_t*>(o+104+8*i)),labels[i]));assert(*reinterpret_cast<int*>(o+128+4*i)==ids[i]);}
    return dialog_result;
}
static int fixture_options(){
    alignas(8) char o[184]={};*reinterpret_cast<int*>(o+96)=2;*reinterpret_cast<int*>(o+128)=1;*reinterpret_cast<int*>(o+132)=2;
    int result=check_power_dispatch(nullptr,o,dialog,base+0x14ebb04);
    for(unsigned off:{8u,16u,104u,112u,120u})drop_string(reinterpret_cast<uint64_t*>(o+off));
    return result==2; // Native Power question converts only id2 to true.
}
extern "C" int power_control_operation_busy(void){return operation_busy;}
extern "C" int power_control_wake(void){++wakes;return wake_ok;}
extern "C" void check_power_continuation(void);
extern "C" int check_power_question(void*environment,uintptr_t page){
    assert(environment==(void*)0x12345678&&page==base+0x7164000);++calls;return answer==3?fixture_options():answer;
}
extern "C" void *workflow_project_services(void){return (void*)0x34567890;}
extern "C" int project_exit_start(uintptr_t b,void*s){assert(b==base&&s==(void*)0x34567890);++starts;return start_ok;}
extern "C" int project_exit_status(void){return save_status;}
extern "C" const char *project_exit_message(void){return "Save failed fixture";}
int main(){
    char folder[]="/tmp/mpc-power-button-XXXXXX";assert(mkdtemp(folder));
    std::snprintf(request_path,sizeof(request_path),"%s/request",folder);
    std::snprintf(boot_path,sizeof(boot_path),"%s/boot",folder);
    void*memory=mmap(nullptr,0x7500000,PROT_READ|PROT_WRITE|PROT_EXEC,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(memory!=MAP_FAILED);
    uintptr_t b=reinterpret_cast<uintptr_t>(memory);auto*entry=reinterpret_cast<uint32_t*>(b+0x14eb910);
    assert(!power_control_install(b));
    const uint32_t expected[]={0xd503233f,0xd106c3ff,0xaa0003e2,0xb002e3c1};std::memcpy(entry,expected,16);
    assert(!power_control_install(b)); // Both guards must pass before either write.
    *reinterpret_cast<uint32_t*>(b+0x14ebb04)=0xd63f0040;
    *reinterpret_cast<uint32_t*>(b+0x14ebb08)=0xd61f0260; // Fixture continuation BR x19.
    const uint32_t jump[]={0x58000050,0xd61f0200};std::memcpy(entry+4,jump,8);
    *reinterpret_cast<uintptr_t*>(entry+6)=reinterpret_cast<uintptr_t>(check_power_continuation);
    __builtin___clear_cache(reinterpret_cast<char*>(entry),reinterpret_cast<char*>(entry)+32);
    for(auto hook:{std::make_pair(uintptr_t(0xf535c0),reinterpret_cast<uintptr_t>(make_string)),std::make_pair(uintptr_t(0xf1c6f0),reinterpret_cast<uintptr_t>(assign_string)),std::make_pair(uintptr_t(0xf0a290),reinterpret_cast<uintptr_t>(drop_string))}){
        std::memcpy(reinterpret_cast<void*>(b+hook.first),jump,8);std::memcpy(reinterpret_cast<void*>(b+hook.first+8),&hook.second,8);
        __builtin___clear_cache(reinterpret_cast<char*>(b+hook.first),reinterpret_cast<char*>(b+hook.first+16));
    }
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
    unlink(request_path);unlink(target.c_str());
    state=IDLE;answer=3;dialog_result=1;int before=starts;
    ask((void*)0x12345678);assert(state==IDLE&&starts==before&&!boot_choice);
    dialog_result=3;ask((void*)0x12345678);assert(state==PENDING&&destination==BOOTLOADER);
    save_status=3;power_control_tick();power_control_tick();assert(state==FAILED&&access(boot_path,F_OK)&&access(request_path,F_OK));
    power_control_button(0x0e,1);save_status=2;power_control_tick();power_control_tick();
    assert(state==REQUESTED&&destination==BOOTLOADER&&access(request_path,F_OK));
    assert(!lstat(boot_path,&st)&&S_ISREG(st.st_mode)&&(st.st_mode&0777)==0600&&st.st_size==1);unlink(boot_path);
    state=IDLE;dialog_result=2;ask((void*)0x12345678);assert(destination==POWER_OFF);power_control_tick();power_control_tick();assert(state==REQUESTED&&!access(request_path,F_OK)&&access(boot_path,F_OK));unlink(request_path);
    assert(dialog_calls==3);rmdir(folder);
    puts("PASS native option bridge Cancel/B2 Bootloader/B3 Power off, native string lifetime, separate host requests, failed-save retry retains boot destination");
    puts("PASS relocated native shutdown-question entry/PAC stack, No, Yes, repeated Power, Stop request, pending save, failed save and retry, durable-save-only request0600, symlink refusal; host poweroff not executed");
}
