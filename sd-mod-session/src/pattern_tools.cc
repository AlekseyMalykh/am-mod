#include "pattern_tools.h"
#include "pattern_library.h"
#include <atomic>
#include <cstdio>
#include <cstring>
#include <exception>
#include <mutex>
#include <stdexcept>
namespace {
using namespace am_pattern;
struct State {Library library;Arrangement song;unsigned mode=0,arr_mode=0;std::string opaque,arr_opaque;};
std::mutex mutex;std::shared_ptr<const State>current,empty_state;
std::vector<std::shared_ptr<const State>>undo,redo;
std::atomic<uint64_t>epoch{1};uintptr_t image;
thread_local char error[96];
thread_local uint64_t expected_epoch;
void set_error(const char*text){std::snprintf(error,sizeof error,"%s",text);}
am_midi_choice_get getter(){return reinterpret_cast<am_midi_choice_get>(image+0x27e4230);}
template<class T>T field(const void*p,std::size_t n){T v;std::memcpy(&v,static_cast<const char*>(p)+n,sizeof v);return v;}
std::shared_ptr<const State>snapshot(){std::lock_guard<std::mutex>lock(mutex);if(!current)throw std::runtime_error("Patterns unavailable");return current;}
const char*message(Result r){return r==Result::limit?"Pattern library limit":r==Result::stale?"Pattern changed: retry":r==Result::unsupported?"Routing or event unsupported":"Invalid pattern operation";}
template<class F>int edit_state(F function){try{
    uint64_t generation=epoch.load();if(expected_epoch&&generation!=expected_epoch)throw std::runtime_error("Project changed: reopen Patterns");
    auto old=snapshot();if(old->mode==2||old->arr_mode==2)throw std::runtime_error("Newer song data: read only");
    auto next=std::make_shared<State>(*old);Result result=function(*next);if(result!=Result::ok)throw std::runtime_error(message(result));
    result=next->song.validate(next->library);if(result!=Result::ok)throw std::runtime_error("Pattern is used by arrangement");
    next->mode=1;std::lock_guard<std::mutex>lock(mutex);if(current!=old||epoch.load()!=generation)throw std::runtime_error("Project changed: retry");
    undo.push_back(current);if(undo.size()>4)undo.erase(undo.begin());redo.clear();current=std::move(next);error[0]=0;return 1;
}catch(const std::bad_alloc&){set_error("Not enough memory for patterns");return 0;}catch(const std::exception&e){set_error(e.what());return 0;}catch(...){set_error("Pattern operation failed");return 0;}}
template<class F>int edit(F function){return edit_state([&](State&s){return function(s.library);});}
template<class F>int arrange(F function){return edit_state([&](State&s){auto result=function(s.song,s.library);if(result==Result::ok)s.arr_mode=1;return result;});}
void publish(std::shared_ptr<State>value){std::lock_guard<std::mutex>lock(mutex);current=std::move(value);undo.clear();redo.clear();epoch.fetch_add(1);}
struct Bytes{const char*data;std::size_t size;};
Bytes archive_string(const void*p){
    unsigned flags=field<unsigned short>(p,22);if(!(flags&(1u<<10)))throw std::runtime_error("Patterns must be a string");
    if(flags&(1u<<12)){unsigned spare=field<unsigned char>(p,21);if(spare>21)throw std::runtime_error("Invalid inline patterns");return {static_cast<const char*>(p),21-spare};}
    auto size=field<uint32_t>(p,0);const char*data=field<const char*>(p,8);if(!data&&size)throw std::runtime_error("Missing patterns");return {data?data:"",size};
}
const char archive_key[]="amPatternsV1";
const char arrangement_key[]="amArrangementV1";
std::shared_ptr<State>capture_archive(void*wrapper){
    auto next=std::make_shared<State>();void*a=wrapper;for(unsigned i=0;i<4;++i){if(!a)throw std::runtime_error("Pattern archive unavailable");a=field<void*>(a,8);}
    if(!a)throw std::runtime_error("Pattern archive unavailable");
    auto cursor=field<uintptr_t>(a,0x150);
    if(cursor<0x38)throw std::runtime_error("Pattern frame unavailable");
    const void*frame=reinterpret_cast<void*>(cursor-0x38);
    if(field<unsigned>(frame,0x30)!=1)throw std::runtime_error("Patterns outside object");
    auto begin=field<uintptr_t>(frame,0),end=field<uintptr_t>(frame,8);
    if(end<begin||(end-begin)%48||(!begin&&end))throw std::runtime_error("Invalid pattern members");
    bool found=false,arr_found=false;std::string library_text,arr_text;
    for(auto at=begin;at<end;at+=48){auto name=archive_string(reinterpret_cast<void*>(at));
        bool is_library=name.size==sizeof(archive_key)-1&&!std::memcmp(name.data,archive_key,name.size);
        bool is_arr=name.size==sizeof(arrangement_key)-1&&!std::memcmp(name.data,arrangement_key,name.size);
        if(!is_library&&!is_arr)continue;
        if((is_library&&found)||(is_arr&&arr_found))throw std::runtime_error("Duplicate song data");
        auto value=archive_string(reinterpret_cast<void*>(at+24));
        if(value.size>(is_arr?65536u:8u*1024u*1024u+64u))throw std::runtime_error("Song archive too large");
        if(is_library){found=true;library_text.assign(value.data,value.size);}else{arr_found=true;arr_text.assign(value.data,value.size);}
    }
    if(found){Result result=library_decode(library_text,getter(),next->library);
        if(result==Result::unsupported){next->mode=2;next->opaque=std::move(library_text);}
        else if(result!=Result::ok)throw std::runtime_error("Damaged pattern library");else next->mode=1;
    }
    if(arr_found){Result result=next->mode==2?Result::unsupported:arrangement_decode(arr_text,next->library,next->song);
        if(result==Result::unsupported){next->arr_mode=2;next->arr_opaque=std::move(arr_text);}
        else if(result!=Result::ok)throw std::runtime_error("Damaged arrangement or pattern references");else next->arr_mode=1;
    }
    return next;
}
struct Transaction {std::shared_ptr<State>value;std::exception_ptr failure;bool captured=false;};
thread_local Transaction*transaction;
}
extern "C" void pt_init(uintptr_t base){if(!base)throw std::invalid_argument("Pattern image missing");std::lock_guard<std::mutex>lock(mutex);if(image&&image!=base)throw std::runtime_error("Pattern image changed");image=base;if(!empty_state)empty_state=std::make_shared<State>();if(!current)current=empty_state;}
extern "C" void pt_reset(){std::lock_guard<std::mutex>lock(mutex);current=empty_state;undo.clear();redo.clear();epoch.fetch_add(1);}
extern "C" uint64_t pt_epoch(){return epoch.load();}
extern "C" void pt_expect_epoch(uint64_t value){expected_epoch=value;}
extern "C" const char*pt_error(){return error;}
extern "C" unsigned pt_count(){try{return snapshot()->library.entries.size();}catch(...){return 0;}}
extern "C" int pt_info(unsigned index,pt_entry*out){if(!out)return 0;std::memset(out,0,sizeof *out);try{auto s=snapshot();if(index>=s->library.entries.size())return 0;const auto&e=s->library.entries[index];std::snprintf(out->name,sizeof out->name,"%s",e.name.c_str());out->id=e.pattern.id();out->revision=e.pattern.revision();out->length=e.pattern.length();out->count=e.pattern.events().size();out->part=e.part;return 1;}catch(...){return 0;}}
extern "C" int pt_capture(const pr_note*notes,unsigned count,int64_t length,uint64_t revision,const char*key,const int mapping[128],const int outputs[128],unsigned mode){
    return edit([&](Library&library){
        if((!notes&&count)||!mapping||!outputs||!key||!key[0]||std::strlen(key)>=128||mode>3||count>PR_MAX_EVENTS||length<=0||length>library_tick_limit)return Result::invalid;
        std::vector<Route>routes;for(unsigned pad=0;pad<128;++pad){if(mapping[pad]<0||mapping[pad]>127||outputs[pad]< -1||outputs[pad]>8)return Result::invalid;Route r;r.program_key=key;r.pad=pad;r.source_note=mapping[pad];r.mixer_bus=outputs[pad]<0?-2:outputs[pad]-1;routes.push_back(r);}
        if(mode==3)return library.empty(length,routes,getter());
        std::vector<InputEvent>input;std::vector<Id>selected;
        for(unsigned i=0;i<count;++i){if(notes[i].sound<0||notes[i].sound>=128)return Result::invalid;InputEvent e;e.native=notes[i].event;e.route=notes[i].sound;input.push_back(e);if(notes[i].selected)selected.push_back(i+1);}
        std::shared_ptr<const Snapshot>source;Result result=Snapshot::capture(library.next,"piano-roll-values",revision,length,routes,input,getter(),source);
        if(result!=Result::ok)return result;
        return library.split(source,mode==2?&selected:nullptr,mode==1?SplitBy::mixer:SplitBy::pads,getter());
    });
}
extern "C" int pt_extract(unsigned i,int64_t begin,int64_t end){return edit([&](Library&l){return l.window(i,begin,end,getter());});}
extern "C" int pt_variation(unsigned i){return edit([&](Library&l){return l.variation(i);});}
extern "C" int pt_remove(unsigned i){return edit([&](Library&l){return l.erase(i);});}
extern "C" int pt_history(int direction){try{std::lock_guard<std::mutex>lock(mutex);if(expected_epoch&&expected_epoch!=epoch.load())throw std::runtime_error("Project changed: reopen Patterns");auto&from=direction<0?undo:redo;auto&to=direction<0?redo:undo;if(from.empty())throw std::runtime_error("No pattern history");to.push_back(current);current=from.back();from.pop_back();error[0]=0;return 1;}catch(const std::exception&e){set_error(e.what());return 0;}}
extern "C" int pt_notes(unsigned index,pr_note*out,unsigned capacity,unsigned char loaded[128]){try{
    auto s=snapshot();if(index>=s->library.entries.size())return -1;const auto&p=s->library.entries[index].pattern;
    if(p.events().size()>capacity||(!out&&!p.events().empty()))return -1;
    if(loaded)std::memset(loaded,0,128);
    unsigned i=0;for(const auto&e:p.events()){am_midi_event decoded{};if(!am_midi_event_copy(&e.native,getter(),AM_MIDI_EVENT_NOTE_ON,&decoded))return -1;
        auto&n=out[i++];std::memset(&n,0,sizeof n);n.event=e.native;n.sound=p.routes()[e.route].pad;n.tune=decoded.tune;n.slice=decoded.slice;unsigned mask=e.native.data[4]|unsigned(e.native.data[5])<<8;n.flags=(mask&1?1:0)|(mask&0x8000?2:0);}
    if(loaded)for(const auto&e:p.events())loaded[p.routes()[e.route].pad]=1;
    return int(i);
}catch(...){return -1;}}
extern "C" int pt_note_number(unsigned index,int pad){try{auto s=snapshot();if(index>=s->library.entries.size())return -1;for(const auto&r:s->library.entries[index].pattern.routes())if(int(r.pad)==pad)return r.source_note;}catch(...){}return -1;}
extern "C" int pt_pad_number(unsigned index,int note){try{auto s=snapshot();if(index>=s->library.entries.size())return -1;int pad=-1;for(const auto&r:s->library.entries[index].pattern.routes())if(int(r.source_note)==note){if(pad>=0&&pad!=int(r.pad))return -1;pad=r.pad;}return pad;}catch(...){return -1;}}
extern "C" int pt_encode(unsigned index,const pr_note*n,step_event*out){
    if(!n||!out||n->tune< -120||n->tune>120||n->slice<0||n->slice>128)return 0;
    int note=pt_note_number(index,n->sound);if(note<0)return 0;step_event e=n->event;e.data[32]=note;
    am_midi_event old{};if(!am_midi_event_copy(&e,getter(),AM_MIDI_EVENT_NOTE_ON,&old))return 0;
    auto setter=reinterpret_cast<void(*)(void*,int,int,int)>(image+0x27e4400);
    if(old.tune!=n->tune)setter(e.data+4,0,0,n->tune);
    if(old.slice!=n->slice){if(!n->slice)return 0;setter(e.data+4,15,0,n->slice);}
    *out=e;return 1;
}
extern "C" int pt_commit(unsigned index,const pr_plan*plan){if(!plan||plan->removed_count>PR_MAX_EVENTS||plan->added_count>PR_MAX_EVENTS||(!plan->removed&&plan->removed_count)||(!plan->added&&plan->added_count))return 0;return edit([&](Library&l){
    std::vector<step_event>removed,added;if(plan->removed_count)removed.assign(plan->removed,plan->removed+plan->removed_count);if(plan->added_count)added.assign(plan->added,plan->added+plan->added_count);
    return l.edit(index,plan->revision,removed,added,getter());
});}
extern "C" int pt_arr_info(pt_arrangement*out){if(!out)return 0;std::memset(out,0,sizeof *out);try{auto s=snapshot();out->revision=s->song.revision;out->length=s->song.length;out->tracks=s->song.tracks.size();out->clips=s->song.clips.size();out->read_only=s->mode==2||s->arr_mode==2;return 1;}catch(...){return 0;}}
extern "C" int pt_arr_track_at(unsigned index,pt_arr_track*out){if(!out)return 0;std::memset(out,0,sizeof *out);try{auto s=snapshot();if(index>=s->song.tracks.size())return 0;const auto&t=s->song.tracks[index];out->id=t.id;std::snprintf(out->name,sizeof out->name,"%s",t.name.c_str());out->muted=t.muted;out->solo=t.solo;return 1;}catch(...){return 0;}}
extern "C" int pt_arr_clip_at(unsigned index,pt_arr_clip*out){if(!out)return 0;std::memset(out,0,sizeof *out);try{auto s=snapshot();if(index>=s->song.clips.size())return 0;const auto&c=s->song.clips[index];out->id=c.id;out->track=c.track;out->pattern=c.pattern;out->start=c.start;out->length=c.length;out->offset=c.offset;out->muted=c.muted;return 1;}catch(...){return 0;}}
extern "C" int pt_pattern_index(uint64_t id){try{auto s=snapshot();for(unsigned i=0;i<s->library.entries.size();++i)if(s->library.entries[i].pattern.id()==id)return (int)i;}catch(...){}return -1;}
extern "C" int pt_arr_add_track(const char*name,uint64_t*out){if(!name||!out||::strnlen(name,32)>=32)return 0;Id id=0;int ok=arrange([&](Arrangement&a,const Library&l){return a.add_track(name,l,id);});if(ok)*out=id;return ok;}
extern "C" int pt_arr_add_clip(uint64_t track,unsigned pattern,int64_t start,int64_t length,uint64_t*out){if(!out)return 0;Id id=0;int ok=arrange([&](Arrangement&a,const Library&l){if(pattern>=l.entries.size())return Result::invalid;return a.add_clip(track,l.entries[pattern].pattern.id(),start,length,0,l,id);});if(ok)*out=id;return ok;}
extern "C" int pt_arr_move(uint64_t id,uint64_t track,int64_t start){return arrange([&](Arrangement&a,const Library&l){return a.move_clip(id,track,start,l);});}
extern "C" int pt_arr_copy(uint64_t source,uint64_t track,int64_t start,uint64_t*out){if(!out)return 0;Id id=0;int ok=arrange([&](Arrangement&a,const Library&l){return a.copy_clip(source,track,start,l,id);});if(ok)*out=id;return ok;}
extern "C" int pt_arr_paste(const pt_arr_clip*source,uint64_t track,int64_t start,uint64_t*out){if(!source||!out||source->muted>1)return 0;auto value=*source;Id id=0;int ok=arrange([&](Arrangement&a,const Library&l){auto result=a.add_clip(track,value.pattern,start,value.length,value.offset,l,id);if(result==Result::ok&&value.muted)result=a.clip_state(id,true,l);return result;});if(ok)*out=id;return ok;}
extern "C" int pt_arr_resize(uint64_t id,int64_t length){return arrange([&](Arrangement&a,const Library&l){return a.resize_clip(id,length,l);});}
extern "C" int pt_arr_cut(uint64_t id,int64_t cursor,uint64_t*out){if(!out)return 0;Id right=0;int ok=arrange([&](Arrangement&a,const Library&l){return a.cut_clip(id,cursor,l,right);});if(ok)*out=right;return ok;}
extern "C" int pt_arr_join(uint64_t first,uint64_t second){return arrange([&](Arrangement&a,const Library&l){return a.join_clips(first,second,l);});}
extern "C" int pt_arr_remove(uint64_t id){return arrange([&](Arrangement&a,const Library&l){return a.erase_clip(id,l);});}
extern "C" int pt_arr_track_state(uint64_t id,int muted,int solo){if((muted!=0&&muted!=1)||(solo!=0&&solo!=1))return 0;return arrange([&](Arrangement&a,const Library&l){return a.track_state(id,muted!=0,solo!=0,l);});}
extern "C" int pt_arr_length(int64_t length){return arrange([&](Arrangement&a,const Library&l){return a.song_length(length,l);});}
extern "C" int pt_pattern_assigned(unsigned index){try{auto s=snapshot();if(index>=s->library.entries.size())return 0;const auto&r=s->library.entries[index].pattern.routes();if(r.empty())return 0;for(const auto&t:r)if(t.program_key==unassigned_program_key)return 0;return 1;}catch(...){return 0;}}
extern "C" int pt_arr_new_pattern(uint64_t track,uint64_t route_template,int64_t start,int64_t length,uint64_t*out){
 if(!out)return 0;
 Id created=0;
 int ok=edit_state([&](State&s){
  if(start<0||length<=0||start>library_tick_limit||length>library_tick_limit-start)return Result::invalid;
  std::vector<Route>routes;routes.reserve(128);
  if(route_template){bool found=false;for(const auto&e:s.library.entries)if(e.pattern.id()==route_template){routes=e.pattern.routes();found=true;break;}if(!found)return Result::stale;}
  else for(unsigned pad=0;pad<128;++pad){Route r;r.program_key=unassigned_program_key;r.pad=pad;r.source_note=pad;routes.push_back(r);}
  auto result=s.library.empty(length,routes,getter());if(result!=Result::ok)return result;
  if(start+length>s.song.length){result=s.song.song_length(start+length,s.library);if(result!=Result::ok)return result;}
  Id row=track;if(!row){result=s.song.add_track("Track "+std::to_string(s.song.tracks.size()+1),s.library,row);if(result!=Result::ok)return result;}
  result=s.song.add_clip(row,s.library.entries.back().pattern.id(),start,length,0,s.library,created);
  if(result==Result::ok)s.arr_mode=1;
  return result;
 });if(ok)*out=created;return ok;
}
extern "C" int pt_arr_variation(uint64_t id){return edit_state([&](State&s){
 Id source=0;for(const auto&c:s.song.clips)if(c.id==id){source=c.pattern;break;}if(!source)return Result::invalid;
 unsigned index=0;while(index<s.library.entries.size()&&s.library.entries[index].pattern.id()!=source)++index;
 auto result=s.library.variation(index);if(result!=Result::ok)return result;
 result=s.song.rebind_clip(id,s.library.entries.back().pattern.id(),s.library);if(result==Result::ok)s.arr_mode=1;return result;
 });}
