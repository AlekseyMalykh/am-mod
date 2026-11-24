#include "pattern_native.h"
#include <algorithm>
#include <bits/stl_tree.h>
#include <cstddef>
#include <cstring>
#include <new>

namespace am_pattern {
namespace {
template<class T>T field(const void*p,std::size_t offset){T v;std::memcpy(&v,static_cast<const char*>(p)+offset,sizeof v);return v;}
const char*at(const void*p,std::size_t n){return static_cast<const char*>(p)+n;}
template<class T>T fn(std::uintptr_t image,std::uintptr_t offset){return reinterpret_cast<T>(image+offset);}
static_assert(sizeof(void*)==8,"Pinned AArch64 native ABI");
static_assert(offsetof(std::_Rb_tree_node_base,_M_parent)==8,"Native tree parent");
static_assert(offsetof(std::_Rb_tree_node_base,_M_left)==16,"Native tree left");
static_assert(offsetof(std::_Rb_tree_node_base,_M_right)==24,"Native tree right");
struct Lease {
    const NativeReadAccess&access;const NativeReadContext&context;
    ~Lease(){access.leave(access.user,context);}
};
struct Read {
    void *sequence=nullptr,*track=nullptr,*program=nullptr,*list=nullptr;
    std::int64_t length=0;std::string key;
    std::array<int,128> forward,reverse;
    std::vector<Route> routes;std::vector<InputEvent> events;
};
bool aligned(const void*p){return p&&!(reinterpret_cast<std::uintptr_t>(p)&7u);}
NativeReadResult scan(const NativeReadRequest&request,const NativeReadContext&context,Read&out){
    if(!context.image||!aligned(context.project)||!aligned(context.things))return NativeReadResult::invalid;
    auto image=context.image;
    if(field<std::uintptr_t>(context.project,0)!=image+0x6ecc648)return NativeReadResult::unsupported;
    int count=field<int>(context.project,0x125c);
    if(count<0||count>1024||request.sequence_index>=count||!field<void*>(context.project,0x1250))return NativeReadResult::invalid;
    out.sequence=fn<void*(*)(void*,int)>(image,0x2909be0)(context.project,request.sequence_index);
    if(!aligned(out.sequence)||field<std::uintptr_t>(out.sequence,0)!=image+0x6ecdff0)return NativeReadResult::unsupported;
    out.length=fn<std::int64_t(*)(void*)>(image,0x29571f0)(out.sequence);
    if(out.length<=0)return NativeReadResult::invalid;
    out.track=field<void*>(context.things,0x368);
    if(!aligned(out.track))return NativeReadResult::invalid;
    out.program=field<void*>(out.track,0xbb8);
    if(!aligned(out.program)||out.program!=field<void*>(context.things,0xb8))return NativeReadResult::stale;
    const char*key=field<const char*>(out.program,0xdc8);
    if(!key)return NativeReadResult::invalid;
    std::size_t key_length=0;while(key_length<128&&key[key_length])++key_length;
    if(!key_length||key_length==128)return NativeReadResult::invalid;
    out.key.assign(key,key_length);
    for(unsigned i=0;i<128;++i){
        if(context.outputs[i]<-1||context.outputs[i]>8)return NativeReadResult::invalid;
        out.forward[i]=field<int>(out.program,0x12a8+i*4);
        out.reverse[i]=field<int>(out.program,0x14a8+i*4);
    }
    /* 0 selects the default Track->Clip map. Nonzero selects an alternate
     * pre-existing map; this parameter does not enable Clip creation. */
    out.list=fn<void*(*)(void*,void*,int)>(image,0x2957ae0)(out.sequence,out.track,0);
    if(!out.list)return NativeReadResult::ok; /* Absent clip is empty, never create. */
    if(!aligned(out.list))return NativeReadResult::invalid;
    auto header=at(out.list,0x68);void*root=field<void*>(header,8);
    auto size=field<std::uint64_t>(out.list,0x88);
    if(size>max_events)return NativeReadResult::limit;
    void*first=field<void*>(header,16),*last=field<void*>(header,24);
    if(!root)return !size&&first==header&&last==header?NativeReadResult::ok:NativeReadResult::invalid;
    if(!size)return NativeReadResult::invalid;
    out.events.reserve(static_cast<std::size_t>(size));
    std::array<int,128> routes;routes.fill(-1);
    /* Same libstdc++ tree ABI as batch_events/Piano Roll. Bounded in-order
     * traversal avoids an unbounded std::_Rb_tree_increment on broken links.
     * Parent reciprocity + distinct children rejects cycles/shared nodes.
     * Pointer readability itself remains the lifetime owner's obligation. */
    std::array<void*,128> stack;unsigned depth=0;std::uint64_t visited=0;
    void*node=root;const void*parent=header;void*previous=nullptr;
    while(node||depth){
        while(node){
            if(!aligned(node)||node==header||field<void*>(node,8)!=parent)return NativeReadResult::invalid;
            if(++visited>size)return NativeReadResult::invalid;
            if(depth==stack.size())return NativeReadResult::limit;
            void*left=field<void*>(node,16),*right=field<void*>(node,24);
            if(left&&left==right)return NativeReadResult::invalid;
            stack[depth++]=node;parent=node;node=left;
        }
        node=stack[--depth];
        if(out.events.empty()&&node!=first)return NativeReadResult::invalid;
        if((field<std::uint32_t>(node,0x28)&255)!=3)return NativeReadResult::unsupported;
        InputEvent value;std::memcpy(&value.native,at(node,0x20),sizeof value.native);
        unsigned sound=value.native.data[32];
        if(sound>=128)return NativeReadResult::invalid;
        int pad=out.reverse[sound];
        if(pad<0||pad>=128||out.forward[pad]!=static_cast<int>(sound))return NativeReadResult::invalid;
        if(routes[pad]<0){
            Route route;route.program_key=out.key;route.pad=static_cast<unsigned>(pad);route.source_note=sound;
            route.mixer_bus=context.outputs[pad]<0?-2:context.outputs[pad]-1;
            routes[pad]=static_cast<int>(out.routes.size());out.routes.push_back(std::move(route));
        }
        value.route=static_cast<unsigned>(routes[pad]);out.events.push_back(value);
        previous=node;parent=node;node=field<void*>(node,24);
    }
    return visited==size&&out.events.size()==size&&previous==last?NativeReadResult::ok:NativeReadResult::invalid;
}
bool same(const Read&a,const Read&b){
    if(a.sequence!=b.sequence||a.track!=b.track||a.program!=b.program||a.list!=b.list
       ||a.length!=b.length||a.key!=b.key||a.forward!=b.forward||a.reverse!=b.reverse
       ||a.events.size()!=b.events.size())return false;
    for(std::size_t i=0;i<a.events.size();++i){
        const auto&x=a.events[i];const auto&y=b.events[i];
        if(x.route!=y.route||x.native.time!=y.native.time||((x.native.type^y.native.type)&~0x100u)
           ||std::memcmp(x.native.data,y.native.data,sizeof x.native.data))return false;
    }
    return true;
}
struct Hash {
    std::uint64_t value=UINT64_C(1469598103934665603);
    void byte(unsigned char b){value=(value^b)*UINT64_C(1099511628211);}
    void number(std::uint64_t n){for(unsigned i=0;i<8;++i){byte(static_cast<unsigned char>(n));n>>=8;}}
    void string(const std::string&s){number(s.size());for(unsigned char c:s)byte(c);}
};
std::uint64_t revision(const NativeReadRequest&request,const NativeReadContext&context,const Read&r){
    Hash h;h.string(request.sequence_key);h.number(context.generation);h.number(context.routing_revision);
    h.number(static_cast<std::uint64_t>(r.length));h.string(r.key);
    for(unsigned i=0;i<128;++i){h.number(r.forward[i]);h.number(r.reverse[i]);h.number(context.outputs[i]);}
    h.number(r.events.size());
    for(const auto&e:r.events){
        h.number(e.native.time);h.number(e.native.type&~0x100u);h.number(e.route);
        /* Match Piano Roll's 37-byte musical payload hash, not ABI padding. */
        for(unsigned i=0;i<37;++i)h.byte(e.native.data[i]);
    }
    return h.value;
}
NativeReadResult convert(Result result){
    switch(result){case Result::ok:return NativeReadResult::ok;case Result::invalid:return NativeReadResult::invalid;
    case Result::unsupported:return NativeReadResult::unsupported;case Result::limit:return NativeReadResult::limit;
    case Result::stale:return NativeReadResult::stale;}
    return NativeReadResult::invalid;
}
}
NativeReadResult read_native(const NativeReadRequest&request,const NativeReadAccess&access,std::shared_ptr<const Snapshot>&out)noexcept{
    try{
        if(!request.snapshot_id||request.sequence_key.empty()||request.sequence_key.size()>=128
           ||request.sequence_key.find('\0')!=std::string::npos||request.sequence_index<0||request.sequence_index>=1024
           ||!access.enter||!access.valid||!access.leave)return NativeReadResult::invalid;
        NativeReadContext context;
        if(!access.enter(access.user,context))return NativeReadResult::unavailable;
        Lease lease{access,context};
        if(!context.read_safe||!context.generation)return NativeReadResult::unavailable;
        if(!access.valid(access.user,context))return NativeReadResult::stale;
        Read first;auto result=scan(request,context,first);if(result!=NativeReadResult::ok)return result;
        if(!access.valid(access.user,context))return NativeReadResult::stale;
        {
            Read second;result=scan(request,context,second);if(result!=NativeReadResult::ok)return result;
            if(!same(first,second)||!access.valid(access.user,context))return NativeReadResult::stale;
        } /* Release the comparison buffer before allocating the final snapshot. */
        std::shared_ptr<const Snapshot> value;
        result=convert(Snapshot::capture(request.snapshot_id,request.sequence_key,revision(request,context,first),first.length,
             first.routes,first.events,fn<am_midi_choice_get>(context.image,0x27e4230),value));
        if(result!=NativeReadResult::ok)return result;
        if(!access.valid(access.user,context))return NativeReadResult::stale;
        out=std::move(value);return NativeReadResult::ok;
    }catch(const std::bad_alloc&){return NativeReadResult::allocation_failed;}
    catch(...){return NativeReadResult::exception;}
}
}
