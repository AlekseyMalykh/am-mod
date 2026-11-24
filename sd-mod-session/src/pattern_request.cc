#include "pattern_request.h"
#include <atomic>
#include <cstring>
#include <mutex>
#include <new>

namespace am_pattern {
struct RequestState {
    std::atomic<RequestPhase> phase{RequestPhase::queued};
    std::atomic<bool> cancel{false};
    NativeReadResult result=NativeReadResult::unavailable;
    std::mutex mutex;std::shared_ptr<const Snapshot> snapshot;
    std::shared_ptr<const ReadCurrency> currency;
};
namespace {
template<class T>T field(const void*p,std::size_t n){T v;std::memcpy(&v,static_cast<const char*>(p)+n,sizeof v);return v;}
template<class T>T fn(std::uintptr_t image,std::uintptr_t offset){return reinterpret_cast<T>(image+offset);}
struct Job {
    std::atomic<unsigned> references{1};
    std::shared_ptr<RequestState> state;
    std::shared_ptr<NativeReadOwner> owner;
    NativeReadRequest request;std::uintptr_t image=0;void*context=nullptr;
    ~Job(){
        if(state){auto expected=RequestPhase::queued;
            state->phase.compare_exchange_strong(expected,RequestPhase::cancelled,std::memory_order_acq_rel);}
        /* Never touch context here: native tag purge can outlive/free it. */
    }
};
struct Payload {void*reference;Job*job;std::uintptr_t spare;};
struct Action {std::uintptr_t invoke,manage;Payload payload;};
static_assert(sizeof(Action)==40&&sizeof(Payload)==24,"Native task ABI");
void drop(Job*p){if(p->references.fetch_sub(1,std::memory_order_acq_rel)==1)delete p;}
int manage(int mode,void*source,void*destination)noexcept{
    auto*p=static_cast<Payload*>(source);
    if(mode==0){p->job->references.fetch_add(1,std::memory_order_relaxed);std::memcpy(destination,p,sizeof *p);}
    else if(mode==1)drop(p->job);
    return mode==2?1:0;
}
struct WorkerLease {Job&job;bool active=false;explicit WorkerLease(Job&j):job(j){}};
bool enter(void*user,NativeReadContext&context){
    auto&w=*static_cast<WorkerLease*>(user);
    if(w.job.state->cancel.load(std::memory_order_acquire))return false;
    w.active=w.job.owner->enter(w.job.context,context);
    if(w.active&&(context.image!=w.job.image||context.project!=field<void*>(w.job.context,0xc8)
        ||context.things!=field<void*>(w.job.context,0x78))){
        w.job.owner->leave(w.job.context,context);w.active=false;
    }
    return w.active;
}
bool valid(void*user,const NativeReadContext&context){
    auto&w=*static_cast<WorkerLease*>(user);
    return !w.job.state->cancel.load(std::memory_order_acquire)
        &&context.project==field<void*>(w.job.context,0xc8)&&context.things==field<void*>(w.job.context,0x78)
        &&w.job.owner->valid(w.job.context,context);
}
void leave(void*user,const NativeReadContext&context){
    auto&w=*static_cast<WorkerLease*>(user);w.job.owner->leave(w.job.context,context);w.active=false;
}
void release_reference(void*reference){
    __atomic_fetch_sub(reinterpret_cast<unsigned*>(static_cast<char*>(reference)+8),1,__ATOMIC_RELEASE);
}
void run(void*data)noexcept{
    auto payload=*static_cast<Payload*>(data);Job&job=*payload.job;auto state=job.state;
    auto expected=RequestPhase::queued;
    bool running=state->phase.compare_exchange_strong(expected,RequestPhase::running,std::memory_order_acq_rel);
    NativeReadResult result=NativeReadResult::unavailable;std::shared_ptr<const Snapshot> snapshot;
    if(running){
        WorkerLease worker{job};NativeReadAccess access;access.user=&worker;access.enter=enter;access.valid=valid;access.leave=leave;
        result=read_native(job.request,access,snapshot);
    }
    /* No context/Project pointer is needed after this point. Publish terminal
     * completion only after releasing the native lifetime counter. */
    job.owner.reset();job.context=nullptr;release_reference(payload.reference);
    if(running){
        std::lock_guard<std::mutex> lock(state->mutex);
        if(state->cancel.load(std::memory_order_acquire))state->phase.store(RequestPhase::cancelled,std::memory_order_release);
        else{state->result=result;state->snapshot=std::move(snapshot);state->phase.store(RequestPhase::complete,std::memory_order_release);}
    }
    job.state.reset();
}
struct ProducerLock {
    std::uintptr_t image;void*mutex;
    ProducerLock(std::uintptr_t b,void*p):image(b),mutex(p){fn<void(*)(void*)>(image,0xf40e50)(mutex);}
    ~ProducerLock(){fn<void(*)(void*)>(image,0xf40e90)(mutex);}
};
}
RequestStatus SnapshotRequest::status()const noexcept{
    RequestStatus out;if(!state_)return out;
    out.phase=state_->phase.load(std::memory_order_acquire);
    if(out.phase==RequestPhase::complete){
        out.result=state_->result;
        if(out.result==NativeReadResult::ok&&state_->currency&&!state_->currency->current())out.result=NativeReadResult::stale;
    }
    return out;
}
void SnapshotRequest::cancel()noexcept{
    if(!state_)return;
    std::lock_guard<std::mutex> lock(state_->mutex);
    auto phase=state_->phase.load(std::memory_order_acquire);
    if(phase!=RequestPhase::queued&&phase!=RequestPhase::running)return;
    state_->cancel.store(true,std::memory_order_release);
    auto expected=RequestPhase::queued;state_->phase.compare_exchange_strong(expected,RequestPhase::cancelled,std::memory_order_acq_rel);
}
bool SnapshotRequest::take(std::shared_ptr<const Snapshot>&out)noexcept{
    if(!state_||state_->phase.load(std::memory_order_acquire)!=RequestPhase::complete)return false;
    std::lock_guard<std::mutex> lock(state_->mutex);
    if(state_->result!=NativeReadResult::ok||!state_->snapshot
       ||(state_->currency&&!state_->currency->current()))return false;
    out=std::move(state_->snapshot);return true;
}
SubmitResult enqueue_snapshot(std::uintptr_t image,void*context,const NativeReadRequest&request,
 std::shared_ptr<NativeReadOwner>owner,SnapshotRequest&out)noexcept{
    try{
        auto phase=out.status().phase;
        if(phase==RequestPhase::queued||phase==RequestPhase::running)return SubmitResult::busy;
        if(!image||!context||!owner||!request.snapshot_id||request.sequence_index<0||request.sequence_index>=1024
           ||request.sequence_key.empty()||request.sequence_key.size()>=128||request.sequence_key.find('\0')!=std::string::npos)return SubmitResult::invalid;
        if(field<std::uintptr_t>(context,0)!=image+0x6d8cd88)return SubmitResult::unsupported;
        void*queues=field<void*>(context,0x48);if(!queues)return SubmitResult::unavailable;
        auto state=std::make_shared<RequestState>();
        state->currency=owner->currency();
        if(!state->currency||!state->currency->current())return SubmitResult::unavailable;
        Job*job=new Job;
        struct Release {Job*p;~Release(){drop(p);}}release{job};
        job->state=state;job->owner=std::move(owner);job->request=request;job->image=image;job->context=context;
        void*reference=static_cast<char*>(context)+0x48;
        Action action{reinterpret_cast<std::uintptr_t>(run),reinterpret_cast<std::uintptr_t>(manage),{reference,job,0}};
        ProducerLock lock(image,static_cast<char*>(queues)+0x220);
        void*q=static_cast<char*>(queues)+0x1d8;
        if(field<unsigned char>(q,0x2d))return SubmitResult::unavailable;
        /* Reclaim completed/cancelled closures without invoking work. Otherwise
         * a full ring could stay permanently busy after its consumer drained. */
        fn<void(*)(void*)>(image,0x2b58c70)(q);
        auto count=field<std::uint64_t>(q,0),write=field<std::uint64_t>(q,0x20),reclaimed=field<std::uint64_t>(q,0x10);
        void*slots=field<void*>(q,0x30);
        if(count<2||count>65536||write>=count||reclaimed>=count||!slots)return SubmitResult::invalid;
        auto next=(write+1==count)?0:write+1;
        if(next==reclaimed||__atomic_load_n(reinterpret_cast<std::uintptr_t*>(static_cast<char*>(slots)+write*0xc0+0xb0),__ATOMIC_ACQUIRE)&3u)return SubmitResult::busy;
        __atomic_fetch_add(reinterpret_cast<unsigned*>(static_cast<char*>(context)+0x50),1,__ATOMIC_ACQUIRE);
        struct Counter {void*p;bool transferred=false;explicit Counter(void*v):p(v){}~Counter(){if(!transferred)release_reference(p);}}counter(reference);
        bool accepted=fn<int(*)(void*,void*,std::uintptr_t)>(image,0x2b59300)(q,&action,reinterpret_cast<std::uintptr_t>(reference)&~std::uintptr_t(3))&255;
        if(!accepted)return SubmitResult::unavailable;
        counter.transferred=true;
        out.state_=std::move(state);return SubmitResult::accepted;
    }catch(const std::bad_alloc&){return SubmitResult::allocation_failed;}
    catch(...){return SubmitResult::exception;}
}
}
