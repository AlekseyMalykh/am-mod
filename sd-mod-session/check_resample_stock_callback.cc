#include <functional>
#include <new>
#include <cassert>
#include <cstdint>
struct NativeName {uintptr_t value;~NativeName(){}};
extern "C" void stock_name_call(uintptr_t fn,uintptr_t*out,const char*prefix,const void*name){NativeName s=reinterpret_cast<NativeName(*)(const char*,const void*)>(fn)(prefix,name);*out=s.value;}
static std::function<void()> retained;
static unsigned called;
extern "C" void stock_callback_init(void*p){new(p)std::function<void()>([](){++called;});}
extern "C" void stock_callback_init_action(void*p,void(*action)()){new(p)std::function<void()>([action](){++called;action();});}
extern "C" void stock_callback_keep(void*p){retained=*static_cast<std::function<void()>*>(p);}
extern "C" unsigned stock_callback_invoke(){assert(retained);retained();return called;}
extern "C" unsigned stock_callback_release(int invoke){if(invoke){assert(retained);retained();}retained={};return called;}
extern "C" void stock_callback_destroy(void*p){static_cast<std::function<void()>*>(p)->~function();}
