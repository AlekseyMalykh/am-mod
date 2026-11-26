#ifndef AM_ARRANGEMENT_H
#define AM_ARRANGEMENT_H
#include "pattern_library.h"
namespace am_pattern {
/* Value model on the control owner. No native pointers, transport thread,
 * audio callback or native Sequence mutations. Pattern IDs refer to Library. */
constexpr unsigned arrangement_track_limit=32,arrangement_clip_limit=512;
struct ArrangementTrack {Id id=0;std::string name;bool muted=false,solo=false;};
struct ArrangementClip {
    Id id=0,track=0,pattern=0;
    std::int64_t start=0,length=0,offset=0;
    bool muted=false;
};
struct Arrangement {
    Id next=1;std::uint64_t revision=1;
    std::int64_t length=30720; /* Eight 4/4 bars initially; all data is ticks. */
    std::vector<ArrangementTrack>tracks;std::vector<ArrangementClip>clips;
    Result validate(const Library&)const;
    Result add_track(const std::string&,const Library&,Id&);
    Result add_clip(Id,Id,std::int64_t,std::int64_t,std::int64_t,const Library&,Id&);
    Result copy_clip(Id,Id,std::int64_t,const Library&,Id&);
    Result move_clip(Id,Id,std::int64_t,const Library&);
    Result resize_clip(Id,std::int64_t,const Library&);
    Result edit_clip(Id,Id,std::int64_t,std::int64_t,const Library&);
    Result rebind_clip(Id,Id,const Library&);
    Result cut_clip(Id,std::int64_t,const Library&,Id&);
    Result join_clips(Id,Id,const Library&);
    Result erase_clip(Id,const Library&);
    Result erase_track(Id,const Library&);
    Result track_state(Id,bool,bool,const Library&);
    Result clip_state(Id,bool,const Library&);
    Result song_length(std::int64_t,const Library&);
};
struct InstrumentTarget {std::string program_key;unsigned pad=0,source_note=0;};
struct ScheduledNote {
    step_event native{};
    Id track=0,clip=0,pattern=0,event=0;
    std::uint64_t cycle=0;unsigned target=0,track_order=0;
};
struct PreparedArrangement {
    std::uint64_t project_epoch=0,arrangement_revision=0,library_revision=0;
    std::int64_t length=0;
    std::vector<InstrumentTarget>targets;std::vector<ScheduledNote>notes;
    bool matches(std::uint64_t,const Arrangement&,const Library&)const;
};
/* Bounded preparation, before playback on the control owner. Emits note
 * onsets in [clip.start,clip.end); preserves gates beyond a clip boundary.
 * No sample seek/retrigger for a gate whose onset precedes the clip window.
 * Failure/exception preserves the previous prepared output. */
Result prepare_arrangement(const Arrangement&,const Library&,std::uint64_t,PreparedArrangement&);
std::string arrangement_encode(const Arrangement&,const Library&);
Result arrangement_decode(const std::string&,const Library&,Arrangement&);
}
#endif
