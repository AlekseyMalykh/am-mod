#include "arrangement_native_queue.h"
#include <cstring>
#include <new>
namespace am_pattern {
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
    return NativePlaybackMaterial::compatible(image)
        &&!std::memcmp(reinterpret_cast<const void*>(image+0x2b59300),post,sizeof post)
        &&!std::memcmp(reinterpret_cast<const void*>(image+0x170b060),copy,sizeof copy);
}
bool same_destination(const NativeDestination&a,const NativeDestination&b){
    return a.project_epoch==b.project_epoch&&a.routing_revision==b.routing_revision
        &&a.destination_revision==b.destination_revision&&a.program_key==b.program_key
        &&a.sequence_ticks==b.sequence_ticks
        &&a.pad_to_note==b.pad_to_note&&a.note_to_pad==b.note_to_pad;
}
}
static NativeQueueResult submit(NativeCacheOwner&owner,const NativePlaybackMaterial*source,const NativeDestination&expected)noexcept{
    try{
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
        const void*events=source?source->events():nullptr;
        const void*pair[]={events,events?field<const void*>(events,0x150):nullptr};
        fn<void(*)(void*,const void*)>(image,0x170b060)(cache,pair);
        return NativeQueueResult::ok;
    }catch(const std::bad_alloc&){return NativeQueueResult::allocation_failed;}
    catch(...){return NativeQueueResult::native_failure;}
}
NativeQueueResult publish_native_cache(NativeCacheOwner&owner,const NativePlaybackMaterial&source)noexcept{
    return submit(owner,&source,source.plan().destination);
}
NativeQueueResult clear_native_cache(NativeCacheOwner&owner,const NativeDestination&expected)noexcept{
    return submit(owner,nullptr,expected);
}
}
