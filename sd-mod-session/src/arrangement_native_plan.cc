#include "arrangement_native_plan.h"
#include <cstring>
namespace am_pattern {
namespace {
bool identity(const std::string&key){return !key.empty()&&key.size()<128&&key.find('\0')==std::string::npos;}
bool same(const NativeDestination&a,const NativeDestination&b){
    return a.project_epoch==b.project_epoch&&a.routing_revision==b.routing_revision
        &&a.destination_revision==b.destination_revision&&a.program_key==b.program_key
        &&a.sequence_ticks==b.sequence_ticks&&a.transport_valid==b.transport_valid
        &&a.playing==b.playing&&a.recording==b.recording
        &&a.pad_to_note==b.pad_to_note&&a.note_to_pad==b.note_to_pad;
}
}
bool NativeArrangementPlan::matches(const PreparedArrangement&prepared,const NativeDestination&now)const{
    return now.transport_valid&&!now.playing&&!now.recording
        &&prepared.project_epoch==destination.project_epoch&&prepared.length==destination.sequence_ticks
        &&prepared.arrangement_revision==arrangement_revision&&prepared.library_revision==library_revision
        &&same(destination,now);
}
NativePlanResult prepare_native_plan(const PreparedArrangement&prepared,const NativeDestination&destination,
                                    am_midi_choice_get get,NativeArrangementPlan&out){
    if(!prepared.project_epoch||!prepared.arrangement_revision||!prepared.library_revision
       ||prepared.length<=0||prepared.length>library_tick_limit||!destination.project_epoch
       ||!destination.routing_revision||!destination.destination_revision||!identity(destination.program_key)
       ||destination.sequence_ticks<=0||destination.sequence_ticks>library_tick_limit)return NativePlanResult::invalid;
    if(prepared.notes.size()>max_events||prepared.targets.size()>max_routes)return NativePlanResult::limit;
    if(prepared.project_epoch!=destination.project_epoch)return NativePlanResult::stale;
    if(!destination.transport_valid)return NativePlanResult::transport_unavailable;
    if(destination.playing||destination.recording)return NativePlanResult::transport_running;
    if(prepared.length!=destination.sequence_ticks)return NativePlanResult::length_mismatch;
    for(unsigned i=0;i<128;++i)if(destination.pad_to_note[i]<-1||destination.pad_to_note[i]>127
        ||destination.note_to_pad[i]<-1||destination.note_to_pad[i]>127)return NativePlanResult::invalid;
    for(const auto&t:prepared.targets){
        if(t.pad>=128||t.source_note>=128)return NativePlanResult::invalid;
        if(!destination.any_program){
            if(t.program_key==unassigned_program_key)return NativePlanResult::program_mismatch;
            if(!identity(t.program_key))return NativePlanResult::invalid;
            if(t.program_key!=destination.program_key)return NativePlanResult::program_mismatch;
        }
        int note=destination.pad_to_note[t.pad];
        if(note<0||destination.note_to_pad[(unsigned)note]!=int(t.pad))return NativePlanResult::mapping_unavailable;
    }
    std::int64_t previous=-1;
    /* Validate every value before allocating the result. A forged tag-4 event
     * must never enter a native tree with refcounted payload ownership. */
    for(const auto&n:prepared.notes){
        if(n.target>=prepared.targets.size()||!n.track||!n.clip||!n.pattern||!n.event
           ||n.native.time<0||n.native.time>=prepared.length||n.native.time<previous)return NativePlanResult::invalid;
        am_midi_event event{};
        if(!am_midi_event_copy(&n.native,get,AM_MIDI_EVENT_NOTE_ON,&event)
           ||event.duration<0||event.duration>INT64_MAX-n.native.time
           ||event.source_note!=prepared.targets[n.target].source_note)return NativePlanResult::invalid;
        previous=n.native.time;
    }
    NativeArrangementPlan result;result.destination=destination;
    result.arrangement_revision=prepared.arrangement_revision;result.library_revision=prepared.library_revision;
    result.notes.reserve(prepared.notes.size());
    for(const auto&n:prepared.notes){
        const auto&t=prepared.targets[n.target];NativeBoundNote note;
        note.native=n.native;note.native.type&=~0x100u;
        note.native.data[32]=(unsigned char)destination.pad_to_note[t.pad];
        note.track=n.track;note.clip=n.clip;note.pattern=n.pattern;note.event=n.event;
        note.cycle=n.cycle;note.pad=t.pad;note.source_note=t.source_note;result.notes.push_back(note);
    }
    out=std::move(result);return NativePlanResult::ok;
}
}
