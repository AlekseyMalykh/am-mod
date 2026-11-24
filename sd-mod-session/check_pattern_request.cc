#include "src/pattern_request.h"
#include "src/pattern_source.h"
#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>
extern "C" {
void*pattern_request_context(void);void*pattern_request_queue(void);void pattern_request_setup(void);
int pattern_request_on_worker(void);int pattern_request_context_alive(void);unsigned pattern_request_references(void);
void pattern_request_drain(unsigned);void pattern_request_reclaim(void);void pattern_request_destroy_context(void);void pattern_request_context_reset(void);
unsigned pattern_request_active_tag(void);
}
using namespace am_pattern;
static int allocations_until_failure=-1;
void*operator new(std::size_t n){
    if(allocations_until_failure==0)throw std::bad_alloc();
    if(allocations_until_failure>0)--allocations_until_failure;
    void*p=std::malloc(n?n:1);if(!p)throw std::bad_alloc();return p;
}
void operator delete(void*p)noexcept{std::free(p);}
template<class T>T field(const void*p,std::size_t n){T v;std::memcpy(&v,static_cast<const char*>(p)+n,sizeof v);return v;}
template<class T>void set(void*p,std::size_t n,T v){std::memcpy(static_cast<char*>(p)+n,&v,sizeof v);}
struct Source {
    std::mutex mutex;std::uint64_t generation=1;bool loading=false;
    unsigned enters=0,leaves=0;bool throw_enter=false,wrong_image=false;
    std::mutex pause_mutex;std::condition_variable pause_cv;bool pause=false,inside=false,resume=false;
};
class Owner:public NativeReadOwner {
    std::shared_ptr<Source> source;std::uintptr_t image;std::uint64_t expected;
    class Stamp:public ReadCurrency {
        std::shared_ptr<Source> source;std::uint64_t expected;
    public:
        Stamp(std::shared_ptr<Source>s,std::uint64_t e):source(std::move(s)),expected(e){}
        bool current()const noexcept override{std::lock_guard<std::mutex>lock(source->mutex);return !source->loading&&source->generation==expected;}
    };
    std::shared_ptr<const ReadCurrency> stamp;
public:
    Owner(std::shared_ptr<Source>s,std::uintptr_t b,std::uint64_t e):source(std::move(s)),image(b),expected(e),stamp(std::make_shared<Stamp>(source,e)){}
    std::shared_ptr<const ReadCurrency> currency()const override{return stamp;}
    bool enter(void*context,NativeReadContext&out)override{
        assert(pattern_request_on_worker()&&pattern_request_context_alive()&&pattern_request_references());
        std::unique_lock<std::mutex> lock(source->mutex);
        if(source->loading||source->generation!=expected)return false;
        if(source->throw_enter)throw std::runtime_error("source fixture failure");
        out.image=image+(source->wrong_image?16:0);out.project=field<void*>(context,0xc8);out.things=field<void*>(context,0x78);
        out.generation=expected;out.read_safe=true;out.outputs.fill(0);out.outputs[16]=3;++source->enters;
        {
            std::unique_lock<std::mutex> pause_lock(source->pause_mutex);
            if(source->pause){source->inside=true;source->pause_cv.notify_all();
                assert(source->pause_cv.wait_for(pause_lock,std::chrono::seconds(5),[&]{return source->resume;}));}
        }
        lock.release();return true; /* Fixture source lease held until leave. */
    }
    bool valid(void*,const NativeReadContext&context)override{
        assert(pattern_request_on_worker()&&pattern_request_context_alive());
        return !source->loading&&source->generation==context.generation;
    }
    void leave(void*,const NativeReadContext&)noexcept override{
        assert(pattern_request_on_worker()&&pattern_request_context_alive());++source->leaves;source->mutex.unlock();
    }
};
extern "C" int pattern_request_proof(std::uintptr_t image){
    pattern_request_setup();auto source=std::make_shared<Source>();
    auto owner=std::make_shared<Owner>(source,image,1);std::weak_ptr<Owner> weak=owner;
    NativeReadRequest request;request.snapshot_id=101;request.sequence_key="sequence-track-scope";request.sequence_index=0;
    auto submit=[&](SnapshotRequest&task){return enqueue_snapshot(image,pattern_request_context(),request,owner,task);};
    SnapshotRequest task;std::shared_ptr<const Snapshot> result;
    assert(submit(task)==SubmitResult::accepted&&pattern_request_references()==1);
    assert(task.status().phase==RequestPhase::queued&&!source->enters&&!task.take(result));
    assert(submit(task)==SubmitResult::busy&&pattern_request_references()==1);
    owner.reset();assert(!weak.expired()); /* Queued work owns its callback object. */
    pattern_request_drain(1);
    assert(task.status().phase==RequestPhase::complete&&task.status().result==NativeReadResult::ok);
    assert(!pattern_request_references()&&weak.expired()&&source->enters==1&&source->leaves==1);
    assert(task.take(result)&&result->events().size()==3&&result->routes()[0].pad==16);
    auto previous=result;assert(!task.take(result)&&result==previous);
    std::weak_ptr<Owner> abandoned;
    {SnapshotRequest dropped;auto temporary=std::make_shared<Owner>(source,image,1);abandoned=temporary;
        assert(enqueue_snapshot(image,pattern_request_context(),request,temporary,dropped)==SubmitResult::accepted);}
    assert(!abandoned.expired());pattern_request_drain(1);
    assert(abandoned.expired()&&!pattern_request_references());
    owner=std::make_shared<Owner>(source,image,1);

    /* Cancellation before execution never touches Project/Program; runner
     * still releases its native context counter. */
    assert(submit(task)==SubmitResult::accepted);task.cancel();unsigned calls=source->enters;
    pattern_request_drain(1);assert(task.status().phase==RequestPhase::cancelled&&!pattern_request_references()&&source->enters==calls);
    assert(!task.take(result)&&result==previous);
    assert(submit(task)==SubmitResult::accepted);++source->generation;
    pattern_request_drain(1);assert(task.status().result==NativeReadResult::unavailable&&!pattern_request_references());
    --source->generation;
    assert(submit(task)==SubmitResult::accepted);source->loading=true;pattern_request_drain(1);
    assert(task.status().result==NativeReadResult::unavailable&&!pattern_request_references());source->loading=false;
    source->wrong_image=true;assert(submit(task)==SubmitResult::accepted);pattern_request_drain(1);
    assert(task.status().result==NativeReadResult::unavailable&&!pattern_request_references()&&source->enters==source->leaves);source->wrong_image=false;
    source->throw_enter=true;assert(submit(task)==SubmitResult::accepted);pattern_request_drain(1);
    assert(task.status().result==NativeReadResult::exception&&!pattern_request_references());source->throw_enter=false;
    auto*q=pattern_request_queue();set<unsigned char>(q,0x2d,1);
    auto old_status=task.status();assert(submit(task)==SubmitResult::unavailable&&!pattern_request_references()&&task.status().phase==old_status.phase);
    set<unsigned char>(q,0x2d,0);
    unsigned oom_cases=0;
    for(int fail=0;fail<32;++fail){
        auto old_phase=task.status().phase;
        allocations_until_failure=fail;auto submitted=submit(task);allocations_until_failure=-1;
        if(submitted==SubmitResult::accepted){pattern_request_drain(1);break;}
        assert(submitted==SubmitResult::allocation_failed&&!pattern_request_references()&&task.status().phase==old_phase);++oom_cases;
    }
    assert(oom_cases>=3&&oom_cases<31);

    /* Fill the real queue, including its producer-inline fallback policy.
     * Our preflight must refuse without running any read on the producer. */
    set<unsigned char>(q,0x2c,1);SnapshotRequest pending[8];calls=source->enters;
    for(unsigned i=0;i<7;++i)assert(submit(pending[i])==SubmitResult::accepted);
    assert(submit(pending[7])==SubmitResult::busy&&source->enters==calls&&pattern_request_references()==7);
    pattern_request_drain(7);assert(!pattern_request_references());
    for(unsigned i=0;i<7;++i)assert(pending[i].status().result==NativeReadResult::ok);
    /* Wrap/reclaim a small ring repeatedly: no permanent-busy dead end. */
    for(unsigned i=0;i<24;++i){SnapshotRequest next;assert(submit(next)==SubmitResult::accepted);pattern_request_drain(1);assert(next.take(result));}
    set<unsigned char>(q,0x2c,0);

    /* Concrete source domain in the actual queue/native snapshot chain. */
    {
        SourceDomain domain;SourceBinding binding;binding.image=image;
        binding.project=field<void*>(pattern_request_context(),0xc8);
        binding.things=field<void*>(pattern_request_context(),0x78);
        binding.program=field<void*>(binding.things,0xb8);binding.outputs.fill(0);
        assert(domain.publish(binding,domain.epoch()));auto bound_owner=domain.owner();
        SnapshotRequest bound;
        assert(enqueue_snapshot(image,pattern_request_context(),request,bound_owner,bound)==SubmitResult::accepted);
        pattern_request_drain(1);assert(bound.status().result==NativeReadResult::ok);
        auto before_change=result;
        {auto change=domain.change();}
        assert(bound.status().result==NativeReadResult::stale&&!bound.take(result)&&result==before_change);
        assert(enqueue_snapshot(image,pattern_request_context(),request,bound_owner,bound)==SubmitResult::unavailable);
        assert(domain.publish(binding,domain.epoch()));bound_owner=domain.owner();
        assert(enqueue_snapshot(image,pattern_request_context(),request,bound_owner,bound)==SubmitResult::accepted);
        {auto change=domain.change();} /* Project change before worker consumes On. */
        pattern_request_drain(1);assert(bound.status().result!=NativeReadResult::ok&&!pattern_request_references());
        assert(domain.publish(binding,domain.epoch()));bound_owner=domain.owner();
        assert(enqueue_snapshot(image,pattern_request_context(),request,bound_owner,bound)==SubmitResult::accepted);
        pattern_request_drain(1);assert(bound.take(result)&&result->events().size()==3);
    }

    /* Native Copy destructor cancels a queued closure WITHOUT invoking it.
     * The native destructor subtracts the purge count. Reclaimer destroys our
     * owned payload later, after context has already become invalid. */
    assert(submit(task)==SubmitResult::accepted);calls=source->enters;owner.reset();
    pattern_request_destroy_context();assert(!pattern_request_context_alive()&&!pattern_request_references()&&source->enters==calls);
    pattern_request_drain(1); /* Retire the cancelled slot without invoking it. */
    pattern_request_reclaim();assert(task.status().phase==RequestPhase::cancelled);
    pattern_request_context_reset();owner=std::make_shared<Owner>(source,image,1);

    /* Destruction while the callback is running: genuine native tag/futex
     * cancellation waits until the read lease and context counter are done. */
    source->pause=true;source->resume=false;source->inside=false;
    assert(submit(task)==SubmitResult::accepted);
    std::thread worker([]{pattern_request_drain(1);});
    {std::unique_lock<std::mutex> lock(source->pause_mutex);
        assert(source->pause_cv.wait_for(lock,std::chrono::seconds(5),[&]{return source->inside;}));}
    std::atomic<bool> destroyed{false};
    std::thread destructor([&]{pattern_request_destroy_context();destroyed.store(true);});
    auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while(pattern_request_active_tag()!=3){assert(std::chrono::steady_clock::now()<deadline);std::this_thread::yield();}
    assert(pattern_request_context_alive()&&pattern_request_references()==1&&!destroyed.load());
    task.cancel();
    {std::lock_guard<std::mutex> lock(source->pause_mutex);source->resume=true;source->pause_cv.notify_all();}
    worker.join();destructor.join();assert(destroyed.load()&&!pattern_request_context_alive()&&!pattern_request_references());
    assert(task.status().phase==RequestPhase::cancelled&&source->enters==source->leaves);
    pattern_request_reclaim();
    std::cout<<"PASS native snapshot request: actual queue/mutex/reclaim/runner and Copy destructor cancellation; owned callback lifetime, worker-only source lease, generation rejection, full-queue no-inline refusal, ring reuse, queued/running cancel and confirmed native destructor waiting; "<<oom_cases<<" submission allocation failures, no leaked context counters. Concrete SourceDomain rejects completed stale results; lifecycle mutations remain fixtures; production writer coverage not enabled.\n";
    return 0;
}
