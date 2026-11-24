#include "src/pattern_source.h"
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>
extern "C" {
void pattern_writer_setup(void);void*pattern_writer_track(void);void*pattern_writer_program(unsigned);
void pattern_writer_clear_trace(void);unsigned pattern_writer_trace_count(void);
unsigned pattern_writer_trace_kind(unsigned);void*pattern_writer_trace_program(unsigned);int pattern_writer_trace_number(unsigned);
int pattern_writer_clip_selectors(void);
void pattern_writer_replacement_setup(void);
void pattern_writer_subtype_setup(void);void pattern_writer_subtype_select(unsigned,unsigned,unsigned);void pattern_writer_subtype_connection(int);
void pattern_writer_subtype_peer(int);
void*pattern_fixture_project(void);void*pattern_fixture_things(void);
}
using namespace am_pattern;
template<class T>T field(const void*p,std::size_t n){T v;std::memcpy(&v,static_cast<const char*>(p)+n,sizeof v);return v;}
template<class T>void set(void*p,std::size_t n,T v){std::memcpy(static_cast<char*>(p)+n,&v,sizeof v);}
static void trace(unsigned i,unsigned kind,void*program,int number){
    assert(pattern_writer_trace_kind(i)==kind&&pattern_writer_trace_program(i)==program&&pattern_writer_trace_number(i)==number);
}
extern "C" int pattern_writer_proof(std::uintptr_t image){
    assert(pattern_writer_clip_selectors());pattern_writer_setup();
    auto setter=reinterpret_cast<void(*)(void*,void*,int)>(image+0x29c9490);
    void*track=pattern_writer_track(),*old_program=pattern_writer_program(0),*next=pattern_writer_program(1);
    setter(track,next,17);assert(field<void*>(track,0xbb8)==next&&field<int>(track,0xbc0)==17);
    assert(pattern_writer_trace_count()==4);trace(0,0x30,old_program,5);trace(1,0x100,next,17);trace(2,0x101,next,17);trace(3,0x38,next,17);
    pattern_writer_clear_trace();setter(track,next,99);
    assert(!pattern_writer_trace_count()&&field<int>(track,0xbc0)==17); /* Same non-null Program is a true native no-op. */
    setter(track,nullptr,55);assert(pattern_writer_trace_count()==3&&field<void*>(track,0xbb8)==nullptr);
    trace(0,0x30,next,17);trace(1,0x100,nullptr,55);trace(2,0x38,nullptr,55);
    pattern_writer_clear_trace();setter(track,nullptr,66);
    assert(pattern_writer_trace_count()==2);trace(0,0x30,nullptr,55);trace(1,0x38,nullptr,66);
    pattern_writer_clear_trace();setter(track,old_program,5);
    assert(pattern_writer_trace_count()==3);trace(0,0x30,nullptr,66);trace(1,0x101,old_program,5);trace(2,0x38,old_program,5);

    /* Two additional real callback entries clear the pointer when the current
     * Program disappears. One receives the Track, the other Track+0x910. */
    auto disappearance=reinterpret_cast<void(*)(void*,void*)>(image+0x29c93a0);
    auto subscriber=reinterpret_cast<void(*)(void*,void*)>(image+0x29c7a80);
    pattern_writer_clear_trace();disappearance(track,next);disappearance(track,nullptr);
    assert(!pattern_writer_trace_count()&&field<void*>(track,0xbb8)==old_program);
    disappearance(track,old_program);assert(pattern_writer_trace_count()==2&&field<void*>(track,0xbb8)==nullptr);
    trace(0,0x30,old_program,5);trace(1,0x102,nullptr,61);
    setter(track,old_program,5);pattern_writer_clear_trace();
    void*listener=static_cast<char*>(track)+0x910;
    subscriber(listener,next);subscriber(listener,nullptr);
    assert(!pattern_writer_trace_count()&&field<void*>(track,0xbb8)==old_program);
    subscriber(listener,old_program);assert(pattern_writer_trace_count()==3&&field<void*>(track,0xbb8)==nullptr);
    trace(0,0x30,old_program,5);trace(1,0x100,nullptr,61);trace(2,0x38,nullptr,61);
    setter(track,old_program,5);

    SourceDomain domain;SourceBinding binding;binding.image=image;binding.project=pattern_fixture_project();binding.things=pattern_fixture_things();binding.program=old_program;
    assert(domain.publish(binding,domain.epoch()));auto owner=domain.owner();
    std::vector<std::uint64_t> context(0x100/8);set(context.data(),0xc8,binding.project);set(context.data(),0x78,binding.things);
    NativeReadContext lease;assert(owner->enter(context.data(),lease));pattern_writer_clear_trace();
    std::atomic<bool>done{false};
    std::thread writer([&]{source_program(domain,track,next,23,setter);done.store(true);});
    auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while(owner->currency()->current()){assert(std::chrono::steady_clock::now()<deadline);std::this_thread::yield();}
    assert(!done.load()&&!pattern_writer_trace_count()&&field<void*>(track,0xbb8)==old_program);
    owner->leave(context.data(),lease);writer.join();
    assert(done.load()&&field<void*>(track,0xbb8)==next&&field<int>(track,0xbc0)==23&&pattern_writer_trace_count()==4);
    assert(!domain.owner()&&!owner->currency()->current());
    /* Listener fixtures do not update CurrentThings. Actual assignment does
     * not itself prove that the full binding is ready for publication. */
    assert(field<void*>(binding.things,0xb8)==old_program);
    std::cout<<"PASS native Program setter 29c9490: pre-notify sees old pointer, listeners/post-notify see new pointer+number; same non-null assignment no-op, null detach/reattach; disappearance callbacks 29c93a0(Track) and 29c7a80(Track+910) clear only matching Program and preserve its number. SourceDomain drains reader before real native setter and stays closed. Observer/rebuild services are fixtures; CurrentThings/async completion and writer coverage not certified.\n";

    pattern_writer_replacement_setup();setter(track,old_program,5);pattern_writer_clear_trace();
    auto replacement=reinterpret_cast<void(*)(void*,void*)>(image+0x29c9670);
    replacement(track,next);
    const unsigned replace_kinds[]={0x110,0x30,0x112,0x112,0x115,0x113,0x113,0x114,0x115,0x111,0x102};
    assert(pattern_writer_trace_count()==11);
    for(unsigned i=0;i<11;++i)trace(i,replace_kinds[i],i<9?old_program:next,i<9?5:92);
    assert(field<void*>(track,0xbb8)==next&&field<int>(track,0xbc0)==92);
    /* Unlike 29c9490, equal pointers still reset, notify and rebuild. */
    pattern_writer_clear_trace();replacement(track,next);
    assert(pattern_writer_trace_count()==4);trace(0,0x110,next,92);trace(1,0x30,next,92);trace(2,0x111,next,92);trace(3,0x102,next,92);
    pattern_writer_clear_trace();replacement(track,nullptr);
    const unsigned detach_kinds[]={0x110,0x30,0x112,0x112,0x115,0x111,0x102};
    assert(pattern_writer_trace_count()==7);
    for(unsigned i=0;i<7;++i)trace(i,detach_kinds[i],i<5?next:nullptr,92);
    assert(field<void*>(track,0xbb8)==nullptr&&field<int>(track,0xbc0)==92);
    pattern_writer_clear_trace();replacement(track,nullptr);
    assert(pattern_writer_trace_count()==4);trace(0,0x110,nullptr,92);trace(1,0x30,nullptr,92);trace(2,0x111,nullptr,92);trace(3,0x102,nullptr,92);
    pattern_writer_clear_trace();replacement(track,old_program);
    const unsigned attach_kinds[]={0x110,0x30,0x113,0x113,0x114,0x115,0x111,0x102};
    assert(pattern_writer_trace_count()==8);
    for(unsigned i=0;i<8;++i)trace(i,attach_kinds[i],i<6?nullptr:old_program,i<6?92:61);
    assert(field<void*>(track,0xbb8)==old_program&&field<int>(track,0xbc0)==61);

    SourceDomain replacement_domain;
    assert(replacement_domain.publish(binding,replacement_domain.epoch()));auto replacement_owner=replacement_domain.owner();
    assert(replacement_owner->enter(context.data(),lease));pattern_writer_clear_trace();done.store(false);
    std::thread replacement_writer([&]{auto change=replacement_domain.change();replacement(track,next);done.store(true);});
    deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while(replacement_owner->currency()->current()){assert(std::chrono::steady_clock::now()<deadline);std::this_thread::yield();}
    assert(!done.load()&&!pattern_writer_trace_count()&&field<void*>(track,0xbb8)==old_program);
    replacement_owner->leave(context.data(),lease);replacement_writer.join();
    assert(done.load()&&pattern_writer_trace_count()==11&&field<void*>(track,0xbb8)==next&&field<int>(track,0xbc0)==92);
    assert(!replacement_domain.owner()&&!replacement_owner->currency()->current());
    std::cout<<"PASS native Program replacement 29c9670 base path: both stores 29c9884/29c98e0, replace/detach/reattach, equal non-null and null pointers still reset/notify/refresh/rebuild; SourceDomain blocks even initial reset until reader leaves and stays closed. RTTI, reset, bindings, refresh and rebuild are fixtures; subtype branches and production hook safety not certified.\n";

    pattern_writer_clear_trace();pattern_writer_subtype_setup();replacement(track,old_program);pattern_writer_clear_trace();
    pattern_writer_subtype_select(2,1,0);replacement(track,next);
    const unsigned subtype_attach_kinds[]={0x110,0x30,0x112,0x112,0x115,0x113,0x113,0x114,0x115,0x120,0x121,0x122,0x111,0x102};
    assert(pattern_writer_trace_count()==14);
    for(unsigned i=0;i<14;++i)trace(i,subtype_attach_kinds[i],i<12?old_program:next,i<12?61:92);
    pattern_writer_subtype_connection(1);
    pattern_writer_clear_trace();replacement(track,next);
    assert(pattern_writer_trace_count()==4);pattern_writer_subtype_connection(1);
    /* Change boolean input/output as well as Program: both branch values
     * must pass through the existing Track callback, not a hardcoded gate. */
    pattern_writer_subtype_select(3,0,1);pattern_writer_clear_trace();replacement(track,old_program);
    const unsigned subtype_replace_kinds[]={0x110,0x30,0x112,0x112,0x115,0x119,0x113,0x113,0x114,0x115,0x120,0x121,0x122,0x111,0x102};
    assert(pattern_writer_trace_count()==15);
    for(unsigned i=0;i<15;++i)trace(i,subtype_replace_kinds[i],i<13?next:old_program,i<13?92:61);
    pattern_writer_subtype_connection(0);
    pattern_writer_clear_trace();replacement(track,nullptr);
    const unsigned subtype_detach_kinds[]={0x110,0x30,0x112,0x112,0x115,0x119,0x111,0x102};
    assert(pattern_writer_trace_count()==8);
    for(unsigned i=0;i<8;++i)trace(i,subtype_detach_kinds[i],i<6?old_program:nullptr,61);
    pattern_writer_subtype_connection(-1);
    pattern_writer_clear_trace();replacement(track,nullptr);
    assert(pattern_writer_trace_count()==4);pattern_writer_subtype_connection(-1);
    pattern_writer_clear_trace();replacement(track,old_program);
    assert(pattern_writer_trace_count()==11);pattern_writer_subtype_connection(0);
    pattern_writer_subtype_peer(1);pattern_writer_clear_trace();replacement(track,next);
    assert(pattern_writer_trace_count()==15);pattern_writer_subtype_peer(0);pattern_writer_subtype_connection(1);
    pattern_writer_clear_trace();replacement(track,old_program);
    assert(pattern_writer_trace_count()==15);pattern_writer_subtype_connection(0);
    /* Move subtype->base, then back. Removing an instrument must unlink the
     * persistent Track connection even when no new subtype is attached. */
    pattern_writer_subtype_select(1,1,0);pattern_writer_clear_trace();replacement(track,next);
    assert(pattern_writer_trace_count()==12);pattern_writer_subtype_connection(-1);
    pattern_writer_clear_trace();replacement(track,old_program);
    assert(pattern_writer_trace_count()==14);pattern_writer_subtype_connection(0);

    SourceDomain subtype_domain;
    assert(subtype_domain.publish(binding,subtype_domain.epoch()));auto subtype_owner=subtype_domain.owner();
    assert(subtype_owner->enter(context.data(),lease));pattern_writer_clear_trace();done.store(false);
    pattern_writer_subtype_select(3,0,1);
    std::thread subtype_writer([&]{auto change=subtype_domain.change();replacement(track,next);done.store(true);});
    deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while(subtype_owner->currency()->current()){assert(std::chrono::steady_clock::now()<deadline);std::this_thread::yield();}
    assert(!done.load()&&!pattern_writer_trace_count()&&field<void*>(track,0xbb8)==old_program);
    pattern_writer_subtype_connection(0);
    subtype_owner->leave(context.data(),lease);subtype_writer.join();
    assert(done.load()&&pattern_writer_trace_count()==15&&field<void*>(track,0xbb8)==next&&field<int>(track,0xbc0)==92);
    pattern_writer_subtype_connection(1);
    assert(!subtype_domain.owner()&&!subtype_owner->currency()->current());
    /* Clean the final live connection using the same native detach path. */
    pattern_writer_clear_trace();replacement(track,nullptr);pattern_writer_subtype_connection(-1);
    std::cout<<"PASS native Program replacement subtype branches: base/subtype transitions, replace/detach/reattach, same pointers, both boolean values; real 236efe0/236f060 construct/assign and native stack cleanup retain the persistent Track connection, unlink old tokens and preserve an independent subscriber. SourceDomain drains reader before reset/connection changes. RTTI, token provider and notification/property services are fixtures; real subtype classes, production thread/lock contracts and coverage not certified.\n";
    return 0;
}
