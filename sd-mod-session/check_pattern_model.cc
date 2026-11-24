#include "src/pattern_model.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <limits>
using namespace am_pattern;
static step_event note(unsigned sound,std::int64_t tick,std::int64_t length){
    step_event e{};e.time=tick;e.type=3;float velocity=.75f;
    std::memcpy(e.data,&velocity,sizeof velocity);std::memcpy(e.data+24,&length,sizeof length);
    e.data[32]=static_cast<unsigned char>(sound);e.data[34]=83;e.data[36]=2;return e;
}
static std::int64_t gate(const step_event&e){std::int64_t n;std::memcpy(&n,e.data+24,sizeof n);return n;}
static InputEvent input(const step_event&e,unsigned route){InputEvent v;v.native=e;v.route=route;return v;}
static Part part(Id id,std::initializer_list<Id> events){Part p;p.id=id;p.events=events;return p;}
static Route route(unsigned pad,unsigned sound,int bus){Route r;r.program_key="program-1";r.pad=pad;r.source_note=sound;r.mixer_bus=bus;return r;}
static int tune_fixture(const void*,int kind,int){assert(kind==0);return 7;}
int main(){
    std::vector<Route> routes={route(0,36,0),route(1,37,1),route(16,52,0)};
    std::vector<InputEvent> source={input(note(36,0,0),0),input(note(36,3840,120),0),
        input(note(37,0,8000),1),input(note(37,3840,3840),1),input(note(52,100,10000),2),input(note(36,3840,120),0)};
    source[1].native.type|=0x100u|0x200u|(3u<<12); /* UI selection, mute, MIDI channel. */
    source[5]=source[1];auto original=source;
    std::shared_ptr<const Snapshot> snapshot;
    assert(Snapshot::capture(100,"sequence-key",7,7680,routes,source,nullptr,snapshot)==Result::ok);
    assert(snapshot->matches("sequence-key",7)&&!snapshot->matches("sequence-key",8));
    assert(snapshot->events()[1].id!=snapshot->events()[5].id);
    assert(!(snapshot->events()[1].native.type&0x100));
    assert(snapshot->events()[1].native.type==(source[1].native.type&~0x100u));
    Parts pads,mixer,manual;
    assert(Parts::split(snapshot,SplitBy::pads,pads)==Result::ok&&pads.parts().size()==3);
    assert(pads.parts()[0].events==std::vector<Id>({1,2,6}));
    assert(Parts::split(snapshot,SplitBy::mixer,mixer)==Result::ok&&mixer.parts().size()==2);
    assert(mixer.parts()[0].events==std::vector<Id>({1,2,5,6}));
    assert(Parts::manual(snapshot,{part(10,{1,2}),part(20,{3,4}),part(30,{5}),part(40,{6})},manual)==Result::ok);
    /* The same pad and byte-identical occurrences may belong to different
     * musical parts; neither bus assignment nor input channel is a part ID. */
    assert(manual.parts()[0].events[1]==2&&manual.parts()[3].events[0]==6);
    Parts previous=manual;
    assert(Parts::manual(snapshot,{part(10,{1,2,3,4,5,6}),part(20,{6})},manual)==Result::invalid);
    assert(manual.parts().size()==previous.parts().size()&&manual.parts()[3].id==40);
    assert(Parts::manual(snapshot,{part(10,{1,2,3,4,5})},manual)==Result::invalid);
    assert(Parts::manual(snapshot,{part(10,{0,1,2,3,4,5,6})},manual)==Result::invalid);
    assert(Parts::manual(snapshot,{part(10,{1,2,3}),part(10,{4,5,6})},manual)==Result::invalid);

    Pattern drums,bass,sample;ExtractionInfo info;
    assert(extract(pads,1,3840,7680,BoundaryPolicy::onsets_only,201,drums,info)==Result::ok);
    assert(drums.length()==3840&&drums.events().size()==2&&drums.routes().size()==1);
    assert(drums.events()[0].native.time==0&&drums.events()[1].native.time==0);
    assert(drums.events()[0].source_snapshot==100&&drums.events()[0].source_event==2&&drums.events()[1].source_event==6);
    assert(drums.events()[0].native.data[34]==83&&drums.events()[0].native.data[36]==2);
    assert(extract(pads,2,3840,7680,BoundaryPolicy::onsets_only,202,bass,info)==Result::ok);
    assert(info.skipped_prior_gates==1&&info.retained_tails==0&&bass.events().size()==1);
    assert(extract(pads,3,0,3840,BoundaryPolicy::onsets_only,203,sample,info)==Result::ok);
    assert(info.retained_tails==1&&sample.length()==3840&&gate(sample.events()[0].native)==10000);
    assert(sample.routes()[0].pad==16&&sample.routes()[0].mixer_bus==0);
    for(unsigned i=0;i<source.size();++i){
        assert(source[i].route==original[i].route);
        assert(!std::memcmp(&source[i].native,&original[i].native,sizeof(step_event)));
    }
    routes[2].mixer_bus=7;source[4].native.time=1000;
    assert(sample.routes()[0].mixer_bus==0&&sample.events()[0].native.time==100);
    assert(snapshot->routes()[2].mixer_bus==0&&snapshot->events()[4].native.time==100);

    Pattern variation;
    assert(Pattern::variation(sample,204,variation)==Result::ok&&variation.parent()==203);
    auto changed=variation.events()[0].native;changed.time=200;
    assert(variation.replace(1,changed,1,nullptr)==Result::ok&&variation.revision()==2);
    assert(sample.events()[0].native.time==100&&variation.events()[0].native.time==200);
    assert(variation.replace(1,changed,1,nullptr)==Result::stale);
    assert(Pattern::variation(sample,205,sample)==Result::invalid);
    changed.time=3840;assert(variation.replace(1,changed,2,nullptr)==Result::invalid);
    assert(variation.revision()==2&&variation.events()[0].native.time==200);
    changed.time=200;changed.data[32]=37;assert(variation.replace(1,changed,2,nullptr)==Result::invalid);
    changed=variation.events()[0].native;changed.type=4;
    assert(variation.replace(1,changed,2,nullptr)==Result::unsupported);
    Pattern empty;assert(Pattern::empty(205,2880,empty)==Result::ok&&empty.events().empty());
    assert(Pattern::empty(0,2880,empty)==Result::invalid&&empty.id()==205);
    info.retained_tails=42;
    assert(extract(pads,1,-1,7680,BoundaryPolicy::onsets_only,206,empty,info)==Result::invalid);
    assert(empty.id()==205&&info.retained_tails==42);
    assert(extract(pads,1,0,3840,BoundaryPolicy::onsets_only,206,empty,info)==Result::ok);
    assert(empty.events().size()==1); /* End is exclusive. */

    /* Failed capture must preserve the previous complete snapshot. */
    auto invalid=original;invalid[0].native.type=4;
    assert(Snapshot::capture(101,"other",0,7680,routes,invalid,nullptr,snapshot)==Result::unsupported&&snapshot->id()==100);
    invalid=original;invalid[0].native.time=7680;
    assert(Snapshot::capture(101,"other",0,7680,routes,invalid,nullptr,snapshot)==Result::invalid);
    invalid=original;invalid[0].native=note(36,10,std::numeric_limits<std::int64_t>::max());
    assert(Snapshot::capture(101,"other",0,7680,routes,invalid,nullptr,snapshot)==Result::invalid);
    invalid=original;invalid[0].route=3;
    assert(Snapshot::capture(101,"other",0,7680,routes,invalid,nullptr,snapshot)==Result::invalid);
    routes[0].mixer_bus=-2;
    std::shared_ptr<const Snapshot> unknown;
    assert(Snapshot::capture(102,"unknown-route",0,7680,routes,original,nullptr,unknown)==Result::ok);
    assert(Parts::split(unknown,SplitBy::mixer,mixer)==Result::unsupported&&mixer.snapshot()->id()==100);
    assert(Parts::split(unknown,SplitBy::pads,manual)==Result::ok);
    auto bad_routes=routes;bad_routes.push_back(routes[0]);
    assert(Snapshot::capture(103,"duplicate-route",0,7680,bad_routes,original,nullptr,unknown)==Result::invalid);
    bad_routes=routes;bad_routes[1].source_note=36;
    assert(Snapshot::capture(103,"ambiguous-pad",0,7680,bad_routes,original,nullptr,unknown)==Result::invalid);
    auto tuned=original;tuned[0].native.data[4]=1;tuned[0].native.data[6]=99;
    assert(Snapshot::capture(104,"tune",0,7680,routes,tuned,tune_fixture,unknown)==Result::ok);
    assert(unknown->events()[0].native.data[6]==99); /* Preserve native modifier bytes, no second encoding. */
    assert(Snapshot::capture(105,"empty",0,2880,{}, {},nullptr,unknown)==Result::ok);
    assert(Parts::split(unknown,SplitBy::pads,manual)==Result::ok&&manual.parts().empty());
    std::vector<InputEvent> too_many(max_events+1,input(note(36,0,0),0));
    assert(Snapshot::capture(106,"over-budget",0,7680,routes,too_many,nullptr,unknown)==Result::limit&&unknown->id()==105);
    too_many.pop_back();
    assert(Snapshot::capture(106,"at-budget",0,7680,routes,too_many,nullptr,unknown)==Result::ok);
    assert(unknown->events().size()==max_events&&unknown->events().back().id==max_events);
    Parts large;
    assert(Parts::split(unknown,SplitBy::pads,large)==Result::ok&&large.parts()[0].events.size()==max_events);
    std::vector<Part> excess_parts(max_parts+1);
    assert(Parts::manual(unknown,excess_parts,large)==Result::limit&&large.parts().size()==1);
    std::vector<Route> many_routes;std::vector<InputEvent> many_events;
    for(unsigned i=0;i<max_parts+1;++i){
        Route r=route(0,36,0);r.program_key="program-"+std::to_string(i);
        many_routes.push_back(r);many_events.push_back(input(note(36,0,0),i));
    }
    assert(Snapshot::capture(107,"many-programs",0,7680,many_routes,many_events,nullptr,unknown)==Result::ok);
    assert(Parts::split(unknown,SplitBy::pads,large)==Result::limit&&large.snapshot()->id()==106);
    many_routes.resize(max_routes+1);
    assert(Snapshot::capture(108,"over-route-budget",0,7680,many_routes,{},nullptr,unknown)==Result::limit&&unknown->id()==107);
    assert(source.size()==original.size()&&original[4].native.time==100);
    std::cout<<"PASS pattern model: immutable snapshot, exclusive pad/mixer/manual parts, duplicate occurrence IDs, independent extraction/variation, explicit boundaries/tails, stale edits, unsupported variants and transactional rejection. Native snapshot adapter/UI/save/playback not connected.\n";
}
