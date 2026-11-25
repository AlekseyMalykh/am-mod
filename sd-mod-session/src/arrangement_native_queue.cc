#include "arrangement_native_queue.h"
#include <cstring>
#include <new>
#include <atomic>
namespace am_pattern {
struct NativeQueueState {
    std::atomic<NativeQueuePhase>phase{NativeQueuePhase::pending};
    NativeDestination destination;
    NativePlaybackOriginIndex origins;
};
struct NativeQueueAccess {
    static void assign(NativeQueueReceipt&out,std::shared_ptr<NativeQueueState>state){out.state_=std::move(state);}
};
namespace {
template<class T>T field(const void*p,unsigned o){T v;std::memcpy(&v,static_cast<const char*>(p)+o,sizeof v);return v;}
template<class T>T fn(std::uintptr_t image,std::uintptr_t o){return reinterpret_cast<T>(image+o);}
struct OwnerExit {NativeCacheOwner&owner;~OwnerExit(){owner.leave();}};
struct ProducerLock {
    std::uintptr_t image;void*mutex;
    ProducerLock(std::uintptr_t b,void*p):image(b),mutex(p){fn<void(*)(void*)>(image,0xf40e50)(mutex);}
    ~ProducerLock(){fn<void(*)(void*)>(image,0xf40e90)(mutex);}
};
bool compatible(std::uintptr_t image){
    const std::uint32_t post[]={0xd503233f,0xa9bb7bfd,0x910003fd,0xa90153f3,0x91008014};
    const std::uint32_t copy[]={0xd503233f,0xd10443ff,0xa90c7bfd,0x910303fd,0xa90e5bf5};
    const std::uint32_t swap[]={0xd503233f,0xa9bc7bfd,0x910003fd,0xa9025bf5,0xf9400815};
    return NativePlaybackMaterial::compatible(image)
        &&!std::memcmp(reinterpret_cast<const void*>(image+0x2b59300),post,sizeof post)
        &&!std::memcmp(reinterpret_cast<const void*>(image+0x170b060),copy,sizeof copy)
        &&!std::memcmp(reinterpret_cast<const void*>(image+0x17088c0),swap,sizeof swap);
}
bool same_destination(const NativeDestination&a,const NativeDestination&b){
    return a.project_epoch==b.project_epoch&&a.routing_revision==b.routing_revision
        &&a.destination_revision==b.destination_revision&&a.program_key==b.program_key
        &&a.sequence_ticks==b.sequence_ticks
        &&a.pad_to_note==b.pad_to_note&&a.note_to_pad==b.note_to_pad;
}
struct SwapJob {
    std::atomic<unsigned>references{1};
    std::shared_ptr<NativeQueueState>state;
    std::uintptr_t image=0;void*cache=nullptr,*control=nullptr,*events=nullptr;
    bool constructed=false;
    ~SwapJob(){
        if(state){auto p=NativeQueuePhase::pending;state->phase.compare_exchange_strong(p,NativeQueuePhase::cancelled,std::memory_order_acq_rel);}
        if(events){
            if(constructed)fn<void(*)(void*)>(image,0x14c4ba0)(events);
            else fn<void(*)(void*,std::size_t)>(image,0xb51408)(events,0x160);
        }
        if(control)fn<void(*)(void*)>(image,0x13bb720)(control);
    }
};
void drop(SwapJob*p)noexcept{if(p->references.fetch_sub(1,std::memory_order_acq_rel)==1)delete p;}
struct SwapPayload {SwapJob*job;std::uintptr_t spare[2];};
struct SwapAction {std::uintptr_t invoke,manage;SwapPayload payload;};
static_assert(sizeof(SwapAction)==40,"Pinned native queue closure ABI");
int manage_swap(int mode,void*source,void*destination)noexcept{
    auto*p=static_cast<SwapPayload*>(source);
    if(mode==0){p->job->references.fetch_add(1,std::memory_order_relaxed);std::memcpy(destination,p,sizeof *p);}
    else if(mode==1)drop(p->job);
    return mode==2?1:0;
}
bool pin(void*control)noexcept{
    auto*count=reinterpret_cast<unsigned*>(static_cast<unsigned char*>(control)+8);
    unsigned n=__atomic_load_n(count,__ATOMIC_ACQUIRE);
    while(n&&n!=~0u){if(__atomic_compare_exchange_n(count,&n,n+1,true,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE))return true;}
    return false;
}
void run_swap(void*payload)noexcept{
    auto&job=*static_cast<SwapPayload*>(payload)->job;
    auto p=NativeQueuePhase::pending;
    if(!job.state->phase.compare_exchange_strong(p,NativeQueuePhase::running,std::memory_order_acq_rel))return;
    if(!pin(job.control)){job.state->phase.store(NativeQueuePhase::cancelled,std::memory_order_release);return;}
    /* Our strong pin prevents stock's nested weak lock from expiring between
     * the decision and the actual swap. This pins CACHE only, not Track/Program.
     * The job's weak ref prevents control-block delete on the consumer. */
    const void*args[]={job.events,job.cache,job.control};
    fn<void(*)(const void*)>(job.image,0x17088c0)(args);
    fn<void(*)(void*)>(job.image,0x123bca0)(job.control);
    p=NativeQueuePhase::running;
    job.state->phase.compare_exchange_strong(p,NativeQueuePhase::applied,std::memory_order_acq_rel);
    // Job/state/retired tree remain queue-owned until control-side reclaim.
}
void submit_confirmed(const NativeCacheBinding&binding,const NativePlaybackMaterial*source,
    const NativeDestination&expected,NativeQueueReceipt&out){
    auto state=std::make_shared<NativeQueueState>();state->destination=expected;
    SwapJob*job=new SwapJob;
    struct Release {SwapJob*p;~Release(){drop(p);}}release{job};
    job->state=state;job->image=binding.image;job->cache=binding.cache;
    auto image=binding.image;
    job->events=fn<void*(*)(std::size_t)>(image,0xb51418)(0x160);
    fn<void(*)(void*)>(image,0x170a2f0)(job->events);job->constructed=true;
    if(source){
        for(const auto&note:source->plan().notes)
            if(!fn<void*(*)(void*,const void*)>(image,0x27ee5d0)(job->events,&note.native))throw std::bad_alloc();
        auto result=NativePlaybackOriginIndex::prepare(*source,job->events,state->origins);
        if(result==NativeMaterialResult::allocation_failed)throw std::bad_alloc();
        if(result!=NativeMaterialResult::ok)throw result;
    }
    job->control=field<void*>(binding.cache,0x30);
    __atomic_fetch_add(reinterpret_cast<unsigned*>(static_cast<unsigned char*>(job->control)+12),1u,__ATOMIC_ACQ_REL);
    SwapAction action{reinterpret_cast<std::uintptr_t>(run_swap),reinterpret_cast<std::uintptr_t>(manage_swap),{job,{0,0}}};
    if(!(fn<int(*)(void*,void*,std::uintptr_t)>(image,0x2b59300)
        (static_cast<unsigned char*>(binding.queues)+0x1d8,&action,0)&255))throw NativeQueueResult::unavailable;
    NativeQueueAccess::assign(out,std::move(state));
}
}
static NativeQueueResult submit(NativeCacheOwner&owner,const NativePlaybackMaterial*source,const NativeDestination&expected,NativeQueueReceipt*out=nullptr)noexcept{
    try{
        if(out&&(out->phase()==NativeQueuePhase::pending||out->phase()==NativeQueuePhase::running))return NativeQueueResult::busy;
        if(!expected.project_epoch||!expected.routing_revision||!expected.destination_revision
            ||expected.sequence_ticks<=0||expected.sequence_ticks>library_tick_limit
            ||expected.program_key.empty()||expected.program_key.size()>=128
            ||expected.program_key==unassigned_program_key||expected.program_key.find('\0')!=std::string::npos)return NativeQueueResult::invalid;
        NativeCacheBinding binding;if(!owner.enter(binding))return NativeQueueResult::unavailable;
        OwnerExit exit{owner};auto image=binding.image;auto*cache=static_cast<unsigned char*>(binding.cache);
        auto*queues=static_cast<unsigned char*>(binding.queues);
        if(!image||!cache||!queues)return NativeQueueResult::invalid;
        if(!compatible(image))return NativeQueueResult::unsupported;
        if(field<std::uintptr_t>(cache,0)!=image+0x6d906c8
           ||field<void*>(cache,8)!=queues+0x1d8||field<void*>(cache,0x90)!=queues
           ||field<void*>(cache,0x28)!=cache||!field<void*>(cache,0x30))return NativeQueueResult::unsupported;
        auto*control=field<unsigned char*>(cache,0x30);
        if(!__atomic_load_n(reinterpret_cast<unsigned*>(control+8),__ATOMIC_ACQUIRE))return NativeQueueResult::unavailable;
        if(source&&(!source->events()||field<std::uintptr_t>(source->events(),0)!=image+0x6d8c0e8))return NativeQueueResult::invalid;
        if(!same_destination(expected,binding.destination))return NativeQueueResult::stale;
        if(source&&(expected.transport_valid!=binding.destination.transport_valid
            ||expected.playing!=binding.destination.playing||expected.recording!=binding.destination.recording))return NativeQueueResult::stale;
        ProducerLock lock(image,queues+0x220);auto*q=queues+0x1d8;
        if(field<unsigned char>(q,0x2d))return NativeQueueResult::unavailable;
        // Same native reclamation/admission protocol as pattern_request.cc.
        fn<void(*)(void*)>(image,0x2b58c70)(q);
        auto count=field<std::uint64_t>(q,0),write=field<std::uint64_t>(q,0x20),read=field<std::uint64_t>(q,0x10);
        auto*slots=field<unsigned char*>(q,0x30);
        if(count<2||count>65536||write>=count||read>=count||!slots)return NativeQueueResult::invalid;
        if((write+1==count?0:write+1)==read
           ||(__atomic_load_n(reinterpret_cast<std::uintptr_t*>(slots+write*0xc0+0xb0),__ATOMIC_ACQUIRE)&3u))return NativeQueueResult::busy;
        if(out){submit_confirmed(binding,source,expected,*out);return NativeQueueResult::ok;}
        const void*events=source?source->events():nullptr;
        const void*pair[]={events,events?field<const void*>(events,0x150):nullptr};
        fn<void(*)(void*,const void*)>(image,0x170b060)(cache,pair);
        return NativeQueueResult::ok;
    }catch(const NativeQueueResult&result){return result;}
    catch(const std::bad_alloc&){return NativeQueueResult::allocation_failed;}
    catch(...){return NativeQueueResult::native_failure;}
}
NativeQueueResult publish_native_cache(NativeCacheOwner&owner,const NativePlaybackMaterial&source)noexcept{
    return submit(owner,&source,source.plan().destination);
}
NativeQueueResult clear_native_cache(NativeCacheOwner&owner,const NativeDestination&expected)noexcept{
    return submit(owner,nullptr,expected);
}
NativeQueuePhase NativeQueueReceipt::phase()const noexcept{return state_?state_->phase.load(std::memory_order_acquire):NativeQueuePhase::empty;}
void NativeQueueReceipt::cancel()noexcept{if(state_)state_->phase.store(NativeQueuePhase::cancelled,std::memory_order_release);}
const NativeDestination*NativeQueueReceipt::destination()const noexcept{return state_?&state_->destination:nullptr;}
const NativePlaybackOriginIndex*NativeQueueReceipt::origins()const noexcept{return state_?&state_->origins:nullptr;}
NativeQueueResult publish_native_cache(NativeCacheOwner&owner,const NativePlaybackMaterial&source,NativeQueueReceipt&out)noexcept{
    return submit(owner,&source,source.plan().destination,&out);
}
NativeQueueResult clear_native_cache(NativeCacheOwner&owner,const NativeDestination&expected,NativeQueueReceipt&out)noexcept{
    return submit(owner,nullptr,expected,&out);
}
}
