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
void pattern_event_command_setup(const step_event*);void pattern_event_command(int);
void pattern_event_delta_setup(const step_event*,const step_event*);void pattern_event_delta(unsigned);
void pattern_history_setup(void);void*pattern_history_create(const step_event*);
int pattern_history_dispatch(const void*);void*pattern_history_transaction(void);unsigned pattern_history_dispatches(void);
int pattern_history_replay(void*,int);
void pattern_history_queue_setup(void);void*pattern_history_queue_context(void);int pattern_history_is_worker(void);
void pattern_history_queue_drain(void);void pattern_history_queue_reclaim(void);void pattern_history_queue_destroy(void);
}
using namespace am_pattern;
static std::shared_ptr<void> retained_history;
static unsigned history_entries;
extern "C" void pattern_history_retain(void*transaction,const void*shared,int index){
    assert(transaction==pattern_history_transaction()&&index==0&&!retained_history);
    retained_history=*static_cast<const std::shared_ptr<void>*>(shared);++history_entries;
}
template<class T>T field(const void*p,std::size_t n){T value;std::memcpy(&value,static_cast<const char*>(p)+n,sizeof value);return value;}
template<class T>void set(void*p,std::size_t n,T value){std::memcpy(static_cast<char*>(p)+n,&value,sizeof value);}
/* Test-only command transport. SourceDomain/context are fixture-owned until
 * all jobs are reclaimed. This is not a production writer API or ownership
 * guarantee for the command's borrowed native EventList. */
