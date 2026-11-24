#include "src/pattern_source.h"
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>
using namespace am_pattern;
template<class T>void set(void*p,std::size_t n,T v){std::memcpy(static_cast<char*>(p)+n,&v,sizeof v);}
struct NativeStorage {
    std::vector<std::uint64_t> project,things,program,context;
    NativeStorage():project(0x1300/8),things(0x400/8),program(0x2900/8),context(0x100/8){
        set<std::uintptr_t>(project.data(),0,0x10000000+0x6ecc648);
        set<void*>(things.data(),0xb8,program.data());set<void*>(context.data(),0xc8,project.data());set<void*>(context.data(),0x78,things.data());
    }
    SourceBinding binding(){SourceBinding b;b.image=0x10000000;b.project=project.data();b.things=things.data();b.program=program.data();b.outputs.fill(0);return b;}
};
template<class Predicate>static void until(const Predicate&predicate){
    auto end=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while(!predicate()){assert(std::chrono::steady_clock::now()<end);std::this_thread::yield();}
}
struct Load {SourceDomain*domain;NativeStorage*storage;unsigned calls=0;bool throws=false,nested=false;};
static void load(void*p,void*){
    auto&l=*static_cast<Load*>(p);++l.calls;
    /* No lock spans native work; these calls must not deadlock. */
    assert(!l.domain->owner());assert(!l.domain->publish(l.storage->binding(),l.domain->epoch()));
    if(l.throws)throw std::runtime_error("native loader fixture failure");
}
static int read(void*p,void*a){auto&l=*static_cast<Load*>(p);source_load(*l.domain,p,a,load);return 7;}
static void new_project(void*p,int*){++static_cast<Load*>(p)->calls;}
static int read_failed(void*p,void*){auto&l=*static_cast<Load*>(p);assert(!l.domain->owner());return 0;}
static void program_change(void*p,void*program,int flags){auto&l=*static_cast<Load*>(p);++l.calls;assert(!program&&flags==17&&!l.domain->owner());}
int main(){
    SourceDomain domain;NativeStorage storage;
    assert(!domain.owner());auto epoch=domain.epoch();auto old_binding=storage.binding();
    auto invalid_binding=old_binding;invalid_binding.outputs[0]=9;assert(!domain.publish(invalid_binding,epoch)&&!domain.owner());
    assert(domain.publish(old_binding,epoch));
    auto owner=domain.owner();assert(owner&&owner->currency()->current());NativeReadContext lease;
    assert(owner->enter(storage.context.data(),lease)&&owner->valid(storage.context.data(),lease));
    NativeReadContext duplicate;assert(!owner->enter(storage.context.data(),duplicate));
    bool refused=false;try{auto change=domain.change();}catch(const std::logic_error&){refused=true;}assert(refused);
    /* A writer closes admission and waits before freeing the old native data. */
    std::atomic<bool>mutated{false};
    std::thread writer([&]{auto change=domain.change();storage.project.clear();storage.project.shrink_to_fit();
        storage.things.clear();storage.things.shrink_to_fit();storage.program.clear();storage.program.shrink_to_fit();mutated.store(true);});
    until([&]{return !owner->currency()->current();});
    assert(!mutated.load()&&!owner->valid(storage.context.data(),lease)&&!domain.owner());
    /* Still live while the read lease exists, even after invalidation. */
    assert(*static_cast<const std::uintptr_t*>(lease.project)==0x10000000+0x6ecc648);
    owner->leave(storage.context.data(),lease);writer.join();assert(mutated.load());
    assert(!owner->enter(storage.context.data(),lease)); /* No deref of freed source. */
    assert(!domain.publish(old_binding,epoch)); /* Late publication must not deref freed source either. */
    NativeStorage next;assert(!domain.publish(next.binding(),epoch));
    assert(domain.publish(next.binding(),domain.epoch()));
    auto next_owner=domain.owner();assert(next_owner&&next_owner->enter(next.context.data(),lease));next_owner->leave(next.context.data(),lease);
    assert(!owner->currency()->current());

    Load loader;loader.domain=&domain;loader.storage=&next;int no=0,yes=1;
    auto before=domain.epoch();source_new(domain,&loader,&no,new_project);
    assert(domain.epoch()==before&&next_owner->currency()->current()&&loader.calls==1);
    source_new(domain,&loader,&yes,new_project);
    assert(domain.epoch()!=before&&!next_owner->currency()->current()&&!domain.owner()&&loader.calls==2);
    assert(domain.publish(next.binding(),domain.epoch()));
    auto previous=domain.owner();assert(source_read(domain,&loader,nullptr,read)==7);
    assert(!previous->currency()->current()&&!domain.owner()&&loader.calls==3);
    assert(domain.publish(next.binding(),domain.epoch()));
    loader.throws=true;bool threw=false;
    try{source_load(domain,&loader,nullptr,load);}catch(const std::runtime_error&){threw=true;}
    assert(threw&&!domain.owner());loader.throws=false;
    assert(domain.publish(next.binding(),domain.epoch()));
    previous=domain.owner();assert(source_read(domain,&loader,nullptr,read_failed)==0);
    assert(!previous->currency()->current()&&!domain.owner());assert(domain.publish(next.binding(),domain.epoch()));
    previous=domain.owner();source_program(domain,&loader,nullptr,17,program_change);
    assert(!previous->currency()->current()&&!domain.owner());
    /* Asynchronous change token remains alive past the initiating callback. */
    {auto change=domain.change();auto nested=domain.change();assert(!domain.publish(next.binding(),domain.epoch()));}
    assert(!domain.owner());assert(domain.publish(next.binding(),domain.epoch()));

    /* Retire waits for outstanding mutation tokens as well as readers. */
    auto pending=domain.change();before=domain.epoch();std::atomic<bool>retired{false};
    std::thread retiring([&]{domain.retire();retired.store(true);});
    until([&]{return domain.epoch()!=before;});assert(!retired.load());
    pending=SourceDomain::Change{};retiring.join();assert(retired.load()&&!domain.owner());
    assert(!domain.publish(next.binding(),domain.epoch()));

    std::shared_ptr<NativeReadOwner> old_owner;std::shared_ptr<const ReadCurrency> stamp;
    {SourceDomain temporary;assert(temporary.publish(next.binding(),temporary.epoch()));old_owner=temporary.owner();stamp=old_owner->currency();}
    assert(!stamp->current()&&!old_owner->enter(next.context.data(),lease));
    std::cout<<"PASS source lifetime domain: participating writer waits before freeing Project/Program, revoked owners never dereference freed memory, explicit epoch-bound rebinding, nested New/Read/Load and exceptions stay closed, cancelled New preserves epoch, Program invalidation, async tokens and retire, late currency checks contain no native access. Native lifecycle callbacks are fixtures; complete production writer coverage not enabled.\n";
}
