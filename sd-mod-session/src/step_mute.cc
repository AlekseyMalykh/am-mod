/* Native replacement map, rather than remove/add normalization: the stock
 * bulk factory cancels the replacement when only mute differs. */
#include <map>
#include <stdint.h>
#include <stddef.h>
struct event {int64_t time;uint32_t type,reserved;unsigned char data[40];};
static uintptr_t base;
struct Less {bool operator()(const event&a,const event&b)const {
 return ((int(*)(const event*,const event*))(base+0x19d4d70))(&a,&b)&255;
}};
using Changes=std::multimap<event,event,Less>;
static_assert(sizeof(Changes)==48,"Pinned native replacement map ABI");
extern "C" void *step_mute_map(uintptr_t b) {base=b;return new Changes;}
extern "C" void step_mute_pair(void*p,const event*old,const event*value) {
 static_cast<Changes*>(p)->emplace(*old,*value);
}
extern "C" void step_mute_free(void*p) noexcept {delete static_cast<Changes*>(p);}
