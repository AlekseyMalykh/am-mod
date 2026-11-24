#include "src/pattern_native.h"
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <new>
#include <stdexcept>
extern "C" {
void*pattern_fixture_project(void);void*pattern_fixture_things(void);void*pattern_fixture_program(void);
void*pattern_fixture_sequence(unsigned);void*pattern_fixture_list(unsigned);void*pattern_fixture_node(unsigned,unsigned);
}
using namespace am_pattern;
static int allocations_until_failure=-1;
void*operator new(std::size_t n){
    if(allocations_until_failure==0)throw std::bad_alloc();
    if(allocations_until_failure>0)--allocations_until_failure;
    void*p=std::malloc(n?n:1);if(!p)throw std::bad_alloc();return p;
}
void operator delete(void*p)noexcept{std::free(p);}
template<class T>T get(const void*p,std::size_t n){T v;std::memcpy(&v,static_cast<const char*>(p)+n,sizeof v);return v;}
template<class T>void set(void*p,std::size_t n,T v){std::memcpy(static_cast<char*>(p)+n,&v,sizeof v);}
struct Owner {
    std::uintptr_t image;unsigned entered=0,left=0,checks=0;
    bool available=true,safe=true,unknown_routes=false;
    unsigned fail_check=0,mutate_check=0,throw_check=0;
    std::uint64_t generation=7,routing_revision=9;
};
static bool enter(void*user,NativeReadContext&context){
    auto&o=*static_cast<Owner*>(user);if(!o.available)return false;
    ++o.entered;o.checks=0;context.image=o.image;context.project=pattern_fixture_project();context.things=pattern_fixture_things();
    context.read_safe=o.safe;context.generation=o.generation;context.routing_revision=o.routing_revision;
    if(!o.unknown_routes){context.outputs.fill(0);context.outputs[16]=3;}
    return true;
}
static bool valid(void*user,const NativeReadContext&context){
    auto&o=*static_cast<Owner*>(user);++o.checks;
    if(o.checks==o.throw_check)throw std::runtime_error("fixture owner failure");
    if(o.checks==o.mutate_check){float changed=.25f;set(pattern_fixture_node(0,0),0x30,changed);}
    return o.checks!=o.fail_check&&context.generation==o.generation&&context.routing_revision==o.routing_revision;
}
static void leave(void*user,const NativeReadContext&){auto&o=*static_cast<Owner*>(user);++o.left;}
static bool same_note(const step_event&a,const step_event&b){return a.time==b.time&&a.type==(b.type&~0x100u)&&!std::memcmp(a.data,b.data,sizeof a.data);}
extern "C" int pattern_native_proof(std::uintptr_t image){
    Owner owner;owner.image=image;NativeReadAccess access;access.user=&owner;access.enter=enter;access.valid=valid;access.leave=leave;
    NativeReadRequest request;request.snapshot_id=100;request.sequence_key="stable-sequence-id";request.sequence_index=0;
    step_event before[3];for(unsigned i=0;i<3;++i)std::memcpy(&before[i],static_cast<char*>(pattern_fixture_node(0,i))+0x20,sizeof(step_event));
    std::shared_ptr<const Snapshot> snapshot;
    assert(read_native(request,access,snapshot)==NativeReadResult::ok&&owner.entered==1&&owner.left==1);
    assert(snapshot->events().size()==3&&snapshot->length()==7680&&snapshot->routes().size()==2);
    for(unsigned i=0;i<3;++i){assert(same_note(snapshot->events()[i].native,before[i]));
        assert(!std::memcmp(&before[i],static_cast<char*>(pattern_fixture_node(0,i))+0x20,sizeof(step_event)));}
    assert(snapshot->routes()[0].pad==16&&snapshot->routes()[0].source_note==52&&snapshot->routes()[0].mixer_bus==2);
    assert(snapshot->routes()[1].pad==0&&snapshot->routes()[1].source_note==36&&snapshot->routes()[1].mixer_bus==-1);
    assert(snapshot->events()[1].id!=snapshot->events()[2].id);
    assert(((snapshot->events()[1].native.type>>12)&15)==6);
    auto getter=reinterpret_cast<am_midi_choice_get>(image+0x27e4230);am_midi_event decoded{};
    assert(am_midi_event_copy(&snapshot->events()[1].native,getter,AM_MIDI_EVENT_NOTE_ON,&decoded));
    assert(decoded.tune==7&&decoded.slice==3&&decoded.repeats==3&&decoded.muted&&decoded.probability==73);
    Parts parts;assert(Parts::split(snapshot,SplitBy::pads,parts)==Result::ok&&parts.parts().size()==2);
    Pattern sample;ExtractionInfo info;
    assert(extract(parts,1,0,3840,BoundaryPolicy::onsets_only,200,sample,info)==Result::ok&&info.retained_tails==1);
    assert(get<std::int64_t>(&sample.events()[0].native,0x28)==10000);
    auto original=snapshot;auto original_revision=snapshot->revision();
    assert(read_native(request,access,snapshot)==NativeReadResult::ok&&snapshot->revision()==original_revision);
    /* Selection is UI state, not a musical revision. */
    auto*first=pattern_fixture_node(0,0);auto type=get<std::uint32_t>(first,0x28);
    set(first,0x28,type^0x100u);
    assert(read_native(request,access,snapshot)==NativeReadResult::ok&&snapshot->revision()==original_revision);
    set(first,0x28,type);
    owner.unknown_routes=true;
    assert(read_native(request,access,snapshot)==NativeReadResult::ok&&snapshot->revision()!=original_revision);
    assert(Parts::split(snapshot,SplitBy::mixer,parts)==Result::unsupported);
    owner.unknown_routes=false;
    ++owner.generation;
    assert(read_native(request,access,snapshot)==NativeReadResult::ok&&snapshot->revision()!=original_revision);
    --owner.generation;snapshot=original;

    auto rejected=[&](NativeReadResult expected){
        auto old=snapshot;unsigned entered=owner.entered,left=owner.left;
        assert(read_native(request,access,snapshot)==expected&&snapshot==old);
        assert(owner.entered==entered+1&&owner.left==left+1);
    };
    owner.available=false;unsigned left=owner.left;
    assert(read_native(request,access,snapshot)==NativeReadResult::unavailable&&snapshot==original&&owner.left==left);
    owner.available=true;owner.safe=false;rejected(NativeReadResult::unavailable);owner.safe=true;
    owner.fail_check=2;rejected(NativeReadResult::stale);owner.fail_check=0;
    owner.fail_check=4;rejected(NativeReadResult::stale);owner.fail_check=0;
    owner.throw_check=2;rejected(NativeReadResult::exception);owner.throw_check=0;
    auto velocity=get<float>(first,0x30);owner.mutate_check=2;
    rejected(NativeReadResult::stale);owner.mutate_check=0;set(first,0x30,velocity);
    set(first,0x28,(type&~255u)|4u);rejected(NativeReadResult::unsupported);set(first,0x28,type);
    auto old_forward=get<int>(pattern_fixture_program(),0x12a8+16*4);
    set(pattern_fixture_program(),0x12a8+16*4,53);rejected(NativeReadResult::invalid);set(pattern_fixture_program(),0x12a8+16*4,old_forward);
    auto project_vptr=get<std::uintptr_t>(pattern_fixture_project(),0);
    set<std::uintptr_t>(pattern_fixture_project(),0,0);rejected(NativeReadResult::unsupported);set(pattern_fixture_project(),0,project_vptr);
    auto seq_vptr=get<std::uintptr_t>(pattern_fixture_sequence(0),0);
    set<std::uintptr_t>(pattern_fixture_sequence(0),0,0);rejected(NativeReadResult::unsupported);set(pattern_fixture_sequence(0),0,seq_vptr);
    auto program=get<void*>(pattern_fixture_things(),0xb8);
    set<void*>(pattern_fixture_things(),0xb8,nullptr);rejected(NativeReadResult::stale);set(pattern_fixture_things(),0xb8,program);
    auto*list=pattern_fixture_list(0);auto count=get<std::uint64_t>(list,0x88);
    set<std::uint64_t>(list,0x88,max_events+1);rejected(NativeReadResult::limit);set(list,0x88,count);
    set<std::uint64_t>(list,0x88,count-1);rejected(NativeReadResult::invalid);set(list,0x88,count);
    void*left_child=get<void*>(first,16);set(first,16,first);rejected(NativeReadResult::invalid);set(first,16,left_child);
    auto first_parent=get<void*>(first,8);set<void*>(first,8,nullptr);rejected(NativeReadResult::invalid);set(first,8,first_parent);
    auto first_header=get<void*>(list,0x78);set<void*>(list,0x78,nullptr);rejected(NativeReadResult::invalid);set(list,0x78,first_header);
    auto length=get<std::int64_t>(first,0x48);set<std::int64_t>(first,0x48,std::numeric_limits<std::int64_t>::max());
    rejected(NativeReadResult::invalid);set(first,0x48,length);
    request.sequence_index=3;rejected(NativeReadResult::invalid);request.sequence_index=0;
    /* All allocation failure points clean up the owner lease and retain the
     * previous complete snapshot. This includes failures in Snapshot::capture. */
    unsigned oom_cases=0;
    for(int fail=0;fail<128;++fail){
        auto old=snapshot;unsigned entered=owner.entered,left_count=owner.left;
        allocations_until_failure=fail;auto result=read_native(request,access,snapshot);allocations_until_failure=-1;
        assert(owner.entered==entered+1&&owner.left==left_count+1);
        if(result==NativeReadResult::ok)break;
        assert(result==NativeReadResult::allocation_failed&&snapshot==old);++oom_cases;
    }
    assert(oom_cases>8&&oom_cases<127);
    for(int index=1;index<=2;++index){request.sequence_index=index;
        assert(read_native(request,access,snapshot)==NativeReadResult::ok&&snapshot->events().empty()&&snapshot->length()==5760);}
    /* Empty map remains absent: the adapter never creates a native Clip. */
    assert(!get<void*>(pattern_fixture_sequence(2),0x638));
    assert(owner.entered==owner.left);
    std::cout<<"PASS native pattern adapter: real sequence/list/length getters, native event tree and packed Tune/Chop decoder, pad inverse maps and bus snapshot, duplicate events, content revisions, empty/absent clips, stale/unsupported/broken-tree rejection; "<<oom_cases<<" allocation failures preserve output and release owner. Executor/lifetime and notifications are fixtures; UI/XPJ/playback not connected.\n";
    return 0;
}
