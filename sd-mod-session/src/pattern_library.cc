#include "pattern_library.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
namespace am_pattern {
namespace {
struct EventKey {
    std::int64_t time;std::uint32_t type;unsigned char data[37];
    explicit EventKey(const step_event&e):time(e.time),type(e.type&~0x100u){std::memcpy(data,e.data,37);}
    bool operator<(const EventKey&b)const{if(time!=b.time)return time<b.time;if(type!=b.type)return type<b.type;return std::memcmp(data,b.data,37)<0;}
};
Result append(Library&library,LibraryEntry entry){
    if(library.entries.size()>=library_limit||library.next==UINT64_MAX||library.revision==UINT64_MAX)return Result::limit;
    std::size_t count=entry.pattern.events().size();for(const auto&e:library.entries)count+=e.pattern.events().size();
    if(count>max_events)return Result::limit;
    if(entry.name.size()>63)entry.name.resize(63);
    library.entries.push_back(std::move(entry));++library.next;++library.revision;return Result::ok;
}
Result expanded(const Pattern&p,const std::vector<Route>&catalog,am_midi_choice_get get,Pattern&out){
    auto notes=p.events();
    for(auto&e:notes){const Route&r=p.routes()[e.route];unsigned at=0;
        while(at<catalog.size()&&(catalog[at].program_key!=r.program_key||catalog[at].pad!=r.pad))++at;
        if(at==catalog.size())return Result::invalid;
        e.route=at;
    }
    return Pattern::restore(p.id(),p.parent(),p.revision(),p.length(),catalog,notes,get,out);
}
}
Result Library::split(std::shared_ptr<const Snapshot>source,const std::vector<Id>*selected,SplitBy by,am_midi_choice_get get){
    if(!source)return Result::invalid;
    Parts parts;Result result;
    if(selected){
        std::set<Id> selection(selected->begin(),selected->end());if(selection.empty()||selection.size()!=selected->size())return Result::invalid;
        std::vector<Part>groups(2);groups[0].id=1;groups[1].id=2;
        for(const auto&e:source->events())groups[selection.count(e.id)?0:1].events.push_back(e.id);
        if(groups[0].events.size()!=selection.size())return Result::invalid;
        if(groups[1].events.empty())groups.pop_back();
        result=Parts::manual(source,groups,parts);
    }else result=Parts::split(source,by,parts);
    if(result!=Result::ok)return result;
    Library pending=*this;
    for(const auto&part:parts.parts()){
        LibraryEntry entry;ExtractionInfo info;entry.part=true;
        result=extract(parts,part.id,0,source->length(),BoundaryPolicy::onsets_only,pending.next,entry.pattern,info);
        if(result!=Result::ok)return result;
        Pattern full;result=expanded(entry.pattern,source->routes(),get,full);if(result!=Result::ok)return result;entry.pattern=std::move(full);
        if(selected)entry.name=part.id==1?"Selected part":"Unassigned";
        else {const auto&r=source->routes()[source->events()[part.events[0]-1].route];
            if(by==SplitBy::pads){entry.name=std::string(1,char('A'+r.pad/16))+(r.pad%16<9?"0":"")+std::to_string(r.pad%16+1);}
            else entry.name=r.mixer_bus<0?"Master":std::string("Bus ")+char('A'+r.mixer_bus);
        }
        result=append(pending,std::move(entry));if(result!=Result::ok)return result;
    }
    *this=std::move(pending);return Result::ok;
}
Result Library::window(unsigned index,std::int64_t begin,std::int64_t end,am_midi_choice_get get){
    if(index>=entries.size())return Result::invalid;
    const auto&entry=entries[index];std::vector<InputEvent>input;
    for(const auto&e:entry.pattern.events()){InputEvent n;n.native=e.native;n.route=e.route;input.push_back(n);}
    std::shared_ptr<const Snapshot>source;
    Result result=Snapshot::capture(next,"library-extract",entry.pattern.revision(),entry.pattern.length(),entry.pattern.routes(),input,get,source);
    if(result!=Result::ok)return result;
    Part part;part.id=1;for(const auto&e:source->events())part.events.push_back(e.id);
    Parts parts;result=Parts::manual(source,{part},parts);if(result!=Result::ok)return result;
    LibraryEntry value;value.name=entry.name+" extract";ExtractionInfo info;
    result=extract(parts,1,begin,end,BoundaryPolicy::onsets_only,next,value.pattern,info);if(result!=Result::ok)return result;
    Pattern full;result=expanded(value.pattern,entry.pattern.routes(),get,full);if(result!=Result::ok)return result;value.pattern=std::move(full);
    return append(*this,std::move(value));
}
Result Library::variation(unsigned index){
    if(index>=entries.size())return Result::invalid;
    LibraryEntry value;value.name=entries[index].name+" var";
    Result result=Pattern::variation(entries[index].pattern,next,value.pattern);return result==Result::ok?append(*this,std::move(value)):result;
}
Result Library::empty(std::int64_t length,const std::vector<Route>&routes,am_midi_choice_get get){
    LibraryEntry value;value.name="Empty pattern";
    Result result=Pattern::restore(next,0,1,length,routes,{},get,value.pattern);return result==Result::ok?append(*this,std::move(value)):result;
}
Result Library::erase(unsigned index){
    if(index>=entries.size())return Result::invalid;
    if(revision==UINT64_MAX)return Result::limit;
    entries.erase(entries.begin()+index);++revision;return Result::ok;
}
Result Library::edit(unsigned index,std::uint64_t expected,const std::vector<step_event>&removed,const std::vector<step_event>&added,am_midi_choice_get get){
    if(index>=entries.size()||(removed.empty()&&added.empty()))return Result::invalid;
    auto&p=entries[index].pattern;if(expected!=p.revision())return Result::stale;
    if(revision==UINT64_MAX||expected==UINT64_MAX)return Result::limit;
    std::map<EventKey,unsigned>removals;for(const auto&e:removed)++removals[EventKey(e)];
    std::vector<PatternEvent>notes;notes.reserve(p.events().size()+added.size());Id id=0;
    for(const auto&e:p.events()){id=std::max(id,e.id);auto it=removals.find(EventKey(e.native));
        if(it!=removals.end()&&it->second)--it->second;else notes.push_back(e);
    }
    for(const auto&item:removals)if(item.second)return Result::stale;
    std::array<int,128>routes;routes.fill(-1);
    for(unsigned i=0;i<p.routes().size();++i){unsigned note=p.routes()[i].source_note;if(routes[note]>=0)return Result::unsupported;routes[note]=(int)i;}
    for(const auto&e:added){if(e.data[32]>127||routes[e.data[32]]<0||id==UINT64_MAX)return Result::invalid;
        PatternEvent note;note.id=++id;note.native=e;note.route=(unsigned)routes[e.data[32]];notes.push_back(note);
    }
    std::stable_sort(notes.begin(),notes.end(),[](const PatternEvent&a,const PatternEvent&b){return a.native.time<b.native.time;});
    std::size_t count=notes.size();for(unsigned i=0;i<entries.size();++i)if(i!=index)count+=entries[i].pattern.events().size();if(count>max_events)return Result::limit;
    Pattern value;Result result=Pattern::restore(p.id(),p.parent(),expected+1,p.length(),p.routes(),notes,get,value);
    if(result==Result::ok){p=std::move(value);++revision;}return result;
}
}
