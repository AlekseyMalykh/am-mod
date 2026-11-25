#ifndef AM_ARRANGEMENT_NATIVE_PLAN_H
#define AM_ARRANGEMENT_NATIVE_PLAN_H
#include "arrangement.h"
#include <array>
namespace am_pattern {
/* Value-only preflight for the first single-native-Track backend. A future
 * owner must obtain these values under a real Project/Program/Sequence lease.
 * Neither this struct nor matches() provides such a lease or opens playback. */
struct NativeDestination {
    std::uint64_t project_epoch=0,routing_revision=0,destination_revision=0;
    std::string program_key;
    std::int64_t sequence_ticks=0;
    bool transport_valid=false,playing=false,recording=false;
    std::array<int,128>pad_to_note,note_to_pad;
    NativeDestination(){pad_to_note.fill(-1);note_to_pad.fill(-1);}
};
enum class NativePlanResult {ok,invalid,limit,stale,transport_unavailable,
    transport_running,length_mismatch,program_mismatch,mapping_unavailable};
struct NativeBoundNote {
    step_event native{};
    Id track=0,clip=0,pattern=0,event=0;
    std::uint64_t cycle=0;
    unsigned pad=0,source_note=0;
};
struct NativeArrangementPlan {
    NativeDestination destination;
    std::uint64_t arrangement_revision=0,library_revision=0;
    /* Runtime Store receipt, assigned only by pt_prepare_native_plan. Zero
     * for the standalone compiler. Not serialized and not a native lease. */
    std::uint64_t store_receipt=0;
    std::vector<NativeBoundNote>notes;
    bool matches(const PreparedArrangement&,const NativeDestination&)const;
};
/* Control owner only, before any native mutation. Rebind the physical pad to
 * its current native note; retain recorded pitch choices and source identity.
 * Requires an exact tick length, not a guessed 128*4/4 limit. Preserves notes,
 * duplicates, gates/tails and inline choices; does not expand repeat/chance.
 * Failure/exception preserves out. No pointers, native calls, clock or DSP. */
NativePlanResult prepare_native_plan(const PreparedArrangement&,
    const NativeDestination&,am_midi_choice_get,NativeArrangementPlan&);
}
#endif
