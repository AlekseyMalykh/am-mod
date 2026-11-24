#include "src/pattern_lifecycle.h"
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>
extern "C" {
void pattern_lifecycle_call_load(void);int pattern_lifecycle_call_read(void);void pattern_lifecycle_call_new(int*);
unsigned pattern_lifecycle_loads(void);unsigned pattern_lifecycle_reads(void);unsigned pattern_lifecycle_news(void);int*pattern_lifecycle_last_choice(void);
}
using namespace am_pattern;
#ifdef MPC_PATTERN_SOURCE_TRIAL
static const bool trial=true;
#else
static const bool trial=false;
#endif
static bool expected_closed;
extern "C" void pattern_lifecycle_original_probe(unsigned){assert(!lifecycle_test_domain().owner()==expected_closed);}
template<class T>void set(void*p,std::size_t n,T v){std::memcpy(static_cast<char*>(p)+n,&v,sizeof v);}
int main(){
    auto&domain=lifecycle_test_domain();assert(!domain.owner());
    std::vector<std::uint64_t>project(0x1300/8),things(0x400/8),program(0x2900/8),context(0x100/8);
    SourceBinding binding;binding.image=0x10000000;binding.project=project.data();binding.things=things.data();binding.program=program.data();
    set<std::uintptr_t>(project.data(),0,binding.image+0x6ecc648);set<void*>(things.data(),0xb8,program.data());
    set<void*>(context.data(),0xc8,project.data());set<void*>(context.data(),0x78,things.data());
    assert(domain.publish(binding,domain.epoch()));auto owner=domain.owner();auto epoch=domain.epoch();
    expected_closed=trial;pattern_lifecycle_call_load();assert(pattern_lifecycle_loads()==1);
    assert((domain.epoch()!=epoch)==trial&&owner->currency()->current()!=trial);
    if(trial)assert(!domain.owner()&&domain.publish(binding,domain.epoch()));
    owner=domain.owner();epoch=domain.epoch();assert(pattern_lifecycle_call_read()==7);
    assert(pattern_lifecycle_reads()==1&&pattern_lifecycle_loads()==2);
    assert(domain.epoch()==epoch+(trial?2:0));
    if(trial)assert(!domain.owner()&&domain.publish(binding,domain.epoch()));
    owner=domain.owner();epoch=domain.epoch();int cancel=0,confirm=1;
    expected_closed=false;pattern_lifecycle_call_new(&cancel);
    assert(pattern_lifecycle_news()==1&&pattern_lifecycle_last_choice()==&cancel&&domain.epoch()==epoch&&owner->currency()->current());
    expected_closed=trial;pattern_lifecycle_call_new(&confirm);
    assert(pattern_lifecycle_news()==2&&pattern_lifecycle_last_choice()==&confirm);
    assert((domain.epoch()!=epoch)==trial);
    if(trial)assert(!domain.owner()&&domain.publish(binding,domain.epoch()));
    if(trial){
        owner=domain.owner();NativeReadContext lease;assert(owner->enter(context.data(),lease));
        unsigned calls=pattern_lifecycle_loads();std::atomic<bool>done{false};
        std::thread writer([&]{pattern_lifecycle_call_load();done.store(true);});
        auto end=std::chrono::steady_clock::now()+std::chrono::seconds(5);
        while(owner->currency()->current()){assert(std::chrono::steady_clock::now()<end);std::this_thread::yield();}
        assert(!done.load()&&pattern_lifecycle_loads()==calls);
        owner->leave(context.data(),lease);writer.join();
        assert(done.load()&&pattern_lifecycle_loads()==calls+1&&!domain.owner());
    }
    std::cout<<"PASS phone lifecycle "<<(trial?"trial":"baseline")<<": actual shared callback include preserves arguments/composition/Read result; "
        <<(trial?"Load and confirmed New close source, nested Read/Load keep closed, active lease drains before original callback":"flag-off callbacks remain original and never touch source epoch")
        <<". Native workflow callbacks are fixtures, full application not built.\n";
}
