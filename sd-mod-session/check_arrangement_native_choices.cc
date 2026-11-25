/* Called only by the ARM pinned-ELF fixture. Real inline factory/getter/setter;
 * no Project, Track, audio backend or fabricated packed choice layout. */
#include "src/arrangement_native_plan.h"
#include "src/arrangement_native_material.h"
#include <cassert>
#include <cstring>
#include <set>
#include <tuple>
extern "C" int check_arrangement_native_choices(std::uintptr_t image){
    using namespace am_pattern;
    auto factory=reinterpret_cast<step_event(*)(unsigned,std::int64_t,std::int64_t,unsigned,unsigned,float)>(image+0x27e54d0);
    auto set=reinterpret_cast<void(*)(void*,int,int,int)>(image+0x27e4400);
    auto get=reinterpret_cast<am_midi_choice_get>(image+0x27e4230);
    auto event=factory(36,960,480,100,0,.8f);set(event.data+4,0,0,-12);set(event.data+4,15,0,7);
    event.data[34]=61;event.data[36]=(event.data[36]&~7u)|3u;
    PreparedArrangement prepared;prepared.project_epoch=7;prepared.arrangement_revision=8;prepared.library_revision=9;prepared.length=3840;
    InstrumentTarget target;target.program_key="native-choice-fixture";target.pad=0;target.source_note=36;prepared.targets.push_back(target);
    ScheduledNote n;n.native=event;n.track=1;n.clip=2;n.pattern=3;n.event=4;n.target=0;prepared.notes.push_back(n);
    NativeDestination destination;destination.program_key=target.program_key;destination.project_epoch=7;destination.routing_revision=1;
    destination.destination_revision=1;destination.sequence_ticks=3840;destination.transport_valid=true;
    for(unsigned i=0;i<128;++i)destination.pad_to_note[i]=destination.note_to_pad[i]=int(i);
    std::swap(destination.pad_to_note[0],destination.pad_to_note[48]);std::swap(destination.note_to_pad[0],destination.note_to_pad[48]);
    NativeArrangementPlan output;assert(prepare_native_plan(prepared,destination,get,output)==NativePlanResult::ok&&output.notes.size()==1);
    am_midi_event decoded{};assert(am_midi_event_copy(&output.notes[0].native,get,AM_MIDI_EVENT_NOTE_ON,&decoded));
    assert(decoded.tick==960&&decoded.duration==480&&decoded.tune==-12&&decoded.slice==7&&decoded.probability==61&&decoded.repeats==4&&decoded.source_note==48);
    auto normalized=output.notes[0].native;normalized.data[32]=36;event.type&=~0x100u;assert(!std::memcmp(&normalized,&event,sizeof event));
    return 1;
}
extern "C" int check_arrangement_native_engine_plan(std::uintptr_t image,void*user,
    void(*consume)(void*,const step_event*,unsigned,std::int64_t)){
    using namespace am_pattern;
    auto factory=reinterpret_cast<step_event(*)(unsigned,std::int64_t,std::int64_t,unsigned,unsigned,float)>(image+0x27e54d0);
    auto set=reinterpret_cast<void(*)(void*,int,int,int)>(image+0x27e4400);
    auto get=reinterpret_cast<am_midi_choice_get>(image+0x27e4230);
    Library library;Arrangement song;const std::int64_t periods[]={7680,15360,30720},gates[]={480,20000,30720};const int tune[]={0,4,7};
    for(unsigned i=0;i<3;++i){
        Route route;route.program_key="native-synth-program";route.pad=15;route.source_note=50;
        PatternEvent n;n.id=1;n.native=factory(50,0,gates[i],100,2,.8f);set(n.native.data+4,0,0,tune[i]);
        LibraryEntry entry;entry.name="Parallel part";assert(Pattern::restore(101+i,0,1,periods[i],{route},{n},get,entry.pattern)==Result::ok);
        library.entries.push_back(entry);library.next=102+i;++library.revision;
        Id row,clip;assert(song.add_track("Part",library,row)==Result::ok&&song.add_clip(row,101+i,0,30720,0,library,clip)==Result::ok);
    }
    PreparedArrangement prepared;assert(prepare_arrangement(song,library,1,prepared)==Result::ok&&prepared.notes.size()==7);
    NativeDestination destination;destination.project_epoch=1;destination.routing_revision=1;destination.destination_revision=1;
    destination.program_key="native-synth-program";destination.sequence_ticks=30720;destination.transport_valid=true;
    destination.pad_to_note[15]=50;destination.note_to_pad[50]=15;
    NativeArrangementPlan plan;assert(prepare_native_plan(prepared,destination,get,plan)==NativePlanResult::ok);
    std::shared_ptr<const NativePlaybackMaterial>material;
    assert(NativePlaybackMaterial::prepare(image,plan,material)==NativeMaterialResult::ok);
    /* Native material must survive the compiler's source values. Read its
     * stock-ordered tree with the actual native iterator; delivery/clock in
     * consume remain fixtures, not a production frame scheduler. */
    plan=NativeArrangementPlan();prepared=PreparedArrangement();library=Library();
    auto events=static_cast<const unsigned char*>(material->events());
    const void*node;std::memcpy(&node,events+0x78,sizeof node);std::vector<step_event>notes;
    std::set<std::tuple<Id,Id,Id,std::uint64_t>>origins;
    while(node!=events+0x68){assert(notes.size()<max_events);step_event n;
        std::memcpy(&n,static_cast<const unsigned char*>(node)+0x20,sizeof n);notes.push_back(n);
        const auto*origin=material->origin(static_cast<const unsigned char*>(node)+0x20);
        assert(origin&&origin->native.time==n.time&&origin->native.type==n.type&&!std::memcmp(origin->native.data,n.data,37));
        assert(origins.emplace(origin->track,origin->clip,origin->event,origin->cycle).second);
        node=reinterpret_cast<void*(*)(const void*)>(image+0xb51738)(node);}
    assert(notes.size()==7&&origins.size()==7);
    consume(user,notes.data(),(unsigned)notes.size(),material->plan().destination.sequence_ticks);
    return 1;
}
