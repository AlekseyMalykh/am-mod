#include "pattern_model.h"
#include <algorithm>
#include <cstring>
#include <limits>
#include <map>
#include <set>
#include <tuple>

namespace am_pattern {
namespace {
bool key_valid(const std::string&key){return !key.empty()&&key.size()<128&&key.find('\0')==std::string::npos;}
bool route_valid(const Route&r){return key_valid(r.program_key)&&r.pad<128&&r.source_note<128&&r.mixer_bus>=-2&&r.mixer_bus<8;}
Result validate(const step_event&e,std::int64_t length,const Route&r,am_midi_choice_get get){
    if((e.type&255)!=3)return Result::unsupported;
    am_midi_event value{};
    if(!am_midi_event_copy(&e,get,AM_MIDI_EVENT_NOTE_ON,&value)
       ||value.tick<0||value.tick>=length||value.duration<0
       ||value.duration>std::numeric_limits<std::int64_t>::max()-value.tick
       ||value.source_note!=r.source_note)return Result::invalid;
    return Result::ok;
}
step_event copy_note(const step_event&e){
    /* Copy only the known inline variant, normalizing ABI padding. Keep all
     * musical flags/modifiers, including muted/probability/repeats/channel. */
    step_event out{};out.time=e.time;out.type=e.type&~0x100u;
    std::memcpy(out.data,e.data,sizeof out.data);return out;
}
std::int64_t duration(const step_event&e){std::int64_t out;std::memcpy(&out,e.data+24,sizeof out);return out;}
}
bool Snapshot::matches(const std::string&key,std::uint64_t revision)const{return source_key_==key&&revision_==revision;}
Result Snapshot::capture(Id id,const std::string&key,std::uint64_t revision,std::int64_t length,
 const std::vector<Route>&routes,const std::vector<InputEvent>&events,am_midi_choice_get get,std::shared_ptr<const Snapshot>&out){
    if(!id||!key_valid(key)||length<=0)return Result::invalid;
    if(events.size()>max_events||routes.size()>max_routes)return Result::limit;
    std::set<std::pair<std::string,unsigned>> unique,sounds;
    for(const auto&r:routes){
        if(!route_valid(r)||!unique.emplace(r.program_key,r.pad).second
           ||!sounds.emplace(r.program_key,r.source_note).second)return Result::invalid;
    }
    for(const auto&e:events){
        if(e.route>=routes.size())return Result::invalid;
        Result result=validate(e.native,length,routes[e.route],get);if(result!=Result::ok)return result;
    }
    std::shared_ptr<Snapshot> value(new Snapshot);
    value->id_=id;value->source_key_=key;value->revision_=revision;value->length_=length;value->routes_=routes;
    value->events_.reserve(events.size());
    for(const auto&e:events){Event v;v.id=value->events_.size()+1;v.native=copy_note(e.native);v.route=e.route;value->events_.push_back(v);}
    out=std::move(value);return Result::ok;
}
Result Parts::manual(std::shared_ptr<const Snapshot>snapshot,const std::vector<Part>&parts,Parts&out){
    if(!snapshot)return Result::invalid;
    if(parts.size()>max_parts)return Result::limit;
    std::vector<unsigned char> seen(snapshot->events().size());std::set<Id> ids;
    for(const auto&p:parts){
        if(!p.id||!ids.insert(p.id).second)return Result::invalid;
        for(Id id:p.events){if(!id||id>seen.size()||seen[id-1])return Result::invalid;seen[id-1]=1;}
    }
    if(std::find(seen.begin(),seen.end(),0)!=seen.end())return Result::invalid;
    Parts value;value.snapshot_=std::move(snapshot);value.parts_=parts;out=std::move(value);return Result::ok;
}
Result Parts::split(std::shared_ptr<const Snapshot>snapshot,SplitBy by,Parts&out){
    if(!snapshot||(by!=SplitBy::pads&&by!=SplitBy::mixer))return Result::invalid;
    using Key=std::tuple<std::string,unsigned,int>;
    std::map<Key,unsigned> groups;std::vector<Part> parts;
    for(const auto&e:snapshot->events()){
        const Route&r=snapshot->routes()[e.route];
        if(by==SplitBy::mixer&&r.mixer_bus==-2)return Result::unsupported;
        Key key=by==SplitBy::pads?Key(r.program_key,r.pad,0):Key("",0,r.mixer_bus);
        auto it=groups.find(key);
        if(it==groups.end()){
            if(parts.size()>=max_parts)return Result::limit;
            unsigned index=static_cast<unsigned>(parts.size());groups.emplace(key,index);
            Part p;p.id=index+1;parts.push_back(p);it=groups.find(key);
        }
        parts[it->second].events.push_back(e.id);
    }
    return manual(std::move(snapshot),parts,out);
}
Result Pattern::empty(Id id,std::int64_t length,Pattern&out){
    if(!id||length<=0)return Result::invalid;
    Pattern value;value.id_=id;value.length_=length;out=std::move(value);return Result::ok;
}
Result Pattern::variation(const Pattern&source,Id id,Pattern&out){
    if(&source==&out||!source.id_||!id||id==source.id_)return Result::invalid;
    Pattern value=source;value.id_=id;value.parent_=source.id_;value.revision_=1;
    out=std::move(value);return Result::ok;
}
Result Pattern::replace(Id id,const step_event&event,std::uint64_t expected,am_midi_choice_get get){
    if(expected!=revision_)return Result::stale;
    if(revision_==std::numeric_limits<std::uint64_t>::max())return Result::limit;
    auto it=std::find_if(events_.begin(),events_.end(),[id](const PatternEvent&e){return e.id==id;});
    if(it==events_.end())return Result::invalid;
    Result result=validate(event,length_,routes_[it->route],get);if(result!=Result::ok)return result;
    it->native=copy_note(event);++revision_;return Result::ok;
}
Result Pattern::restore(Id id,Id parent,std::uint64_t revision,std::int64_t length,
 const std::vector<Route>&routes,const std::vector<PatternEvent>&events,am_midi_choice_get get,Pattern&out){
    if(!id||parent==id||!revision)return Result::invalid;
    if(events.size()>max_events)return Result::limit;
    std::vector<InputEvent> input;input.reserve(events.size());std::set<Id> ids;
    for(const auto&e:events){
        if(!e.id||!ids.insert(e.id).second||bool(e.source_snapshot)!=bool(e.source_event))return Result::invalid;
        InputEvent value;value.native=e.native;value.route=e.route;input.push_back(value);
    }
    std::shared_ptr<const Snapshot> checked;
    Result result=Snapshot::capture(id,"pattern-values",revision,length,routes,input,get,checked);
    if(result!=Result::ok)return result;
    Pattern value;value.id_=id;value.parent_=parent;value.revision_=revision;value.length_=length;value.routes_=routes;value.events_=events;
    for(unsigned i=0;i<events.size();++i)value.events_[i].native=checked->events()[i].native;
    out=std::move(value);return Result::ok;
}
Result extract(const Parts&parts,Id part_id,std::int64_t begin,std::int64_t end,BoundaryPolicy policy,
 Id pattern_id,Pattern&out,ExtractionInfo&info){
    auto snapshot=parts.snapshot();
    if(!snapshot||!pattern_id||begin<0||end<=begin||end>snapshot->length()||policy!=BoundaryPolicy::onsets_only)return Result::invalid;
    auto part=std::find_if(parts.parts().begin(),parts.parts().end(),[part_id](const Part&p){return p.id==part_id;});
    if(part==parts.parts().end())return Result::invalid;
    Pattern value;value.id_=pattern_id;value.length_=end-begin;
    ExtractionInfo report;std::map<unsigned,unsigned> routes;
    /* A short extract from a large source must not reserve the whole part. */
    auto count=std::count_if(part->events.begin(),part->events.end(),[&](Id id){
        auto tick=snapshot->events()[id-1].native.time;return tick>=begin&&tick<end;
    });
    value.events_.reserve(static_cast<std::size_t>(count));
    for(Id id:part->events){
        const Event&e=snapshot->events()[id-1];const auto&native=e.native;
        if(native.time<begin){if(duration(native)>begin-native.time)++report.skipped_prior_gates;continue;}
        if(native.time>=end)continue;
        if(duration(native)>end-native.time)++report.retained_tails;
        auto route=routes.find(e.route);
        if(route==routes.end()){
            unsigned index=static_cast<unsigned>(value.routes_.size());
            value.routes_.push_back(snapshot->routes()[e.route]);routes.emplace(e.route,index);route=routes.find(e.route);
        }
        PatternEvent note;note.id=value.events_.size()+1;note.source_snapshot=snapshot->id();note.source_event=e.id;
        note.native=native;note.native.time-=begin;note.route=route->second;value.events_.push_back(note);
    }
    out=std::move(value);info=report;return Result::ok;
}
}
