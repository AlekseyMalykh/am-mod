#include "src/pattern_tools.h"
#include "src/pattern_library.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <cstdlib>
#include <new>
#include <stdexcept>
using namespace am_pattern;
static int fail_after=-1;
void*operator new(std::size_t size){if(fail_after==0){fail_after=-1;throw std::bad_alloc();}if(fail_after>0)--fail_after;void*p=std::malloc(size?size:1);if(!p)throw std::bad_alloc();return p;}
void operator delete(void*p)noexcept{std::free(p);}
void*operator new(std::size_t size,const std::nothrow_t&)noexcept{try{return ::operator new(size);}catch(...){return nullptr;}}
void operator delete(void*p,const std::nothrow_t&)noexcept{::operator delete(p);}
template<class T>void put(void*p,unsigned at,T value){std::memcpy(static_cast<char*>(p)+at,&value,sizeof value);}
static std::string written,arrangement_written;static unsigned writes;
static void writer(void*,const char*key,const void*value){auto data=*static_cast<const char*const*>(value);std::size_t size;std::memcpy(&size,static_cast<const char*>(value)+sizeof data,sizeof size);
    if(!std::strcmp(key,"amArrangementV1")){arrangement_written.assign(data,size);return;}
    assert(!std::strcmp(key,"amPatternsV1"));written.assign(data,size);++writes;
}
static void original(void*,void*){}
struct Input {
    alignas(8) char wrapper[4][16]{},archive[0x158]{},frame[0x38]{},members[144]{};
    std::string text,arrangement;unsigned count=1;
    explicit Input(std::string value):text(std::move(value)){
        for(unsigned i=0;i<4;++i)put<void*>(wrapper[i],8,i==3?archive:wrapper[i+1]);
        put<std::uintptr_t>(archive,0x150,reinterpret_cast<std::uintptr_t>(frame)+0x38);put<unsigned>(frame,0x30,1);
        string(members,"amPatternsV1",12);string(members+24,text.data(),text.size());bounds();
    }
    static void string(void*p,const char*s,std::size_t n){put<uint32_t>(p,0,n);put<const char*>(p,8,s);put<unsigned short>(p,22,1u<<10);}
    void bounds(){put<std::uintptr_t>(frame,0,reinterpret_cast<std::uintptr_t>(members));put<std::uintptr_t>(frame,8,reinterpret_cast<std::uintptr_t>(members)+count*48);}
    void song(const std::string&value){arrangement=value;string(members+48,"amArrangementV1",15);string(members+72,arrangement.data(),arrangement.size());count=2;bounds();}
};
static Input*input;static int success=1;
static int read_original(void*p,void*){pt_archive_load(p,input->wrapper,original);return success;}
static void read(Input&i){input=&i;assert(pt_archive_read(nullptr,nullptr,read_original)==success);}
static unsigned editing;
static int encode(void*,const pr_note*n,step_event*out){return pt_encode(editing,n,out);}
static int commit(void*,const pr_plan*plan){return pt_commit(editing,plan);}
static void load(piano_roll&p){pt_entry e;assert(pt_info(editing,&e));pr_note notes[PR_MAX_EVENTS];unsigned char loaded[128];int n=pt_notes(editing,notes,PR_MAX_EVENTS,loaded);assert(n>=0&&pr_load(&p,notes,n,e.length,e.revision,loaded));}
int main(){
    /* The host uses no packed choice masks/native setter. Only the archive
     * writer entry is redirected by choosing this test image base. */
    pt_init(reinterpret_cast<std::uintptr_t>(writer)-0x1d27d90);
    int map[128],outputs[128];for(int i=0;i<128;++i){map[i]=i;outputs[i]=i<16?1:2;}
    std::swap(map[0],map[36]);std::swap(map[16],map[52]);
    pr_note notes[3]{};for(unsigned i=0;i<3;++i){notes[i].event.type=3;float velocity=.8f;std::memcpy(notes[i].event.data,&velocity,4);put(&notes[i].event,0x28,int64_t(480));notes[i].event.data[34]=100;notes[i].sound=i<2?0:16;notes[i].event.data[32]=map[notes[i].sound];}
    notes[2].event.time=100;put(&notes[2].event,0x28,int64_t(10000));auto before=std::string(reinterpret_cast<char*>(notes),sizeof notes);
    assert(pt_capture(notes,3,7680,1,"program",map,outputs,0)&&pt_count()==2);
    pt_entry missing_entry;std::memset(&missing_entry,42,sizeof missing_entry);assert(!pt_info(999,&missing_entry)&&!missing_entry.name[0]&&!missing_entry.id);
    assert(!std::memcmp(before.data(),notes,sizeof notes));pt_entry original_entry;assert(pt_info(0,&original_entry)&&original_entry.count==2);
    assert(pt_pad_number(0,map[0])==0&&pt_pad_number(999,0)==-1&&pt_pad_number(0,128)==-1);
    assert(pt_variation(0)&&pt_count()==3);editing=2;piano_roll roll;assert(pr_init(&roll,{nullptr,encode,commit}));load(roll);
    pr_select_all(&roll);assert(pr_copy(&roll));assert(pr_edit(&roll,240,0,120,-20)&&pr_apply(&roll));load(roll);
    assert(roll.notes[0].event.time==240);pt_entry unchanged;assert(pt_info(0,&unchanged)&&unchanged.revision==original_entry.revision);
    assert(pt_history(-1));load(roll);assert(roll.notes[0].event.time==0);assert(pt_history(1));load(roll);assert(roll.notes[0].event.time==240);
    assert(pt_extract(1,0,3840)&&pt_count()==4);pr_note extracted[4];assert(pt_notes(3,extracted,4,nullptr)==1);int64_t tail;std::memcpy(&tail,extracted[0].event.data+24,8);assert(tail==10000);
    assert(pt_capture(notes,3,7680,1,"program",map,outputs,3));editing=4;pr_reset_view(&roll);load(roll);assert(!roll.count&&roll.clip_count==2);assert(pr_paste(&roll));load(roll);assert(roll.count==2);
    assert(pr_delete(&roll,0));load(roll);assert(!roll.count);assert(pt_info(0,&unchanged)&&unchanged.count==2);
    pt_archive_save(nullptr,nullptr,original);assert(writes==1&&written.compare(0,5,"AMP1 ")==0);std::string saved=written;
    Library decoded;assert(library_decode(saved,nullptr,decoded)==Result::ok&&decoded.entries.size()==5);
    assert(decoded.entries[2].pattern.parent()==decoded.entries[0].pattern.id());
    Library multi=decoded;auto routes=multi.entries[0].pattern.routes();routes[1].program_key="other-program";Pattern multi_pattern;auto&p=multi.entries[0].pattern;
    assert(Pattern::restore(p.id(),p.parent(),p.revision(),p.length(),routes,p.events(),nullptr,multi_pattern)==Result::ok);multi.entries[0].pattern=std::move(multi_pattern);
    Library keep=decoded;assert(library_decode(library_encode(multi),nullptr,keep)==Result::unsupported&&keep.entries.size()==decoded.entries.size());
    Library oversized=decoded;auto&big=oversized.entries[0].pattern;Pattern huge;
    assert(Pattern::restore(big.id(),big.parent(),big.revision(),library_tick_limit+1,big.routes(),big.events(),nullptr,huge)==Result::ok);big=std::move(huge);
    assert(library_decode(library_encode(oversized),nullptr,keep)==Result::unsupported&&keep.entries.size()==decoded.entries.size());
    for(std::size_t i=0;i<saved.size();++i){Library keep=decoded;assert(library_decode(saved.substr(0,i),nullptr,keep)!=Result::ok);assert(keep.entries.size()==decoded.entries.size());}
    auto bad=saved;bad[8]=bad[8]=='0'?'1':'0';Input corrupt(bad);input=&corrupt;bool refused=false;try{pt_archive_read(nullptr,nullptr,read_original);}catch(const std::exception&){refused=true;}assert(refused&&pt_count()==5);
    Input restored(saved);pt_reset();assert(!pt_count());read(restored);assert(pt_count()==5);pt_archive_save(nullptr,nullptr,original);assert(written==saved);
    Input missing("");missing.count=0;missing.bounds();success=0;read(missing);assert(pt_count()==5);success=1;read(missing);assert(!pt_count());writes=0;pt_archive_save(nullptr,nullptr,original);assert(!writes);
    Input future("AMP2 future opaque\n");read(future);assert(!pt_variation(0));pt_archive_save(nullptr,nullptr,original);assert(written==future.text);
    pt_reset();notes[0].selected=1;assert(pt_capture(notes,3,7680,1,"program",map,outputs,2)&&pt_count()==2);assert(pt_info(0,&unchanged)&&unchanged.count==1);assert(pt_info(1,&unchanged)&&unchanged.count==2);
    auto count=pt_count();outputs[0]=-1;assert(!pt_capture(notes,3,7680,1,"program",map,outputs,1)&&pt_count()==count);outputs[0]=1;
    assert(pt_capture(notes,3,7680,1,"program",map,outputs,1)&&pt_count()==count+2);
    Input duplicate(saved);std::memcpy(duplicate.members+48,duplicate.members,48);duplicate.count=2;duplicate.bounds();input=&duplicate;refused=false;try{pt_archive_read(nullptr,nullptr,read_original);}catch(const std::exception&){refused=true;}assert(refused&&pt_count()==4);
    pt_archive_save(nullptr,nullptr,original);std::string stable=written;unsigned failures=0;bool completed=false;
    for(int at=0;at<2048;++at){fail_after=at;int ok=pt_variation(0);fail_after=-1;
        if(ok){assert(pt_history(-1));completed=true;break;}
        ++failures;assert(pt_count()==4);pt_archive_save(nullptr,nullptr,original);assert(written==stable);
    }
    assert(completed&&failures>0);
    while(pt_count()<library_limit)assert(pt_variation(0));
    pt_archive_save(nullptr,nullptr,original);stable=written;
    assert(!pt_variation(0)&&pt_count()==library_limit);pt_archive_save(nullptr,nullptr,original);assert(written==stable);
    pt_reset();std::vector<pr_note>many(PR_MAX_EVENTS,notes[0]);std::vector<step_event>removed;removed.reserve(PR_MAX_EVENTS);
    for(unsigned i=0;i<many.size();++i){many[i].event.time=i%7680;removed.push_back(many[i].event);}
    assert(pt_capture(many.data(),many.size(),7680,1,"program",map,outputs,0)&&pt_count()==1);pt_entry large;assert(pt_info(0,&large)&&large.count==PR_MAX_EVENTS);
    pr_plan deletion{};deletion.revision=large.revision;deletion.removed=removed.data();deletion.removed_count=removed.size();
    assert(pt_commit(0,&deletion)&&pt_info(0,&large)&&!large.count);assert(pt_history(-1)&&pt_info(0,&large)&&large.count==PR_MAX_EVENTS);
    pt_expect_epoch(pt_epoch());pt_reset();assert(!pt_capture(notes,3,7680,1,"program",map,outputs,0)&&!pt_count());pt_expect_epoch(pt_epoch());assert(pt_capture(notes,3,7680,1,"program",map,outputs,0));
    uint64_t row=0,block=0,right=0;assert(pt_arr_add_track("Drums",&row));assert(pt_arr_add_clip(row,0,0,15360,&block));
    pt_arrangement info;assert(pt_arr_info(&info)&&info.tracks==1&&info.clips==1);
    pt_arr_track track_view;pt_arr_clip clip_view;assert(pt_arr_track_at(0,&track_view)&&track_view.id==row&&!std::strcmp(track_view.name,"Drums"));
    assert(pt_arr_clip_at(0,&clip_view)&&clip_view.id==block&&pt_pattern_index(clip_view.pattern)==0);assert(!pt_arr_clip_at(99,&clip_view)&&!clip_view.id);
    assert(!pt_remove(0)&&pt_count()==2);PreparedArrangement ready;assert(pt_prepare_song(ready)&&pt_prepared_current(ready)&&ready.notes.size()==4);
    NativeDestination native_destination;native_destination.project_epoch=pt_epoch();native_destination.routing_revision=1;native_destination.destination_revision=1;
    native_destination.program_key="program";native_destination.sequence_ticks=info.length;native_destination.transport_valid=true;
    for(unsigned i=0;i<128;++i){native_destination.pad_to_note[i]=map[i];native_destination.note_to_pad[(unsigned)map[i]]=int(i);}
    NativeArrangementPlan native_plan;assert(pt_prepare_native_plan(native_destination,native_plan)&&native_plan.notes.size()==4&&pt_native_plan_current(native_plan,native_destination));
    auto wrong_destination=native_destination;wrong_destination.playing=true;assert(!pt_prepare_native_plan(wrong_destination,native_plan)&&native_plan.notes.size()==4&&!pt_native_plan_current(native_plan,wrong_destination));
    assert(pt_arr_cut(block,3840,&right)&&!pt_prepared_current(ready));assert(pt_arr_join(block,right));
    assert(!pt_native_plan_current(native_plan,native_destination));
    pt_archive_save(nullptr,nullptr,original);std::string library_saved=written,song_saved=arrangement_written;assert(song_saved.compare(0,5,"AAR1 ")==0);
    Input combined(library_saved);combined.song(song_saved);char swap[48];std::memcpy(swap,combined.members,48);std::memcpy(combined.members,combined.members+48,48);std::memcpy(combined.members+48,swap,48);
    pt_reset();pt_expect_epoch(0);read(combined);assert(pt_arr_info(&info)&&info.clips==1&&pt_count()==2);assert(pt_prepare_song(ready));
    auto loaded_epoch=pt_epoch();success=0;read(combined);success=1;assert(pt_epoch()==loaded_epoch&&pt_prepared_current(ready));
    pt_expect_epoch(pt_epoch());assert(pt_arr_remove(block)&&pt_remove(0));assert(pt_arr_info(&info)&&!info.clips&&pt_count()==1);
    assert(pt_history(-1)&&pt_count()==2);assert(pt_history(-1)&&pt_arr_info(&info)&&info.clips==1);assert(pt_prepared_current(ready));
    read(combined);assert(!pt_prepared_current(ready));auto previous_size=ready.notes.size();assert(!pt_prepare_song(ready)&&ready.notes.size()==previous_size);pt_expect_epoch(pt_epoch());assert(pt_prepare_song(ready));
    Input broken(library_saved);broken.song(song_saved);broken.arrangement[8]=broken.arrangement[8]=='0'?'1':'0';Input::string(broken.members+72,broken.arrangement.data(),broken.arrangement.size());input=&broken;refused=false;
    try{pt_archive_read(nullptr,nullptr,read_original);}catch(const std::exception&){refused=true;}assert(refused&&pt_arr_info(&info)&&info.clips==1&&pt_count()==2);
    Input dangling("");dangling.count=0;dangling.bounds();dangling.song(song_saved);std::memmove(dangling.members,dangling.members+48,48);dangling.count=1;dangling.bounds();input=&dangling;refused=false;
    try{pt_archive_read(nullptr,nullptr,read_original);}catch(const std::exception&){refused=true;}assert(refused&&pt_count()==2);
    Input future_song(library_saved);future_song.song("AAR2 opaque song\n");read(future_song);pt_expect_epoch(pt_epoch());assert(pt_arr_info(&info)&&info.read_only&&!pt_remove(0)&&!pt_arr_add_track("New",&row));
    pt_archive_save(nullptr,nullptr,original);assert(arrangement_written==future_song.arrangement&&written==library_saved);
    pt_reset();pt_expect_epoch(0);pr_destroy(&roll);
    assert(!pt_native_plan_current(native_plan,native_destination));
    assert(pt_arr_info(&info)&&!info.tracks&&!info.clips&&!pt_count());
    assert(pt_capture(notes,3,7680,1,"program",map,outputs,0)&&pt_arr_add_track("Paste",&row));
    assert(pt_arr_add_clip(row,0,0,15360,&block)&&pt_arr_cut(block,3840,&right));
    assert(pt_arr_clip_at(1,&clip_view)&&clip_view.id==right&&clip_view.offset==3840);
    assert(pt_arr_remove(right)&&pt_arr_paste(&clip_view,row,3840,&right));
    assert(pt_arr_clip_at(1,&clip_view)&&clip_view.offset==3840&&clip_view.length==11520);
    uint64_t untouched=42;assert(!pt_arr_paste(&clip_view,row,3840,&untouched)&&untouched==42);
    assert(pt_arr_copy(right,row,15360,&block));pt_reset();
    pt_expect_epoch(pt_epoch());unsigned new_failures=0;bool new_done=false;
    for(int point=0;point<1024;++point){uint64_t out=42;fail_after=point;int ok=pt_arr_new_pattern(0,0,0,3840,&out);fail_after=-1;
        if(ok){new_done=true;block=out;break;}
        ++new_failures;assert(out==42&&!pt_count()&&pt_arr_info(&info)&&!info.tracks&&!info.clips);
    }
    assert(new_done&&new_failures&&pt_count()==1&&!pt_pattern_assigned(0));
    assert(pt_arr_info(&info)&&info.tracks==1&&info.clips==1&&pt_arr_clip_at(0,&clip_view));row=clip_view.track;
    assert(pt_history(-1)&&!pt_count()&&pt_arr_info(&info)&&!info.tracks&&!info.clips);
    assert(pt_history(1)&&pt_arr_clip_at(0,&clip_view));block=clip_view.id;
    uint64_t unassigned=clip_view.pattern;
    assert(pt_arr_copy(block,row,7680,&right));assert(pt_arr_variation(right)&&pt_count()==2);
    assert(pt_arr_clip_at(0,&clip_view)&&clip_view.pattern==unassigned);assert(pt_arr_clip_at(1,&clip_view)&&clip_view.pattern!=unassigned);
    assert(pt_history(-1)&&pt_count()==1&&pt_arr_clip_at(1,&clip_view)&&clip_view.pattern==unassigned);
    assert(pt_arr_info(&info));auto edit_revision=info.revision;
    assert(!pt_arr_edit_clip(block,row,7680,960,edit_revision));assert(pt_arr_info(&info)&&info.revision==edit_revision);
    assert(!pt_arr_edit_clip(block,row,960,960,edit_revision+1));assert(pt_arr_edit_clip(block,row,960,960,edit_revision));
    assert(pt_history(-1)&&pt_arr_clip_at(0,&clip_view)&&clip_view.start==0&&clip_view.length==3840);
    uint64_t invalid_output=55;unsigned before_new=pt_count();assert(!pt_arr_new_pattern(row,unassigned,7680,3840,&invalid_output)&&invalid_output==55&&pt_count()==before_new);
    pt_archive_save(nullptr,nullptr,original);Input new_archive(written);new_archive.song(arrangement_written);pt_reset();pt_expect_epoch(0);read(new_archive);
    assert(pt_count()==1&&pt_arr_info(&info)&&info.clips==2&&!pt_pattern_assigned(0));
    pt_reset();pt_expect_epoch(0);assert(pt_capture(notes,3,7680,1,"program",map,outputs,0));pt_entry route_source;assert(pt_info(0,&route_source));
    assert(pt_arr_new_pattern(0,route_source.id,30720,7680,&block)&&pt_pattern_assigned(pt_count()-1));
    assert(pt_arr_info(&info)&&info.length==38400);assert(pt_history(-1)&&pt_count()==2&&pt_arr_info(&info)&&info.length==30720&&!info.tracks&&!info.clips);
    pt_reset();pt_expect_epoch(0);
    std::cout<<"PASS direct creation Store: atomic track/pattern/clip and song extension, route-only inheritance, unassigned drafts, archive roundtrip, unique variation per reference, staged clip edit/overlap/stale refusal and "<<new_failures<<" allocation failures without orphan data.\n";
    std::cout<<"PASS arrangement store: shared history, protected references, cut/join, value readers, prepared epoch guard, order-independent archive fields, failed/corrupt/dangling/future loads, reset. Native archive fixtures only.\n";
    std::cout<<"PASS pattern tools: copied source/pads/mixer/manual duplicate split, independent variation, existing Piano Roll edit/copy/paste/delete/history, empty pattern, retained tails, versioned archive roundtrip, legacy/failed/corrupt/duplicate/future fields; "<<failures<<" allocation failures and library limit preserve prior state; source unchanged. Host native archive frame/writer fixtures; hardware not tested.\n";
}