struct QueuedCommand {
    std::atomic<unsigned>references{1};std::shared_ptr<void>command;
    SourceDomain*domain;std::shared_ptr<const ReadCurrency>currency;
    void*context;std::atomic<int>*result;unsigned*destroyed;
    ~QueuedCommand(){++*destroyed;} /* Never dereference context on reclaim. */
};
struct CommandPayload {void*reference;QueuedCommand*job;std::uintptr_t spare;};
struct CommandAction {std::uintptr_t invoke,manage;CommandPayload payload;};
static_assert(sizeof(CommandAction)==40,"native closure ABI");
static void command_drop(QueuedCommand*p){if(p->references.fetch_sub(1,std::memory_order_acq_rel)==1)delete p;}
static int command_manage(int mode,void*source,void*destination){
    auto*p=static_cast<CommandPayload*>(source);
    if(mode==0){p->job->references.fetch_add(1,std::memory_order_relaxed);std::memcpy(destination,p,sizeof *p);}
    else if(mode==1)command_drop(p->job);
    return mode==2?1:0;
}
static void command_run(void*data){
    auto payload=*static_cast<CommandPayload*>(data);auto&job=*payload.job;
    assert(pattern_history_is_worker()&&field<unsigned>(job.context,0x50));
    int result=-2;
    if(job.currency->current()){
        auto before=job.domain->epoch();auto change=job.domain->change();
        if(job.domain->epoch()==before+1)result=pattern_history_dispatch(&job.command);
    }
    job.command.reset();job.context=nullptr;
    __atomic_fetch_sub(reinterpret_cast<unsigned*>(static_cast<char*>(payload.reference)+8),1,__ATOMIC_RELEASE);
    job.result->store(result,std::memory_order_release);
}
static bool command_enqueue(std::uintptr_t image,SourceDomain&domain,const std::shared_ptr<const ReadCurrency>&currency,
 const std::shared_ptr<void>&command,std::atomic<int>&result,unsigned&destroyed){
    void*context=pattern_history_queue_context(),*owner=field<void*>(context,0x48),*q=static_cast<char*>(owner)+0x1d8;
    auto*job=new QueuedCommand;job->command=command;job->domain=&domain;job->currency=currency;
    job->context=context;job->result=&result;job->destroyed=&destroyed;
    CommandAction action{reinterpret_cast<std::uintptr_t>(command_run),reinterpret_cast<std::uintptr_t>(command_manage),{static_cast<char*>(context)+0x48,job,0}};
    auto native=[&](std::uintptr_t offset){return image+offset;};
    reinterpret_cast<void(*)(void*)>(native(0xf40e50))(static_cast<char*>(owner)+0x220);
    reinterpret_cast<void(*)(void*)>(native(0x2b58c70))(q);
    auto capacity=field<std::uint64_t>(q,0),write=field<std::uint64_t>(q,0x20),reclaimed=field<std::uint64_t>(q,0x10);
    bool accepted=false;
    if(!field<unsigned char>(q,0x2d)&&capacity==4&&write<capacity&&reclaimed<capacity&&(write+1)%capacity!=reclaimed){
        auto*slot=static_cast<char*>(field<void*>(q,0x30))+write*0xc0;
        if(!(__atomic_load_n(reinterpret_cast<std::uintptr_t*>(slot+0xb0),__ATOMIC_ACQUIRE)&3u)){
            __atomic_fetch_add(reinterpret_cast<unsigned*>(static_cast<char*>(context)+0x50),1,__ATOMIC_ACQUIRE);
            accepted=reinterpret_cast<int(*)(void*,void*,std::uintptr_t)>(native(0x2b59300))(q,&action,reinterpret_cast<std::uintptr_t>(action.payload.reference)&~std::uintptr_t(3))&255;
            if(!accepted)__atomic_fetch_sub(reinterpret_cast<unsigned*>(static_cast<char*>(context)+0x50),1,__ATOMIC_RELEASE);
        }
    }
    reinterpret_cast<void(*)(void*)>(native(0xf40e90))(static_cast<char*>(owner)+0x220);
    command_drop(job);return accepted;
}
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
    {auto change=domain.change();pattern_event_insert(&first);pattern_event_insert(&duplicate);pattern_event_insert(&duplicate);}
    pattern_event_command_setup(&duplicate);pattern_event_delta_setup(&first,&duplicate);
    auto expect_snapshot=[&](unsigned count){
        publish(domain,binding,bridge);++request.snapshot_id;
        assert(read_native(request,bridge.access(),fresh)==NativeReadResult::ok&&fresh->events().size()==count);
        assert(original->events().size()==3&&pattern.events().size()==2&&same(copied,pattern.events()[0].native));
    };
    expect_snapshot(3);
    assert(bridge.owner->enter(bridge.context,lease));done.store(false);
    const auto command_owner=bridge.owner;unsigned before_notifications=pattern_event_notifications();
    std::thread command_writer([&]{auto change=domain.change();pattern_event_command(0);done.store(true);});
    deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while(command_owner->currency()->current()){assert(std::chrono::steady_clock::now()<deadline);std::this_thread::yield();}
    assert(!done.load()&&field<std::uint64_t>(list,0x88)==3&&pattern_event_notifications()==before_notifications);
    command_owner->leave(bridge.context,lease);command_writer.join();
    assert(done.load()&&!domain.owner()&&field<std::uint64_t>(list,0x88)==2&&same(duplicate,note(1)));
    expect_snapshot(2);
    {auto change=domain.change();pattern_event_command(1);}expect_snapshot(3);
    assert(same(first,note(0))&&same(duplicate,note(1))&&same(duplicate,note(2)));
    {auto change=domain.change();pattern_event_command(0);}expect_snapshot(2);
    {auto change=domain.change();pattern_event_command(1);}expect_snapshot(3);
    /* Actual native delta application, including the single-remove/add fast
     * path and the multi-event clear/restore path, not a C++ erase loop. */
    {auto change=domain.change();pattern_event_delta(2);}expect_snapshot(3);
    assert(note(2).time==4800&&note(2).data[32]==52&&same(duplicate,note(1)));
    {auto change=domain.change();pattern_event_delta(3);}expect_snapshot(3);
    assert(same(first,note(0))&&same(duplicate,note(1))&&same(duplicate,note(2)));
    for(unsigned i=0;i<4;++i){
        {auto change=domain.change();pattern_event_delta(0);}expect_snapshot(0);
        {auto change=domain.change();pattern_event_delta(1);}expect_snapshot(3);
        assert(same(first,note(0))&&same(duplicate,note(1))&&same(duplicate,note(2)));
    }
    before_notifications=pattern_event_notifications();
    {auto change=domain.change();set(list,0x48,unsigned(2));pattern_event_delta(0);
        assert(field<unsigned>(list,0x48)==2&&field<std::uint64_t>(list,0x88)==0);
        pattern_event_delta(1);assert(field<unsigned>(list,0x48)==2);
        set(list,0x48,unsigned(0));
    }
    assert(pattern_event_notifications()==before_notifications);expect_snapshot(3);
    pattern_history_setup();
    unsigned command_destroyed=0;
    std::shared_ptr<void> owned_command(pattern_history_create(&duplicate),[&](void*p){
        auto table=*static_cast<std::uintptr_t**>(p);reinterpret_cast<void(*)(void*)>(table[1])(p);++command_destroyed;
    });
    std::weak_ptr<void> command_lifetime=owned_command;
    assert(field<unsigned>(owned_command.get(),0x28)==1&&field<std::uint64_t>(list,0x88)==3);
    assert(bridge.owner->enter(bridge.context,lease));const auto dispatch_owner=bridge.owner;done.store(false);
    std::atomic<int>dispatch_result{-1};
    std::thread dispatch_writer([&]{auto change=domain.change();dispatch_result.store(pattern_history_dispatch(&owned_command));done.store(true);});
    deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while(dispatch_owner->currency()->current()){assert(std::chrono::steady_clock::now()<deadline);std::this_thread::yield();}
    assert(!done.load()&&!pattern_history_dispatches()&&!history_entries&&field<std::uint64_t>(list,0x88)==3);
    dispatch_owner->leave(bridge.context,lease);dispatch_writer.join();
    assert(done.load()&&dispatch_result.load()==0&&pattern_history_dispatches()==1&&history_entries==1);
    assert(retained_history.get()==owned_command.get()&&field<unsigned>(owned_command.get(),0x28)==0);
    owned_command.reset();assert(!command_lifetime.expired()&&!command_destroyed);expect_snapshot(2);
    {auto change=domain.change();assert(pattern_history_replay(retained_history.get(),0)!=0);}expect_snapshot(2);
    assert(field<unsigned>(retained_history.get(),0x28)==0&&history_entries==1);
    {auto change=domain.change();assert(pattern_history_replay(retained_history.get(),1)==0);}expect_snapshot(3);
    assert(field<unsigned>(retained_history.get(),0x28)==1);
    {auto change=domain.change();assert(pattern_history_replay(retained_history.get(),1)!=0);}expect_snapshot(3);
    assert(field<unsigned>(retained_history.get(),0x28)==1&&history_entries==1);
    {auto change=domain.change();assert(pattern_history_replay(retained_history.get(),0)==0);}expect_snapshot(2);
    assert(field<unsigned>(retained_history.get(),0x28)==0);
    {auto change=domain.change();assert(pattern_history_replay(retained_history.get(),1)==0);}expect_snapshot(3);
    assert(same(first,note(0))&&same(duplicate,note(1))&&same(duplicate,note(2)));
    retained_history.reset();assert(command_lifetime.expired()&&command_destroyed==1);
    pattern_history_queue_setup();unsigned jobs_destroyed=0;std::atomic<int>queued_result{-1};
    auto make_command=[&]{return std::shared_ptr<void>(pattern_history_create(&duplicate),[&](void*p){
        auto table=*static_cast<std::uintptr_t**>(p);reinterpret_cast<void(*)(void*)>(table[1])(p);++command_destroyed;
    });};
    auto queued_command=make_command();std::weak_ptr<void>queued_lifetime=queued_command;
    assert(bridge.owner->enter(bridge.context,lease));auto queued_owner=bridge.owner;
    assert(command_enqueue(image,domain,queued_owner->currency(),queued_command,queued_result,jobs_destroyed));
    queued_command.reset();assert(!queued_lifetime.expired()&&field<unsigned>(pattern_history_queue_context(),0x50)==1);
    assert(pattern_history_dispatches()==1&&field<std::uint64_t>(list,0x88)==3&&queued_result.load()==-1);
    std::thread queued_writer([]{pattern_history_queue_drain();});
    deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while(queued_owner->currency()->current()){assert(std::chrono::steady_clock::now()<deadline);std::this_thread::yield();}
    assert(queued_result.load()==-1&&pattern_history_dispatches()==1&&field<std::uint64_t>(list,0x88)==3);
    queued_owner->leave(bridge.context,lease);queued_writer.join();
    assert(queued_result.load()==0&&pattern_history_dispatches()==2&&history_entries==2);
    assert(!field<unsigned>(pattern_history_queue_context(),0x50)&&!domain.owner());
    pattern_history_queue_reclaim();assert(jobs_destroyed==1&&!queued_lifetime.expired());expect_snapshot(2);
    {auto change=domain.change();assert(pattern_history_replay(retained_history.get(),1)==0);}expect_snapshot(3);
    retained_history.reset();assert(queued_lifetime.expired()&&command_destroyed==2);

    queued_result.store(-1);queued_command=make_command();queued_lifetime=queued_command;
    assert(command_enqueue(image,domain,bridge.owner->currency(),queued_command,queued_result,jobs_destroyed));queued_command.reset();
    {auto change=domain.change();} /* Queued command's source stamp is now stale. */
    pattern_history_queue_drain();pattern_history_queue_reclaim();
    assert(queued_result.load()==-2&&queued_lifetime.expired()&&command_destroyed==3&&jobs_destroyed==2);
    assert(pattern_history_dispatches()==2&&!field<unsigned>(pattern_history_queue_context(),0x50));expect_snapshot(3);

    queued_result.store(-1);queued_command=make_command();queued_lifetime=queued_command;
    assert(command_enqueue(image,domain,bridge.owner->currency(),queued_command,queued_result,jobs_destroyed));queued_command.reset();
    assert(!queued_lifetime.expired()&&field<unsigned>(pattern_history_queue_context(),0x50)==1);
    pattern_history_queue_destroy();pattern_history_queue_reclaim();
    /* Native tag cancellation releases the context counter immediately;
     * the consumer must skip the cancelled slot before reclaim frees it. */
    assert(queued_result.load()==-1&&!queued_lifetime.expired()&&jobs_destroyed==2);
    assert(!field<unsigned>(pattern_history_queue_context(),0x50));
    pattern_history_queue_drain();pattern_history_queue_reclaim();
    assert(queued_result.load()==-1&&queued_lifetime.expired()&&command_destroyed==4&&jobs_destroyed==3);
    assert(pattern_history_dispatches()==2&&!field<unsigned>(pattern_history_queue_context(),0x50));
    assert(field<std::uint64_t>(list,0x88)==3&&same(first,note(0))&&same(duplicate,note(1))&&same(duplicate,note(2)));
    std::cout<<"PASS queued native event command: actual enqueue/mutex/runner/reclaim and Copy destructor tag cancellation; closure retains constructed command until worker/history or cancellation reclaim, worker waits for reader before dispatch, stale source skips dispatch, context counters balanced, command destructors run once. Queue/context storage, base destructor/other queue and history services are fixtures; production source writer coverage and history selection not certified.\n";
    std::cout<<"PASS native event CommandManager: real ctor19cf2b0/dispatch19d0a00/transaction19cc1f0, DeleteEvent ctor19d8ba0 mode2, command state wrappers19c9a50/19c9c40; retained history owns command after caller release, native Undo/Redo changes state and restores duplicates, native virtual destructor runs once. SourceDomain waits before dispatch. History storage/maintenance/monitor are adapters; queue, actual history selection and GUI Clear not certified.\n";
    std::cout<<"PASS native history methods/deltas: actual DeleteEvent vtable execute/undo/redo (inline fixture command; no history manager), 27f0ee0 bulk remove/restore and single replacement, duplicate notes, repeated replay, nested batch counter retained; SourceDomain drains reader before native command. Source snapshot/pattern unchanged. Factory, real history stack, GUI Clear entry, exception rollback and thread contracts not certified.\n";
    std::cout<<"PASS native event writers: actual 27eef10 iterator erase/successor/free/change trees and 27ef360 exact-key wrapper; duplicate occurrences, missing key, last-node header reset, 32 inserts/erases, native batch flag suppresses notifications while mutations still occur; SourceDomain waits for reader, stays closed, explicit republish gives fresh snapshot; original snapshot/extracted pattern unchanged. Notifications are fixtures; other variants, full bulk/history coverage, caller threads and production coverage not certified.\n";
    return 0;
}