extern "C" int pt_arr_edit_clip(uint64_t id,uint64_t track,int64_t start,int64_t length,uint64_t revision){
 return arrange([&](Arrangement&a,const Library&l){return a.revision==revision?a.edit_clip(id,track,start,length,l):Result::stale;});
}
namespace am_pattern {
int pt_prepare_song(PreparedArrangement&out){try{auto generation=epoch.load();if(expected_epoch&&expected_epoch!=generation)throw std::runtime_error("Project changed: reopen Patterns");auto s=snapshot();if(s->mode==2||s->arr_mode==2)throw std::runtime_error("Newer song data: read only");
    PreparedArrangement prepared;auto result=prepare_arrangement(s->song,s->library,generation,prepared);if(result!=Result::ok)throw std::runtime_error(message(result));
    std::lock_guard<std::mutex>lock(mutex);if(epoch.load()!=generation||current!=s)throw std::runtime_error("Song changed: prepare again");out=std::move(prepared);return 1;
}catch(const std::exception&e){set_error(e.what());return 0;}}
bool pt_prepared_current(const PreparedArrangement&prepared){std::lock_guard<std::mutex>lock(mutex);return current&&prepared.matches(epoch.load(),current->song,current->library);}
int pt_prepare_native_plan(const NativeDestination&destination,NativeArrangementPlan&out){try{
    PreparedArrangement prepared;if(!pt_prepare_song(prepared))return 0;
    NativeArrangementPlan value;auto result=prepare_native_plan(prepared,destination,getter(),value);
    if(result!=NativePlanResult::ok){
        const char*reason=result==NativePlanResult::program_mismatch?"Arrange needs another Program":
            result==NativePlanResult::length_mismatch?"Native sequence length differs":
            result==NativePlanResult::mapping_unavailable?"Native pad mapping unavailable":
            result==NativePlanResult::transport_running?"Stop transport before preparing":
            result==NativePlanResult::transport_unavailable?"Native transport unavailable":
            result==NativePlanResult::stale?"Project changed: prepare again":"Arrange native plan refused";
        throw std::runtime_error(reason);
    }
    std::lock_guard<std::mutex>lock(mutex);
    if(!current||!prepared.matches(epoch.load(),current->song,current->library))throw std::runtime_error("Song changed: prepare again");
    out=std::move(value);error[0]=0;return 1;
}catch(const std::exception&e){set_error(e.what());return 0;}}
bool pt_native_plan_current(const NativeArrangementPlan&plan,const NativeDestination&destination){
    std::lock_guard<std::mutex>lock(mutex);if(!current||current->mode==2||current->arr_mode==2)return false;
    PreparedArrangement metadata;metadata.project_epoch=epoch.load();metadata.arrangement_revision=current->song.revision;
    metadata.library_revision=current->library.revision;metadata.length=current->song.length;return plan.matches(metadata,destination);
}
}
extern "C" void pt_archive_save(void*data,void*a,void(*original)(void*,void*)){
    auto s=snapshot();std::string payload=s->mode==2?s->opaque:s->mode==1?library_encode(s->library):"";
    std::string arrangement=s->arr_mode==2?s->arr_opaque:s->arr_mode==1?arrangement_encode(s->song,s->library):"";original(data,a);
    auto write=[&](const char*key,const std::string&value){struct {const char*data;std::size_t size,capacity,unused;}text={value.data(),value.size(),value.size(),0};reinterpret_cast<void(*)(void*,const char*,const void*)>(image+0x1d27d90)(a,key,&text);};
    if(s->mode)write(archive_key,payload);
    if(s->arr_mode)write(arrangement_key,arrangement);
}
extern "C" void pt_archive_load(void*data,void*a,void(*original)(void*,void*)){
    original(data,a);if(!transaction)return;try{if(transaction->captured)throw std::runtime_error("Multiple pattern objects");transaction->captured=true;transaction->value=capture_archive(a);}catch(...){transaction->failure=std::current_exception();}
}
extern "C" int pt_archive_read(void*data,void*json,int(*original)(void*,void*)){
    if(transaction)throw std::runtime_error("Nested pattern read");
    Transaction pending;pending.value=std::make_shared<State>();
    struct Scope {~Scope(){transaction=nullptr;}}scope;transaction=&pending;
    int ok=original(data,json);if(pending.failure)std::rethrow_exception(pending.failure);if(ok)publish(std::move(pending.value));return ok;
}
