#include "src/arrangement_native_plan.h"
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <new>
using namespace am_pattern;
static int fail_after=-1;
__attribute__((noinline)) void*operator new(std::size_t n){if(fail_after==0){fail_after=-1;throw std::bad_alloc();}if(fail_after>0)--fail_after;void*p=std::malloc(n?n:1);if(!p)throw std::bad_alloc();return p;}
__attribute__((noinline)) void operator delete(void*p)noexcept{std::free(p);}
void*operator new(std::size_t n,const std::nothrow_t&)noexcept{try{return ::operator new(n);}catch(...){return nullptr;}}
void operator delete(void*p,const std::nothrow_t&)noexcept{::operator delete(p);}
/* Getter seam only: these bytes are fixture values, not a native choice codec. */
static int choices(const void*p,int kind,int){const auto*b=static_cast<const unsigned char*>(p);return kind==0?int(static_cast<signed char>(b[2])):int(b[3]);}
static void add(Library&library,Id id,std::int64_t length,unsigned pad,unsigned source,std::int64_t gate){
    Route r;r.program_key="program";r.pad=pad;r.source_note=source;r.mixer_bus=int(pad%8);
    PatternEvent n;n.id=1;n.native.type=3|(7u<<12);float velocity=.73f;std::memcpy(n.native.data,&velocity,4);
    std::memcpy(n.native.data+24,&gate,8);n.native.data[32]=source;n.native.data[34]=83;n.native.data[36]=3;
    LibraryEntry entry;entry.name="pattern";assert(Pattern::restore(id,0,1,length,{r},{n},nullptr,entry.pattern)==Result::ok);
    library.entries.push_back(entry);library.next=id+1;++library.revision;
}
static std::string bytes(const NativeArrangementPlan&p){
    const auto&d=p.destination;std::string out=std::to_string(d.project_epoch)+":"+d.program_key+":"+std::to_string(d.routing_revision)+":"+std::to_string(d.destination_revision)+":"+std::to_string(d.sequence_ticks)+":"+std::to_string(d.transport_valid)+std::to_string(d.playing)+std::to_string(d.recording)+":"+std::to_string(p.arrangement_revision)+":"+std::to_string(p.library_revision);
    for(int n:d.pad_to_note)out+=":"+std::to_string(n);
    for(int n:d.note_to_pad)out+=":"+std::to_string(n);
    for(const auto&n:p.notes){out.append(reinterpret_cast<const char*>(&n.native),sizeof n.native);out+=std::to_string(n.track)+":"+std::to_string(n.clip)+":"+std::to_string(n.pattern)+":"+std::to_string(n.event)+":"+std::to_string(n.cycle)+":"+std::to_string(n.pad)+":"+std::to_string(n.source_note);}
    return out;
}
int main(){
    Library library;add(library,101,7680,0,36,120);add(library,102,15360,16,52,40000);add(library,103,30720,13,60,30720);
    const auto source=library_encode(library);Arrangement song;Id rows[3],clips[3];
    for(unsigned i=0;i<3;++i){assert(song.add_track("Part",library,rows[i])==Result::ok);assert(song.add_clip(rows[i],101+i,0,30720,0,library,clips[i])==Result::ok);}
    PreparedArrangement prepared;assert(prepare_arrangement(song,library,9,prepared)==Result::ok&&prepared.notes.size()==7);
    NativeDestination destination;destination.project_epoch=9;destination.routing_revision=2;destination.destination_revision=3;
    destination.program_key="program";destination.sequence_ticks=30720;destination.transport_valid=true;
    for(unsigned i=0;i<128;++i)destination.pad_to_note[i]=destination.note_to_pad[i]=int(i);
    std::swap(destination.pad_to_note[0],destination.pad_to_note[72]);std::swap(destination.pad_to_note[16],destination.pad_to_note[99]);
    for(unsigned i=0;i<128;++i)destination.note_to_pad[(unsigned)destination.pad_to_note[i]]=int(i);
    NativeArrangementPlan plan;assert(prepare_native_plan(prepared,destination,nullptr,plan)==NativePlanResult::ok&&plan.matches(prepared,destination));
    assert(plan.notes.size()==7);bool tail=false;
    for(unsigned i=0;i<plan.notes.size();++i){auto&out=plan.notes[i];const auto&in=prepared.notes[i];const auto&target=prepared.targets[in.target];
        assert(out.native.time==in.native.time&&out.native.type==in.native.type&&out.native.data[32]==destination.pad_to_note[target.pad]);
        assert(out.pad==target.pad&&out.source_note==target.source_note&&out.track==in.track&&out.clip==in.clip&&out.cycle==in.cycle);
        auto normalized=out.native;normalized.data[32]=in.native.data[32];assert(!std::memcmp(&normalized,&in.native,sizeof normalized));
        std::int64_t gate;std::memcpy(&gate,out.native.data+24,8);tail|=out.native.time+gate>prepared.length;
    }
    assert(tail&&library_encode(library)==source);
    const auto stable=bytes(plan);
    auto refuse=[&](const PreparedArrangement&p,const NativeDestination&d,NativePlanResult expected){assert(prepare_native_plan(p,d,nullptr,plan)==expected&&bytes(plan)==stable);};
    auto d=destination;d.playing=true;refuse(prepared,d,NativePlanResult::transport_running);assert(!plan.matches(prepared,d));
    d=destination;d.recording=true;refuse(prepared,d,NativePlanResult::transport_running);
    d=destination;d.transport_valid=false;refuse(prepared,d,NativePlanResult::transport_unavailable);
    d=destination;++d.project_epoch;refuse(prepared,d,NativePlanResult::stale);
    d=destination;--d.sequence_ticks;refuse(prepared,d,NativePlanResult::length_mismatch);
    d=destination;d.program_key="other";refuse(prepared,d,NativePlanResult::program_mismatch);
    d=destination;d.note_to_pad[72]=72;refuse(prepared,d,NativePlanResult::mapping_unavailable);
    d=destination;d.pad_to_note[0]=-1;refuse(prepared,d,NativePlanResult::mapping_unavailable);
    d=destination;d.pad_to_note[127]=128;refuse(prepared,d,NativePlanResult::invalid);
    d=destination;++d.routing_revision;assert(!plan.matches(prepared,d));d=destination;++d.destination_revision;assert(!plan.matches(prepared,d));
    d=destination;std::swap(d.pad_to_note[0],d.pad_to_note[1]);assert(!plan.matches(prepared,d));
    auto p=prepared;++p.arrangement_revision;assert(!plan.matches(p,destination));p=prepared;++p.library_revision;assert(!plan.matches(p,destination));
    p=prepared;p.targets[0].program_key="other";refuse(p,destination,NativePlanResult::program_mismatch);
    p=prepared;p.notes[0].target=999;refuse(p,destination,NativePlanResult::invalid);
    p=prepared;p.notes[0].native.type=4;refuse(p,destination,NativePlanResult::invalid);
    p=prepared;p.notes.back().native.time=0;refuse(p,destination,NativePlanResult::invalid);
    p=prepared;p.notes[0].native.time=prepared.length;refuse(p,destination,NativePlanResult::invalid);
    p=prepared;p.notes[0].native.data[32]^=1;refuse(p,destination,NativePlanResult::invalid);
    p=prepared;std::int64_t gate=-1;std::memcpy(p.notes[0].native.data+24,&gate,8);refuse(p,destination,NativePlanResult::invalid);
    p=prepared;gate=INT64_MAX;std::memcpy(p.notes.back().native.data+24,&gate,8);refuse(p,destination,NativePlanResult::invalid);
    p=prepared;gate=0;std::memcpy(p.notes[0].native.data+24,&gate,8);NativeArrangementPlan zero;
    assert(prepare_native_plan(p,destination,nullptr,zero)==NativePlanResult::ok);std::memcpy(&gate,zero.notes[0].native.data+24,8);assert(!gate);
    p=prepared;p.notes[0].native.data[4]=1;p.notes[0].native.data[5]=128;p.notes[0].native.data[6]=(unsigned char)-12;p.notes[0].native.data[7]=8;
    NativeArrangementPlan packed;assert(prepare_native_plan(p,destination,choices,packed)==NativePlanResult::ok&&packed.notes[0].native.data[6]==(unsigned char)-12&&packed.notes[0].native.data[7]==8);
    refuse(p,destination,NativePlanResult::invalid); /* packed choices need a decoder */
    p=prepared;p.notes.assign(max_events,prepared.notes[0]);NativeArrangementPlan full;
    assert(prepare_native_plan(p,destination,nullptr,full)==NativePlanResult::ok&&full.notes.size()==max_events);
    p.notes.push_back(p.notes[0]);refuse(p,destination,NativePlanResult::limit);
    p=prepared;p.notes.clear();p.targets.clear();assert(prepare_native_plan(p,destination,nullptr,zero)==NativePlanResult::ok&&zero.notes.empty());
    unsigned failures=0;bool done=false;
    auto allocation_input=prepared;auto allocation_destination=destination;
    allocation_destination.program_key=std::string(70,'p');for(auto&t:allocation_input.targets)t.program_key=allocation_destination.program_key;
    for(int i=0;i<64;++i){NativeArrangementPlan output=plan;fail_after=i;
        try{auto result=prepare_native_plan(allocation_input,allocation_destination,nullptr,output);fail_after=-1;assert(result==NativePlanResult::ok);done=true;break;
        }catch(const std::bad_alloc&){fail_after=-1;++failures;assert(bytes(output)==stable);}}
    assert(done&&failures);
    std::cout<<"PASS arrangement native plan: parallel 2/4/8 flatten, current pad remap, musical payload/provenance, tails/zero gates, exact destination length, transport/project/routing checks, mixed Program refusal, invalid tag/target/order, packed getter seam, duplicate and event limits; "<<failures<<" allocation failure points. Value-only preflight; no native publication/playback.\n";
}
