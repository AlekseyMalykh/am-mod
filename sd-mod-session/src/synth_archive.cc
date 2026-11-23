/* Same native archive ABI and transaction discipline as mixer_archive.cc.
 * Separate musical instrument state; no mixer-bus/track identity coupling. */
#include "synth_archive.h"
#include <atomic>
#include <cstring>
#include <exception>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
namespace {
const char key[]="amSynthV1";
struct State {am_synth_binding binding{};unsigned mode=AM_SYNTH_ARCHIVE_LEGACY;uint64_t revision=0;std::string opaque;};
std::mutex mutex;std::shared_ptr<const State>current;uintptr_t image;
std::atomic<uint64_t>generation{1};std::atomic<unsigned>loading{0};
struct Change {Change(){loading.fetch_add(1);generation.fetch_add(1);}~Change(){generation.fetch_add(1);loading.fetch_sub(1);}};
template<class T>T field(const void*p,size_t at){T v;std::memcpy(&v,static_cast<const char*>(p)+at,sizeof v);return v;}
struct Bytes{const char*data;size_t size;};
Bytes string_value(const void*p){
 unsigned flags=field<unsigned short>(p,22);if(!(flags&(1u<<10)))throw std::runtime_error("Synth extension must be a string");
 if(flags&(1u<<12)){unsigned spare=field<unsigned char>(p,21);if(spare>21)throw std::runtime_error("Invalid inline Synth archive string");return {static_cast<const char*>(p),21-spare};}
 size_t size=field<uint32_t>(p,0);const char*s=field<const char*>(p,8);if(!s&&size)throw std::runtime_error("Missing Synth archive string");return {s?s:"",size};
}
std::shared_ptr<State>capture(void*wrapper){
 auto next=std::make_shared<State>();void*a=wrapper;
 for(unsigned i=0;i<4;++i){if(!a)throw std::runtime_error("Synth input archive unavailable");a=field<void*>(a,8);}
 if(!a)throw std::runtime_error("Synth input archive unavailable");
 uintptr_t cursor=field<uintptr_t>(a,0x150);if(cursor<0x38)throw std::runtime_error("Synth input frame unavailable");
 const void*f=reinterpret_cast<const void*>(cursor-0x38);if(field<unsigned>(f,0x30)!=1)throw std::runtime_error("Synth extension outside object");
 uintptr_t begin=field<uintptr_t>(f,0),end=field<uintptr_t>(f,8);
 if(end<begin||(end-begin)%48||(!begin&&end))throw std::runtime_error("Invalid Synth input members");
 bool found=false;
 for(uintptr_t at=begin;at<end;at+=48){Bytes name=string_value(reinterpret_cast<void*>(at));
  if(name.size!=sizeof(key)-1||std::memcmp(name.data,key,name.size))continue;
  if(found)throw std::runtime_error("Duplicate Synth extension");
  found=true;
  Bytes value=string_value(reinterpret_cast<void*>(at+24));
  if(am_synth_binding_decode(value.data,value.size,&next->binding)==AM_SYNTH_BINDING_OK)next->mode=AM_SYNTH_ARCHIVE_KNOWN;
  else{next->mode=AM_SYNTH_ARCHIVE_OPAQUE;next->opaque.assign(value.data,value.size);}
 }
 return next;
}
void publish(std::shared_ptr<State>next){std::lock_guard<std::mutex>lock(mutex);
 if(current&&current->revision==UINT64_MAX)throw std::overflow_error("Synth revision exhausted");
 next->revision=current?current->revision+1:1;current=std::move(next);
}
std::shared_ptr<const State>snapshot(){std::lock_guard<std::mutex>lock(mutex);if(!current)throw std::runtime_error("Synth archive not initialized");return current;}
struct Transaction{std::shared_ptr<State>pending;std::exception_ptr failure;bool captured=false;};
thread_local Transaction*transaction;
}
extern "C" void am_synth_archive_init(uintptr_t base){
 if(!base)throw std::invalid_argument("Synth archive requires native image");
 auto initial=std::make_shared<State>();std::lock_guard<std::mutex>lock(mutex);
 if(image&&image!=base)throw std::runtime_error("Synth archive image changed");
 image=base;
 if(!current){initial->revision=1;current=std::move(initial);}
}
extern "C" void am_synth_archive_reset(){Change change;publish(std::make_shared<State>());}
extern "C" uint64_t am_synth_archive_generation(){return generation.load(std::memory_order_acquire);}
extern "C" int am_synth_archive_loading(){return loading.load(std::memory_order_acquire)!=0;}
extern "C" int am_synth_archive_snapshot(am_synth_binding*out,uint64_t*revision,unsigned*mode){
 if(!out||!revision||!mode)return 0;
 auto s=snapshot();*out=s->binding;*revision=s->revision;*mode=s->mode;return 1;
}
extern "C" am_synth_archive_edit am_synth_archive_replace(const am_synth_binding*b,uint64_t revision){
 if(!am_synth_binding_valid(b))return AM_SYNTH_EDIT_INVALID;
 auto next=std::make_shared<State>();next->binding=*b;next->mode=AM_SYNTH_ARCHIVE_KNOWN;
 std::lock_guard<std::mutex>lock(mutex);
 if(!current||current->revision!=revision)return AM_SYNTH_EDIT_STALE;
 if(current->mode==AM_SYNTH_ARCHIVE_OPAQUE)return AM_SYNTH_EDIT_READONLY;
 if(revision==UINT64_MAX||loading.load(std::memory_order_acquire))return AM_SYNTH_EDIT_INVALID;
 next->revision=revision+1;current=std::move(next);return AM_SYNTH_EDIT_OK;
}
extern "C" void am_synth_archive_load(void*data,void*a,void(*original)(void*,void*)){
 Change change;original(data,a);if(!transaction)return;
 try{if(transaction->captured)throw std::runtime_error("Multiple Synth project objects");transaction->captured=true;transaction->pending=capture(a);}
 catch(...){transaction->failure=std::current_exception();}
}
extern "C" int am_synth_archive_read(void*data,void*json,int(*original)(void*,void*)){
 Change change;if(transaction)throw std::runtime_error("Nested Synth project read");
 Transaction pending;pending.pending=std::make_shared<State>();
 struct Scope{Transaction*old;explicit Scope(Transaction*t):old(transaction){transaction=t;}~Scope(){transaction=old;}}scope(&pending);
 int ok=original(data,json);if(pending.failure)std::rethrow_exception(pending.failure);if(ok)publish(std::move(pending.pending));return ok;
}
extern "C" void am_synth_archive_save(void*data,void*a,void(*original)(void*,void*)){
 auto s=snapshot();std::string payload;
 if(s->mode==AM_SYNTH_ARCHIVE_OPAQUE)payload=s->opaque;
 else if(s->mode==AM_SYNTH_ARCHIVE_KNOWN){size_t size=0;
  if(am_synth_binding_encode(&s->binding,nullptr,0,&size)!=AM_SYNTH_BINDING_OK)throw std::runtime_error("Invalid Synth snapshot");
  payload.resize(size+1);if(am_synth_binding_encode(&s->binding,&payload[0],payload.size(),nullptr)!=AM_SYNTH_BINDING_OK)throw std::runtime_error("Synth encoding failed");payload.resize(size);
 }
 original(data,a);
 if(s->mode!=AM_SYNTH_ARCHIVE_LEGACY){struct{const char*data;size_t size,capacity,unused;}text={payload.data(),payload.size(),payload.size(),0};
  reinterpret_cast<void(*)(void*,const char*,const void*)>(image+0x1d27d90)(a,key,&text);
 }
}
