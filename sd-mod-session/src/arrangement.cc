#include "arrangement.h"
#include <algorithm>
#include <cstring>
#include <map>
#include <set>
#include <tuple>
namespace am_pattern {
namespace {
const Pattern*pattern(const Library&library,Id id){
    const Pattern*found=nullptr;
    for(const auto&e:library.entries)if(e.pattern.id()==id){if(found)return nullptr;found=&e.pattern;}
    return found;
}
ArrangementClip*clip(Arrangement&song,Id id){for(auto&c:song.clips)if(c.id==id)return &c;return nullptr;}
std::int64_t phase(std::int64_t offset,std::int64_t delta,std::int64_t period){
    auto rem=delta%period;return offset>=period-rem?offset-(period-rem):offset+rem;
}
template<class F>Result change(Arrangement&song,const Library&library,F operation){
    Result current=song.validate(library);if(current!=Result::ok)return current;
    if(song.revision==UINT64_MAX)return Result::limit;
    Arrangement value=song;Result result=operation(value);if(result!=Result::ok)return result;
    result=value.validate(library);if(result!=Result::ok)return result;
    ++value.revision;song=std::move(value);return Result::ok;
}
bool next_id(Arrangement&song,Id&out){if(!song.next||song.next==UINT64_MAX)return false;out=song.next++;return true;}
}
Result Arrangement::validate(const Library&library)const{
    if(!next||!revision||length<=0||length>library_tick_limit)return Result::invalid;
    if(tracks.size()>arrangement_track_limit||clips.size()>arrangement_clip_limit)return Result::limit;
    std::set<Id>ids,track_ids,pattern_ids;
    for(const auto&e:library.entries)if(!e.pattern.id()||!pattern_ids.insert(e.pattern.id()).second)return Result::invalid;
    for(const auto&t:tracks){
        if(!t.id||t.id>=next||!ids.insert(t.id).second||t.name.empty()||t.name.size()>31||t.name.find('\0')!=std::string::npos)return Result::invalid;
        track_ids.insert(t.id);
    }
    std::vector<const ArrangementClip*>ordered;ordered.reserve(clips.size());
    for(const auto&c:clips){
        if(!c.id||c.id>=next||!ids.insert(c.id).second||!track_ids.count(c.track)||!c.pattern
           ||c.start<0||c.length<=0||c.start>length||c.length>length-c.start)return Result::invalid;
        const auto*p=pattern(library,c.pattern);if(!p)return Result::stale;
        if(p->length()<=0||c.offset<0||c.offset>=p->length())return Result::invalid;
        ordered.push_back(&c);
    }
    std::sort(ordered.begin(),ordered.end(),[](const ArrangementClip*a,const ArrangementClip*b){return std::tie(a->track,a->start)<std::tie(b->track,b->start);});
    for(unsigned i=1;i<ordered.size();++i)if(ordered[i-1]->track==ordered[i]->track&&ordered[i]->start<ordered[i-1]->start+ordered[i-1]->length)return Result::invalid;
    return Result::ok;
}
Result Arrangement::add_track(const std::string&name,const Library&library,Id&out){
    Id created=0;auto result=change(*this,library,[&](Arrangement&s){if(s.tracks.size()>=arrangement_track_limit||!next_id(s,created))return Result::limit;ArrangementTrack t;t.id=created;t.name=name;s.tracks.push_back(t);return Result::ok;});
    if(result==Result::ok)out=created;
    return result;
}
Result Arrangement::add_clip(Id track,Id source,std::int64_t start,std::int64_t duration,std::int64_t offset,const Library&library,Id&out){
    Id created=0;auto result=change(*this,library,[&](Arrangement&s){if(s.clips.size()>=arrangement_clip_limit||!next_id(s,created))return Result::limit;ArrangementClip c;c.id=created;c.track=track;c.pattern=source;c.start=start;c.length=duration;c.offset=offset;s.clips.push_back(c);return Result::ok;});
    if(result==Result::ok)out=created;
    return result;
}
Result Arrangement::copy_clip(Id source,Id track,std::int64_t start,const Library&library,Id&out){
    Id created=0;auto result=change(*this,library,[&](Arrangement&s){auto*c=clip(s,source);if(!c)return Result::invalid;auto copy=*c;if(s.clips.size()>=arrangement_clip_limit||!next_id(s,created))return Result::limit;copy.id=created;copy.track=track;copy.start=start;s.clips.push_back(copy);return Result::ok;});
    if(result==Result::ok)out=created;
    return result;
}
Result Arrangement::move_clip(Id id,Id track,std::int64_t start,const Library&library){return change(*this,library,[&](Arrangement&s){auto*c=clip(s,id);if(!c)return Result::invalid;c->track=track;c->start=start;return Result::ok;});}
Result Arrangement::resize_clip(Id id,std::int64_t length,const Library&library){return change(*this,library,[&](Arrangement&s){auto*c=clip(s,id);if(!c)return Result::invalid;c->length=length;return Result::ok;});}
Result Arrangement::edit_clip(Id id,Id track,std::int64_t start,std::int64_t length,const Library&library){return change(*this,library,[&](Arrangement&s){auto*c=clip(s,id);if(!c)return Result::invalid;c->track=track;c->start=start;c->length=length;return Result::ok;});}
Result Arrangement::rebind_clip(Id id,Id pattern,const Library&library){return change(*this,library,[&](Arrangement&s){auto*c=clip(s,id);if(!c)return Result::invalid;c->pattern=pattern;return Result::ok;});}
Result Arrangement::erase_clip(Id id,const Library&library){return change(*this,library,[&](Arrangement&s){auto it=std::find_if(s.clips.begin(),s.clips.end(),[&](const ArrangementClip&c){return c.id==id;});if(it==s.clips.end())return Result::invalid;s.clips.erase(it);return Result::ok;});}
Result Arrangement::cut_clip(Id id,std::int64_t cursor,const Library&library,Id&out){
    Id created=0;auto result=change(*this,library,[&](Arrangement&s){
        auto*c=clip(s,id);if(!c)return Result::invalid;
        if(cursor<=c->start||cursor>=c->start+c->length)return Result::invalid;
        auto*p=pattern(library,c->pattern);if(!p)return Result::stale;
        if(s.clips.size()>=arrangement_clip_limit||!next_id(s,created))return Result::limit;
        auto right=*c;auto delta=cursor-c->start;right.id=created;right.start=cursor;right.length-=delta;right.offset=phase(c->offset,delta,p->length());c->length=delta;s.clips.push_back(right);return Result::ok;
    });if(result==Result::ok)out=created;return result;
}
Result Arrangement::join_clips(Id first,Id second,const Library&library){return change(*this,library,[&](Arrangement&s){
    if(first==second)return Result::invalid;
    auto*a=clip(s,first),*b=clip(s,second);if(!a||!b)return Result::invalid;
    if(a->start>b->start)std::swap(a,b);
    const auto*p=pattern(library,a->pattern);if(!p)return Result::stale;
    if(a->track!=b->track||a->pattern!=b->pattern||a->muted!=b->muted||a->start+a->length!=b->start||phase(a->offset,a->length,p->length())!=b->offset)return Result::invalid;
    Id remove=b->id;a->length+=b->length;s.clips.erase(std::find_if(s.clips.begin(),s.clips.end(),[&](const ArrangementClip&c){return c.id==remove;}));return Result::ok;
});}
Result Arrangement::track_state(Id id,bool muted,bool solo,const Library&library){return change(*this,library,[&](Arrangement&s){for(auto&t:s.tracks)if(t.id==id){t.muted=muted;t.solo=solo;return Result::ok;}return Result::invalid;});}
Result Arrangement::clip_state(Id id,bool muted,const Library&library){return change(*this,library,[&](Arrangement&s){auto*c=clip(s,id);if(!c)return Result::invalid;c->muted=muted;return Result::ok;});}
Result Arrangement::song_length(std::int64_t value,const Library&library){return change(*this,library,[&](Arrangement&s){s.length=value;return Result::ok;});}
bool PreparedArrangement::matches(std::uint64_t epoch,const Arrangement&song,const Library&library)const{return project_epoch==epoch&&arrangement_revision==song.revision&&library_revision==library.revision;}
Result prepare_arrangement(const Arrangement&song,const Library&library,std::uint64_t epoch,PreparedArrangement&out){
    if(!epoch)return Result::invalid;
    Result valid=song.validate(library);if(valid!=Result::ok)return valid;
    bool solo=false;for(const auto&t:song.tracks)solo|=t.solo;
    std::map<Id,unsigned>audible;for(unsigned i=0;i<song.tracks.size();++i){const auto&t=song.tracks[i];if(!t.muted&&(!solo||t.solo))audible[t.id]=i;}
    std::size_t count=0;
    for(const auto&c:song.clips)if(!c.muted&&audible.count(c.track)){const auto*p=pattern(library,c.pattern);for(const auto&e:p->events()){
        auto first=phase(e.native.time,p->length()-c.offset,p->length());
        if(first<c.length){auto occurrences=std::uint64_t((c.length-1-first)/p->length())+1;if(occurrences>max_events-count)return Result::limit;count+=std::size_t(occurrences);}
    }}
    PreparedArrangement value;value.project_epoch=epoch;value.arrangement_revision=song.revision;value.library_revision=library.revision;value.length=song.length;value.notes.reserve(count);
    std::map<std::tuple<std::string,unsigned,unsigned>,unsigned>targets;
    for(const auto&c:song.clips)if(!c.muted&&audible.count(c.track)){const auto*p=pattern(library,c.pattern);for(const auto&e:p->events()){
        auto first=phase(e.native.time,p->length()-c.offset,p->length());if(first>=c.length)continue;
        const auto&r=p->routes()[e.route];auto key=std::make_tuple(r.program_key,r.pad,r.source_note);auto it=targets.find(key);
        if(it==targets.end()){if(value.targets.size()>=max_routes)return Result::limit;unsigned index=value.targets.size();InstrumentTarget target;target.program_key=r.program_key;target.pad=r.pad;target.source_note=r.source_note;value.targets.push_back(target);it=targets.emplace(key,index).first;}
        for(auto delta=first;delta<c.length;){
            ScheduledNote note;note.native=e.native;note.native.time=c.start+delta;note.native.type&=~0x100u;
            std::int64_t gate;std::memcpy(&gate,note.native.data+24,8);if(gate>INT64_MAX-note.native.time)return Result::invalid;
            note.track=c.track;note.clip=c.id;note.pattern=c.pattern;note.event=e.id;note.target=it->second;note.track_order=audible[c.track];
            note.cycle=std::uint64_t(delta/p->length())+(delta%p->length()>=p->length()-c.offset?1u:0u);value.notes.push_back(note);
            if(p->length()>=c.length-delta)break;
            delta+=p->length();
        }
    }}
    std::stable_sort(value.notes.begin(),value.notes.end(),[](const ScheduledNote&a,const ScheduledNote&b){return std::tie(a.native.time,a.track_order,a.clip,a.event)<std::tie(b.native.time,b.track_order,b.clip,b.event);});
    out=std::move(value);return Result::ok;
}
}
