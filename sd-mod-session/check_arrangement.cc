#include "src/arrangement.h"
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <new>
#include <tuple>
using namespace am_pattern;
static int fail_after=-1;
void*operator new(std::size_t n){if(fail_after==0){fail_after=-1;throw std::bad_alloc();}if(fail_after>0)--fail_after;void*p=std::malloc(n?n:1);if(!p)throw std::bad_alloc();return p;}
void operator delete(void*p)noexcept{std::free(p);}
void*operator new(std::size_t n,const std::nothrow_t&)noexcept{try{return ::operator new(n);}catch(...){return nullptr;}}
void operator delete(void*p,const std::nothrow_t&)noexcept{::operator delete(p);}
static PatternEvent note(Id id,std::int64_t tick,std::int64_t duration,unsigned pitch){
    PatternEvent e;e.id=id;e.native.time=tick;e.native.type=3|(6u<<12);float velocity=.75f;
    std::memcpy(e.native.data,&velocity,4);std::memcpy(e.native.data+24,&duration,8);e.native.data[32]=pitch;e.native.data[34]=73;e.native.data[36]=2;return e;
}
static void add(Library&l,Id id,std::int64_t length,unsigned pad,unsigned pitch,std::vector<PatternEvent>events){
    Route route;route.program_key="program";route.pad=pad;route.source_note=pitch;route.mixer_bus=3;
    LibraryEntry entry;entry.name="pattern";assert(Pattern::restore(id,0,1,length,{route},events,nullptr,entry.pattern)==Result::ok);l.entries.push_back(entry);l.next=id+1;++l.revision;
}
static std::string music(const PreparedArrangement&song){
    std::string result;
    for(const auto&n:song.notes){const auto&t=song.targets[n.target];result+=t.program_key+":"+std::to_string(t.pad)+":"+std::to_string(n.native.time)+":";
        result.append(reinterpret_cast<const char*>(&n.native.type),4);result.append(reinterpret_cast<const char*>(n.native.data),37);
    }return result;
}
int main(){
    Library library;add(library,101,7680,0,36,{note(1,0,120,36),note(2,3840,120,36)});
    add(library,102,15360,16,52,{note(1,0,20000,52)});
    add(library,103,30720,13,60,{note(1,0,30720,60),note(2,19200,480,60)});
    std::string source=library_encode(library);Arrangement song;Id drums,sample,synth,dclip,sclip,mclip;
    assert(song.add_track("Drums",library,drums)==Result::ok&&song.add_track("Sample",library,sample)==Result::ok&&song.add_track("Synth",library,synth)==Result::ok);
    assert(song.add_clip(drums,101,0,30720,0,library,dclip)==Result::ok);
    assert(song.add_clip(sample,102,0,30720,0,library,sclip)==Result::ok);
    assert(song.add_clip(synth,103,0,30720,0,library,mclip)==Result::ok);
    PreparedArrangement original;assert(prepare_arrangement(song,library,7,original)==Result::ok&&original.notes.size()==12&&original.targets.size()==3&&original.matches(7,song,library));
    for(const auto&n:original.notes)assert(n.native.data[34]==73&&(n.native.type>>12)==6&&n.native.data[36]==2);
    auto canonical=music(original);Id right=99;
    assert(song.cut_clip(dclip,9600,library,right)==Result::ok&&song.clips.size()==4);
    PreparedArrangement cut;assert(prepare_arrangement(song,library,7,cut)==Result::ok&&music(cut)==canonical&&!original.matches(7,song,library));
    assert(song.join_clips(dclip,right,library)==Result::ok&&song.clips.size()==3);
    assert(song.cut_clip(sclip,6000,library,right)==Result::ok&&prepare_arrangement(song,library,7,cut)==Result::ok&&music(cut)==canonical);
    for(const auto&n:cut.notes)assert(!(n.pattern==102&&n.native.time==6000));
    assert(song.join_clips(sclip,right,library)==Result::ok);
    assert(song.cut_clip(mclip,12345,library,right)==Result::ok&&prepare_arrangement(song,library,7,cut)==Result::ok&&music(cut)==canonical);
    assert(song.join_clips(mclip,right,library)==Result::ok);
    auto revision=song.revision;auto next=song.next;right=999;
    assert(song.cut_clip(dclip,0,library,right)==Result::invalid&&right==999&&song.next==next&&song.revision==revision);
    assert(song.copy_clip(dclip,drums,0,library,right)==Result::invalid&&right==999&&song.revision==revision);
    assert(song.move_clip(sclip,drums,0,library)==Result::invalid&&song.revision==revision);
    assert(song.song_length(7680,library)==Result::invalid&&song.length==30720);
    Id extra,copy;assert(song.add_track("Variation",library,extra)==Result::ok&&song.copy_clip(dclip,extra,0,library,copy)==Result::ok);
    assert(song.clip_state(copy,true,library)==Result::ok&&prepare_arrangement(song,library,7,cut)==Result::ok&&music(cut)==canonical);
    assert(song.erase_clip(copy,library)==Result::ok);
    assert(song.track_state(drums,true,false,library)==Result::ok&&prepare_arrangement(song,library,7,cut)==Result::ok&&cut.notes.size()==4);
    assert(song.track_state(synth,false,true,library)==Result::ok&&prepare_arrangement(song,library,7,cut)==Result::ok&&cut.notes.size()==2);
    assert(song.track_state(synth,true,true,library)==Result::ok&&prepare_arrangement(song,library,7,cut)==Result::ok&&cut.notes.empty());
    assert(song.track_state(synth,false,false,library)==Result::ok&&song.track_state(drums,false,false,library)==Result::ok);
    assert(prepare_arrangement(song,library,7,cut)==Result::ok&&music(cut)==canonical);
    Library deleted=library;assert(deleted.erase(0)==Result::ok);auto prior=music(cut);
    assert(prepare_arrangement(song,deleted,7,cut)==Result::stale&&music(cut)==prior&&!cut.matches(8,song,library));
    assert(library_encode(library)==source); /* All clip operations are references. */
    Library tiny;add(tiny,1,1,0,36,{note(1,0,120,36)});Arrangement dense;Id track,block;
    assert(dense.song_length(32769,tiny)==Result::ok&&dense.add_track("Dense",tiny,track)==Result::ok&&dense.add_clip(track,1,0,32769,0,tiny,block)==Result::ok);
    assert(prepare_arrangement(dense,tiny,1,cut)==Result::limit&&music(cut)==prior);
    assert(dense.resize_clip(block,32768,tiny)==Result::ok&&prepare_arrangement(dense,tiny,1,cut)==Result::ok&&cut.notes.size()==32768);
    unsigned failures=0;bool success=false;
    for(int point=0;point<256;++point){PreparedArrangement output=original;fail_after=point;
        try{auto result=prepare_arrangement(song,library,7,output);fail_after=-1;assert(result==Result::ok&&music(output)==canonical);success=true;break;}
        catch(const std::bad_alloc&){fail_after=-1;++failures;assert(music(output)==canonical&&output.arrangement_revision==original.arrangement_revision);}
    }assert(success&&failures);
    std::cout<<"PASS arrangement: parallel 2/4/8-bar references, bounded repeated-note preparation, cut/join preserves musical stream and held gates, copy/move/overlap rejection, mute/solo, missing-pattern/project revision rejection, 32768-note limit, "<<failures<<" allocation failures preserve prepared output; Library unchanged. Native playback/UI not connected.\n";
}
