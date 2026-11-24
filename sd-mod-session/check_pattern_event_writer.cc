#include "src/pattern_source.h"
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstring>
#include <iostream>
#include <thread>
extern "C" {
void*pattern_fixture_project(void);void*pattern_fixture_things(void);void*pattern_fixture_program(void);
void*pattern_fixture_list(unsigned);void*pattern_fixture_node(unsigned,unsigned);
void pattern_event_writer_setup(void);unsigned pattern_event_notifications(void);
std::uint64_t pattern_event_notified_size(void);
void*pattern_event_erase(unsigned);void*pattern_event_erase_key(const step_event*);void pattern_event_insert(const step_event*);
}
using namespace am_pattern;
template<class T>T field(const void*p,std::size_t n){T value;std::memcpy(&value,static_cast<const char*>(p)+n,sizeof value);return value;}
template<class T>void set(void*p,std::size_t n,T value){std::memcpy(static_cast<char*>(p)+n,&value,sizeof value);}
struct ReadBridge {
    std::shared_ptr<NativeReadOwner> owner;
    std::uint64_t context[0x100/8]{};
    NativeReadAccess access(){
        NativeReadAccess result;result.user=this;
        result.enter=[](void*p,NativeReadContext&c){auto&b=*static_cast<ReadBridge*>(p);return b.owner->enter(b.context,c);};
        result.valid=[](void*p,const NativeReadContext&c){auto&b=*static_cast<ReadBridge*>(p);return b.owner->valid(b.context,c);};
        result.leave=[](void*p,const NativeReadContext&c){auto&b=*static_cast<ReadBridge*>(p);b.owner->leave(b.context,c);};return result;
    }
};
static void publish(SourceDomain&domain,const SourceBinding&binding,ReadBridge&bridge){
    assert(domain.publish(binding,domain.epoch()));bridge.owner=domain.owner();assert(bridge.owner);
}
static bool same(const step_event&a,const step_event&b){return a.time==b.time&&a.type==b.type&&!std::memcmp(a.data,b.data,37);}
static step_event note(unsigned ordinal){step_event value;std::memcpy(&value,static_cast<char*>(pattern_fixture_node(0,ordinal))+0x20,sizeof value);return value;}
extern "C" int pattern_event_writer_proof(std::uintptr_t image){
    pattern_event_writer_setup();void*list=pattern_fixture_list(0);void*end=static_cast<char*>(list)+0x68;
    SourceBinding binding;binding.image=image;binding.project=pattern_fixture_project();binding.things=pattern_fixture_things();binding.program=pattern_fixture_program();binding.outputs.fill(0);
    SourceDomain domain;ReadBridge bridge;set(bridge.context,0xc8,binding.project);set(bridge.context,0x78,binding.things);publish(domain,binding,bridge);
    NativeReadRequest request;request.snapshot_id=1;request.sequence_key="event-writer-sequence";request.sequence_index=0;
    std::shared_ptr<const Snapshot> original;
    assert(read_native(request,bridge.access(),original)==NativeReadResult::ok&&original->events().size()==3);
    Parts parts;assert(Parts::split(original,SplitBy::pads,parts)==Result::ok);
    Pattern pattern;ExtractionInfo info;
    assert(extract(parts,2,0,7680,BoundaryPolicy::onsets_only,10,pattern,info)==Result::ok&&pattern.events().size()==2);
    const step_event copied=pattern.events()[0].native;
    const step_event first=note(0),duplicate=note(1);assert(same(duplicate,note(2)));
    void*survivor=pattern_fixture_node(0,2);const auto stale=bridge.owner;
    NativeReadContext lease;assert(bridge.owner->enter(bridge.context,lease));
    std::atomic<bool>done{false};
    std::thread writer([&]{auto change=domain.change();assert(pattern_event_erase(1)==survivor);done.store(true);});
    auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while(stale->currency()->current()){assert(std::chrono::steady_clock::now()<deadline);std::this_thread::yield();}
    assert(!done.load()&&!pattern_event_notifications()&&field<std::uint64_t>(list,0x88)==3);
    bridge.owner->leave(bridge.context,lease);writer.join();
    assert(done.load()&&field<std::uint64_t>(list,0x88)==2&&pattern_event_notifications()==1&&pattern_event_notified_size()==2);
    assert(field<std::uint64_t>(list,0x118)==1); /* Native deletion change tree. */
    assert(pattern_fixture_node(0,1)==survivor&&same(first,note(0))&&same(duplicate,note(1)));
    assert(!domain.owner()&&!stale->currency()->current());
    std::shared_ptr<const Snapshot> fresh=original;
    assert(read_native(request,bridge.access(),fresh)==NativeReadResult::unavailable&&fresh==original);
    publish(domain,binding,bridge);++request.snapshot_id;
    assert(read_native(request,bridge.access(),fresh)==NativeReadResult::ok&&fresh->events().size()==2&&fresh->revision()!=original->revision());
    assert(original->events().size()==3&&pattern.events().size()==2&&same(copied,pattern.events()[0].native));
    /* Exact-key wrapper: a missing key must not remove a neighbouring note;
     * a copied existing key removes one occurrence, even with duplicates. */
    step_event absent=duplicate;absent.time=7000;
    {auto change=domain.change();assert(pattern_event_erase_key(&absent)==end);}
    absent=duplicate;absent.data[32]=37;
    {auto change=domain.change();assert(pattern_event_erase_key(&absent)==end);}
    absent=duplicate;set(&absent,0x28,std::int64_t(241));
    {auto change=domain.change();assert(pattern_event_erase_key(&absent)==end);}
    assert(field<std::uint64_t>(list,0x88)==2&&pattern_event_notifications()==1);
    {auto change=domain.change();assert(pattern_event_erase_key(&duplicate)==end);}
    assert(field<std::uint64_t>(list,0x88)==1&&same(first,note(0))&&pattern_event_notifications()==2);
    {auto change=domain.change();assert(pattern_event_erase(0)==end);}
    assert(field<std::uint64_t>(list,0x88)==0&&pattern_event_notifications()==3&&pattern_event_notified_size()==0);
    assert(!field<void*>(list,0x70)&&field<void*>(list,0x78)==end&&field<void*>(list,0x80)==end);
    publish(domain,binding,bridge);++request.snapshot_id;
    assert(read_native(request,bridge.access(),fresh)==NativeReadResult::ok&&fresh->events().empty());

    /* Repeat with 32 identical occurrences: clear by native iterator erases,
     * not a claim that every native bulk/history/clear entry is covered. */
    {auto change=domain.change();for(unsigned i=0;i<32;++i)pattern_event_insert(&duplicate);}
    assert(field<std::uint64_t>(list,0x88)==32);
    {auto change=domain.change();pattern_event_erase_key(&duplicate);}
    assert(field<std::uint64_t>(list,0x88)==31&&same(duplicate,note(0))&&same(duplicate,note(30)));
    {auto change=domain.change();for(unsigned i=31;i;--i){
        pattern_event_erase(0);assert(field<std::uint64_t>(list,0x88)==i-1&&pattern_event_notified_size()==i-1);
    }}
    assert(field<std::uint64_t>(list,0x88)==0&&pattern_event_notifications()==67);
    assert(field<std::uint64_t>(list,0x118)==35);
    /* Native batching suppresses notifications even though nodes are freed.
     * A hook on post-change notification cannot be the lifetime fence. */
    {auto change=domain.change();set(list,0x48,unsigned(1));
        for(unsigned i=0;i<3;++i)pattern_event_insert(&duplicate);
        assert(field<std::uint64_t>(list,0x88)==3);
        for(unsigned i=0;i<3;++i)pattern_event_erase(0);
        set(list,0x48,unsigned(0));
    }
    assert(field<std::uint64_t>(list,0x88)==0&&field<std::uint64_t>(list,0x118)==38&&pattern_event_notifications()==67);
    publish(domain,binding,bridge);++request.snapshot_id;
    assert(read_native(request,bridge.access(),fresh)==NativeReadResult::ok&&fresh->events().empty());
    assert(original->events().size()==3&&same(copied,pattern.events()[0].native));
    std::cout<<"PASS native event writers: actual 27eef10 iterator erase/successor/free/change trees and 27ef360 exact-key wrapper; duplicate occurrences, missing key, last-node header reset, 32 inserts/erases, native batch flag suppresses notifications while mutations still occur; SourceDomain waits for reader, stays closed, explicit republish gives fresh snapshot; original snapshot/extracted pattern unchanged. Notifications are fixtures; other variants, native bulk/history entries, caller threads and production coverage not certified.\n";
    return 0;
}
