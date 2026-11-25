#include "arrangement_native_material.h"
#include <algorithm>
#include <cstring>
#include <new>
namespace am_pattern {
static_assert(sizeof(void*)==8&&sizeof(step_event)==56,"Pinned native event ABI");
namespace {
template<class T>T fn(std::uintptr_t image,std::uintptr_t offset){return reinterpret_cast<T>(image+offset);}
template<class T>T field(const void*p,unsigned offset){T out;std::memcpy(&out,static_cast<const char*>(p)+offset,sizeof out);return out;}
bool valid(const NativeArrangementPlan&p,am_midi_choice_get get){
    const auto&d=p.destination;
    if(!p.arrangement_revision||!p.library_revision||!d.project_epoch||!d.routing_revision||!d.destination_revision
       ||!d.transport_valid||d.playing||d.recording||d.sequence_ticks<=0||d.sequence_ticks>library_tick_limit
       ||d.program_key.empty()||d.program_key.size()>=128||d.program_key.find('\0')!=std::string::npos
       ||d.program_key==unassigned_program_key||p.notes.size()>max_events)return false;
    for(unsigned i=0;i<128;++i)if(d.pad_to_note[i]<-1||d.pad_to_note[i]>127||d.note_to_pad[i]<-1||d.note_to_pad[i]>127)return false;
    std::int64_t previous=-1;
    for(const auto&n:p.notes){
        am_midi_event decoded{};
        if(!n.track||!n.clip||!n.pattern||!n.event||n.pad>=128||n.source_note>=128
           ||(n.native.type&0x100u)||n.native.time<previous||n.native.time<0||n.native.time>=d.sequence_ticks
           ||!am_midi_event_copy(&n.native,get,AM_MIDI_EVENT_NOTE_ON,&decoded)
           ||decoded.duration<0||decoded.duration>INT64_MAX-n.native.time
           ||d.pad_to_note[n.pad]<0||decoded.source_note!=unsigned(d.pad_to_note[n.pad])
           ||d.note_to_pad[decoded.source_note]!=int(n.pad))return false;
        previous=n.native.time;
    }
    return true;
}
}
bool NativePlaybackMaterial::compatible(std::uintptr_t image)noexcept{
    if(!image)return false;
    const std::uint32_t ctor[]={0xd503245f,0x4f000400,0x9101a003,0x91006002,0x91004001,0x9102a004,0xb900081f,0xa9010402};
    const std::uint32_t dtor[]={0xd503233f,0xd10203ff,0xd0022e61,0x91330021};
    const std::uint32_t deleting[]={0xd503233f,0xa9be7bfd,0x910003fd,0xf9000bf3,0xaa0003f3,0x97fffeaf,0xaa1303e0,0xd2802901};
    const std::uint32_t insert[]={0xd503233f,0xd10383ff,0xd0024ba2,0xf941cc42};
    const std::uint32_t holder_ctor[]={0xd503233f,0xa9bd7bfd,0x910003fd,0xa90153f3,0xaa0003f3,0xf90013f5,0x91052275,0x94438385};
    const std::uint32_t holder_dtor[]={0xd503233f,0xa9be7bfd,0x9002c641,0x9103a021,0x910003fd,0xa90153f3};
    const std::uint32_t holder_size=0xd2802c01;
    return !std::memcmp(reinterpret_cast<const void*>(image+0x27eb120),ctor,sizeof ctor)
        &&!std::memcmp(reinterpret_cast<const void*>(image+0x27edba0),dtor,sizeof dtor)
        &&!std::memcmp(reinterpret_cast<const void*>(image+0x27ee0d0),deleting,sizeof deleting)
        &&!std::memcmp(reinterpret_cast<const void*>(image+0x27ee5d0),insert,sizeof insert)
        &&field<std::uintptr_t>(reinterpret_cast<const void*>(image+0x6dbbcc0),0)==image+0x27edba0
        &&field<std::uintptr_t>(reinterpret_cast<const void*>(image+0x6dbbcc0),8)==image+0x27ee0d0
        &&!std::memcmp(reinterpret_cast<const void*>(image+0x170a2f0),holder_ctor,sizeof holder_ctor)
        &&!std::memcmp(reinterpret_cast<const void*>(image+0x14c4a20),holder_dtor,sizeof holder_dtor)
        &&!std::memcmp(reinterpret_cast<const void*>(image+0x14c4c88),&holder_size,sizeof holder_size)
        &&field<std::uintptr_t>(reinterpret_cast<const void*>(image+0x6d8c0e8),0)==image+0x14c4a20
        &&field<std::uintptr_t>(reinterpret_cast<const void*>(image+0x6d8c0e8),8)==image+0x14c4ba0;
}
NativePlaybackMaterial::~NativePlaybackMaterial(){
    if(constructed_)fn<void(*)(void*)>(image_,0x14c4a20)(storage_.data());
}
const NativeBoundNote*NativePlaybackMaterial::origin(const void*native_event)const noexcept{
    const auto key=reinterpret_cast<std::uintptr_t>(native_event);
    auto found=std::lower_bound(links_.begin(),links_.end(),key,[](const Link&a,std::uintptr_t b){return a.event<b;});
    return found!=links_.end()&&found->event==key?&plan_.notes[found->note]:nullptr;
}
NativeMaterialResult NativePlaybackMaterial::prepare(std::uintptr_t image,const NativeArrangementPlan&plan,
    std::shared_ptr<const NativePlaybackMaterial>&out)noexcept{
    try{
        if(!compatible(image))return NativeMaterialResult::unsupported;
        if(!valid(plan,fn<am_midi_choice_get>(image,0x27e4230)))return NativeMaterialResult::invalid;
        std::shared_ptr<NativePlaybackMaterial>value(new NativePlaybackMaterial(image,plan));
        value->links_.reserve(plan.notes.size());
        auto*events=value->storage_.data();
        fn<void(*)(void*)>(image,0x170a2f0)(events);value->constructed_=true;
        for(unsigned i=0;i<value->plan_.notes.size();++i){
            auto*node=fn<void*(*)(void*,const void*)>(image,0x27ee5d0)(events,&value->plan_.notes[i].native);
            if(!node||node==events+0x68)return NativeMaterialResult::native_failure;
            Link link{reinterpret_cast<std::uintptr_t>(node)+0x20,i};value->links_.push_back(link);
        }
        if(field<std::uint64_t>(events,0x88)!=plan.notes.size())return NativeMaterialResult::native_failure;
        std::sort(value->links_.begin(),value->links_.end(),[](const Link&a,const Link&b){return a.event<b.event;});
        for(unsigned i=1;i<value->links_.size();++i)if(value->links_[i-1].event==value->links_[i].event)return NativeMaterialResult::native_failure;
        out=std::move(value);return NativeMaterialResult::ok;
    }catch(const std::bad_alloc&){return NativeMaterialResult::allocation_failed;}
    catch(...){return NativeMaterialResult::native_failure;}
}
const NativeBoundNote*NativePlaybackOriginIndex::origin(const void*event)const noexcept{
    auto key=reinterpret_cast<std::uintptr_t>(event);
    auto found=std::lower_bound(links_.begin(),links_.end(),key,[](const Link&a,std::uintptr_t b){return a.event<b;});
    return found!=links_.end()&&found->event==key?&plan_.notes[found->note]:nullptr;
}
NativeMaterialResult NativePlaybackOriginIndex::prepare(const NativePlaybackMaterial&source,
    const void*leased_copy,NativePlaybackOriginIndex&out)noexcept{
    try{
        if(!source.constructed_||!leased_copy||leased_copy==source.events())return NativeMaterialResult::invalid;
        if(!NativePlaybackMaterial::compatible(source.image_))return NativeMaterialResult::unsupported;
        const auto*copy=static_cast<const unsigned char*>(leased_copy);const auto*original=source.storage_.data();
        auto count=source.plan_.notes.size();
        if(count>max_events||field<std::uintptr_t>(copy,0)!=source.image_+0x6d8c0e8
           ||field<std::uint64_t>(copy,0x88)!=count||field<std::uint64_t>(original,0x88)!=count)return NativeMaterialResult::invalid;
        NativePlaybackOriginIndex value;value.plan_=source.plan_;value.links_.reserve(count);
        const void*a=field<const void*>(original,0x78),*b=field<const void*>(copy,0x78);
        const void*ae=original+0x68,*be=copy+0x68;
        auto next=fn<const void*(*)(const void*)>(source.image_,0xb51738);
        /* Stock RB-tree copy preserves in-order occurrence identity, including
         * equal keys. Do not re-sort values or collapse identical notes: those
         * may belong to different musical tracks/clips. ABI padding is unused. */
        for(unsigned i=0;i<count;++i){
            if(!a||!b||a==ae||b==be)return NativeMaterialResult::invalid;
            auto*event=static_cast<const unsigned char*>(a)+0x20;
            auto*copied=static_cast<const unsigned char*>(b)+0x20;
            auto note=source.origin(event);if(!note)return NativeMaterialResult::invalid;
            auto c=field<step_event>(copied,0);
            if(c.time!=note->native.time||c.type!=note->native.type||std::memcmp(c.data,note->native.data,37))return NativeMaterialResult::invalid;
            Link link{reinterpret_cast<std::uintptr_t>(copied),unsigned(note-source.plan_.notes.data())};value.links_.push_back(link);
            a=next(a);b=next(b);
        }
        if(a!=ae||b!=be)return NativeMaterialResult::invalid;
        std::sort(value.links_.begin(),value.links_.end(),[](const Link&a,const Link&b){return a.event<b.event;});
        for(unsigned i=1;i<value.links_.size();++i)if(value.links_[i-1].event==value.links_[i].event)return NativeMaterialResult::invalid;
        out=std::move(value);return NativeMaterialResult::ok;
    }catch(const std::bad_alloc&){return NativeMaterialResult::allocation_failed;}
    catch(...){return NativeMaterialResult::native_failure;}
}
}
