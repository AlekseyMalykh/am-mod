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
    return 0;
}
