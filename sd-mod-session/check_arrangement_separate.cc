/* Store/library operations only; archive writer is an explicit receiver. */
#include "src/pattern_tools.h"
#include "src/pattern_library.h"
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <new>
#include <set>
using namespace am_pattern;
static int fail_after=-1;
__attribute__((noinline)) void*operator new(std::size_t n){if(fail_after==0){fail_after=-1;throw std::bad_alloc();}if(fail_after>0)--fail_after;void*p=std::malloc(n?n:1);if(!p)throw std::bad_alloc();return p;}
__attribute__((noinline)) void operator delete(void*p)noexcept{std::free(p);}
__attribute__((noinline)) void*operator new(std::size_t n,const std::nothrow_t&)noexcept{try{return ::operator new(n);}catch(...){return nullptr;}}
__attribute__((noinline)) void operator delete(void*p,const std::nothrow_t&)noexcept{::operator delete(p);}
static std::string lib_text,song_text;
static void writer(void*,const char*key,const void*value){const auto*data=static_cast<const char*const*>(value);std::size_t size;std::memcpy(&size,static_cast<const char*>(value)+8,8);if(!std::strcmp(key,"amPatternsV1"))lib_text.assign(*data,size);else{assert(!std::strcmp(key,"amArrangementV1"));song_text.assign(*data,size);}}
static void original(void*,void*){}
static std::pair<std::string,std::string>saved(){lib_text.clear();song_text.clear();pt_archive_save(nullptr,nullptr,original);return {lib_text,song_text};}
static pt_arr_clip clip(uint64_t id){pt_arr_clip c;assert(pt_arr_clip_by_id(id,&c));return c;}
static unsigned ordinal(uint64_t id){int index=pt_pattern_index(id);assert(index>=0);return unsigned(index);}
static std::vector<pr_note>notes(uint64_t id){pt_entry e;assert(pt_info(ordinal(id),&e));std::vector<pr_note>v(e.count);assert(pt_notes(ordinal(id),v.data(),v.size(),nullptr)==int(v.size()));return v;}
struct Fixture{uint64_t row,other,later,source,left,right,reference;};
static Fixture setup(){
 pt_reset();pt_expect_epoch(pt_epoch());Fixture f{};int mapping[128],outputs[128];for(int i=0;i<128;++i){mapping[i]=i;outputs[i]=i%3;}
 std::swap(mapping[0],mapping[36]);std::swap(mapping[16],mapping[52]);
 pr_note n[6]{};int pads[]={0,1,0,16,0,17};int64_t ticks[]={0,960,1920,3840,0,7200};
 for(unsigned i=0;i<6;++i){n[i].sound=pads[i];n[i].selected=1;n[i].event.type=3;n[i].event.time=ticks[i];n[i].event.data[32]=(unsigned char)mapping[pads[i]];n[i].event.data[34]=100;float velocity=.8f;std::memcpy(n[i].event.data,&velocity,4);int64_t gate=i==0||i==4?10000:i==5?2000:480;std::memcpy(n[i].event.data+24,&gate,8);}
 assert(pt_capture(n,6,7680,1,"source-program",mapping,outputs,2)&&pt_count()==1);pt_entry entry;assert(pt_info(0,&entry));f.source=entry.id;
 assert(pt_arr_add_track("Song",&f.row)&&pt_arr_add_track("Other",&f.other)&&pt_arr_add_track("Later",&f.later));
 assert(pt_arr_add_clip(f.row,0,3840,15360,&f.left)&&pt_arr_cut(f.left,5760,&f.right));
 assert(pt_arr_copy(f.right,f.other,0,&f.reference));return f;
}
static std::multiset<std::string>music(){PreparedArrangement p;assert(pt_prepare_song(p));std::multiset<std::string>r;for(const auto&n:p.notes){std::string key(reinterpret_cast<const char*>(&n.native),sizeof n.native);const auto&t=p.targets[n.target];key+=t.program_key+":"+std::to_string(t.pad)+":"+std::to_string(t.source_note);r.insert(std::move(key));}return r;}
int main(){
 pt_init(reinterpret_cast<uintptr_t>(writer)-0x1d27d90);
 auto f=setup();auto input=notes(f.source);for(auto&n:input)n.selected=n.sound==0;
 auto before=saved();auto sounding=music();auto geometry=clip(f.right);uint64_t receipt=pt_serial(),result=99;
 assert(pt_arr_separate(f.right,f.source,receipt,input.data(),input.size(),0,&result));auto selected=clip(result),remaining=clip(f.right);
 assert(selected.id!=f.right&&selected.pattern!=f.source&&remaining.pattern!=f.source&&remaining.pattern!=selected.pattern);
 assert(selected.start==geometry.start&&selected.length==geometry.length&&selected.offset==geometry.offset&&selected.offset==1920);
 assert(remaining.start==geometry.start&&remaining.length==geometry.length&&remaining.offset==geometry.offset);
 assert(clip(f.left).pattern==f.source&&clip(f.reference).pattern==f.source&&notes(f.source).size()==6);
 auto chosen=notes(selected.pattern),rest=notes(remaining.pattern);assert(chosen.size()==3&&rest.size()==3);
 for(const auto&n:chosen)assert(n.sound==0);
 for(const auto&n:rest)assert(n.sound!=0);
 assert(music()==sounding);pt_arr_track row;assert(pt_arr_track_at(1,&row)&&row.id==selected.track);assert(pt_arr_track_at(2,&row)&&row.id==f.other);
 auto encoded=saved();Library decoded;Arrangement decoded_song;assert(library_decode(encoded.first,nullptr,decoded)==Result::ok&&arrangement_decode(encoded.second,decoded,decoded_song)==Result::ok);
 for(const auto&e:decoded.entries)if(e.pattern.id()!=f.source){assert(e.pattern.parent()==f.source&&e.pattern.length()==7680&&e.pattern.routes().size()==128);for(const auto&n:e.pattern.events()){const auto&old=decoded.entries[0].pattern.events()[n.id-1];assert(n.source_snapshot==old.source_snapshot&&n.source_event==old.source_event&&!std::memcmp(&n.native,&old.native,sizeof n.native));}}
 assert(pt_history(-1)&&saved()==before&&pt_serial()!=receipt);assert(pt_history(1)&&saved()==encoded);assert(pt_history(-1));
 result=99;assert(!pt_arr_separate(f.right,f.source,receipt,input.data(),input.size(),0,&result)&&result==99&&saved()==before);
 auto current=pt_serial();auto damaged=input;damaged[0].event.time=1;assert(!pt_arr_separate(f.right,f.source,current,damaged.data(),damaged.size(),0,&result));
 assert(!pt_arr_separate(f.right,f.source,current,input.data(),input.size()-1,0,&result));
 damaged=input;for(auto&n:damaged)n.selected=0;
 assert(!pt_arr_separate(f.right,f.source,current,damaged.data(),damaged.size(),0,&result));assert(pt_serial()==current);
 assert(pt_arr_separate(f.right,f.source,current,input.data(),input.size(),1,&result));assert(clip(f.right).pattern==f.source&&clip(f.reference).pattern==f.source&&notes(clip(result).pattern).size()==3);
 assert(pt_history(-1)&&saved()==before);for(auto&n:input)n.selected=1;
 unsigned count_before=pt_count();assert(pt_arr_separate(f.right,f.source,pt_serial(),input.data(),input.size(),0,&result)&&result==f.right&&pt_count()==count_before+1&&clip(result).pattern!=f.source&&clip(f.reference).pattern==f.source&&clip(result).track!=f.row);
 assert(pt_history(-1)&&saved()==before);
 // Exact duplicate occurrences can also be separated individually.
 for(auto&n:input)n.selected=0;
 input[4].selected=1;
 assert(pt_arr_separate(f.right,f.source,pt_serial(),input.data(),input.size(),0,&result));assert(notes(clip(result).pattern).size()==1&&notes(clip(f.right).pattern).size()==5&&music()==sounding);
 assert(pt_history(-1));
 // Mute/Solo and clip mute follow the extracted material.
 assert(pt_arr_track_state(f.row,1,1));auto muted=clip(f.right);muted.muted=1;assert(pt_arr_remove(f.right)&&pt_arr_paste(&muted,f.row,muted.start,&f.right));
 assert(pt_arr_separate(f.right,f.source,pt_serial(),input.data(),input.size(),0,&result));assert(clip(result).muted);assert(pt_arr_track_at(1,&row)&&row.id==clip(result).track&&row.muted&&row.solo);
 f=setup();input=notes(f.source);for(auto&n:input)n.selected=n.sound==0;before=saved();receipt=pt_serial();bool completed=false;unsigned failures=0;
 for(int i=0;i<4096;++i){result=99;fail_after=i;int ok=pt_arr_separate(f.right,f.source,receipt,input.data(),input.size(),0,&result);fail_after=-1;if(ok){completed=true;break;}++failures;assert(result==99&&pt_serial()==receipt&&saved()==before);}
 assert(completed&&failures&&pt_history(-1)&&saved()==before);
 while(pt_count()<library_limit-1)assert(pt_variation(0));
 before=saved();result=99;receipt=pt_serial();assert(!pt_arr_separate(f.right,f.source,receipt,input.data(),input.size(),0,&result)&&saved()==before&&pt_serial()==receipt&&result==99);
 assert(pt_arr_separate(f.right,f.source,receipt,input.data(),input.size(),1,&result)&&pt_count()==library_limit);
 for(auto&n:input)n.selected=1;
 before=saved();result=99;assert(!pt_arr_separate(f.right,f.source,pt_serial(),input.data(),input.size(),0,&result)&&saved()==before&&result==99);
 assert(pt_arr_move(f.right,f.later,clip(f.right).start)&&pt_count()==library_limit);
 f=setup();input=notes(f.source);for(auto&n:input)n.selected=n.sound==0;uint64_t id;
 for(unsigned i=3;i<arrangement_track_limit;++i)assert(pt_arr_add_track(("T"+std::to_string(i)).c_str(),&id));
 before=saved();result=99;
 assert(!pt_arr_separate(f.right,f.source,pt_serial(),input.data(),input.size(),0,&result)&&saved()==before&&result==99);
 f=setup();input=notes(f.source);for(auto&n:input)n.selected=n.sound==0;
 pt_arrangement info;assert(pt_arr_info(&info));while(info.clips<arrangement_clip_limit){uint64_t extra;assert(pt_arr_add_clip(f.row,0,(int64_t)info.clips-3,1,&extra));assert(pt_arr_info(&info));}
 before=saved();result=99;assert(!pt_arr_separate(f.right,f.source,pt_serial(),input.data(),input.size(),0,&result)&&saved()==before&&result==99);
 for(auto&n:input)n.selected=1;
 assert(pt_arr_separate(f.right,f.source,pt_serial(),input.data(),input.size(),0,&result)&&pt_arr_info(&info)&&info.clips==arrangement_clip_limit);
 pt_reset();pt_expect_epoch(pt_epoch());assert(pt_arr_new_pattern(0,0,0,7680,&id));auto big_clip=clip(id);pt_entry big;assert(pt_info(0,&big));
 std::vector<step_event>many(PR_MAX_EVENTS);for(unsigned i=0;i<many.size();++i){many[i]=input[0].event;many[i].time=i%7680;many[i].data[32]=0;}
 pr_plan plan{};plan.revision=big.revision;plan.added=many.data();plan.added_count=many.size();assert(pt_commit(0,&plan));for(unsigned i=1;i<4;++i)assert(pt_variation(0));
 input=notes(big_clip.pattern);input[0].selected=1;before=saved();result=99;
 assert(!pt_arr_separate(id,big_clip.pattern,pt_serial(),input.data(),input.size(),1,&result)&&saved()==before&&result==99);
 for(auto&n:input)n.selected=1;
 assert(!pt_arr_separate(id,big_clip.pattern,pt_serial(),input.data(),input.size(),0,&result)&&saved()==before);
 uint64_t destination;assert(pt_arr_add_track("Move",&destination)&&pt_arr_move(id,destination,0));
 auto old=pt_serial();pt_reset();assert(pt_serial()!=old);pt_expect_epoch(0);result=99;assert(!pt_arr_separate(f.right,f.source,old,input.data(),input.size(),0,&result)&&!pt_count()&&result==99);
 std::cout<<"PASS Separate Store: selected rows/events, unchanged source and other references, no doubled musical events, repeated phase/period/tails, duplicate occurrence identity, independent Copy/all-selected detach, inherited mute/solo, one Undo, stale receipt including Undo/Redo, malformed input, library/track/clip/event limits, versioned codec and "<<failures<<" allocation failure points without partial publication. No native playback or physical device.\n";
}
