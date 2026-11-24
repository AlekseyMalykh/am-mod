#include <cassert>
#include "src/power_control.cc"
static int result,calls,starts;
extern "C" int power_control_operation_busy(void){return 0;}
extern "C" int power_control_wake(void){return 1;}
extern "C" void* workflow_project_services(void){return nullptr;}
extern "C" int project_exit_start(uintptr_t,void*){++starts;return 0;}
extern "C" int project_exit_status(void){return 0;}
extern "C" const char*project_exit_message(void){return "Fixture";}
static int receive(void*,void*options){
 ++calls;char*o=static_cast<char*>(options);
 assert(*reinterpret_cast<int*>(o+96)==3);
 const char*labels[]={"Cancel","Bootloader","Power off"};const int ids[]={1,3,2};
 for(unsigned i=0;i<3;++i){
  assert(!std::strcmp(reinterpret_cast<const char*>(*reinterpret_cast<uint64_t*>(o+104+8*i)),labels[i]));
  assert(*reinterpret_cast<int*>(o+128+4*i)==ids[i]);
 }
 return result;
}
extern "C" void check_power_native(uintptr_t image){
 assert(power_control_install(image));
 uintptr_t vtable[3]={0,0,reinterpret_cast<uintptr_t>(receive)};
 void*manager=vtable;alignas(8) char environment[0x210]={};
 *reinterpret_cast<void**>(environment+0x208)=&manager;
 auto ask=reinterpret_cast<int(*)(void*)>(image+0x14eb910);
 result=1;assert(!ask(environment)&&state==IDLE&&!starts);
 result=3;assert(!ask(environment)&&state==PENDING&&destination==BOOTLOADER&&!starts);
 state=IDLE;result=2;assert(!ask(environment)&&state==PENDING&&destination==POWER_OFF&&!starts);
 assert(calls==3);state=IDLE;
 std::puts("PASS actual pinned native Power question constructs/destructs all option strings, three button IDs, B2 bootloader and B3 poweroff admission; GUI manager/save are fixtures");
}
