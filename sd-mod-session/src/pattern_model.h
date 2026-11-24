#ifndef AM_PATTERN_MODEL_H
#define AM_PATTERN_MODEL_H
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
extern "C" {
#include "midi_event.h"
}

/* Control/UI-owner value model, linked by opt-in Pattern Tools only.
 * Native tag 3 is inline; no pointer/refcounted variant may enter this model.
 * These structs are NOT a disk format. IDs are caller-owned project identities,
 * not native tree pointers or mutable sequence indexes. Allocations may throw;
 * failure/exception preserves the old complete output. Callers must catch at
 * their command boundary. Never call from audio/render/input producers. */
namespace am_pattern {
using Id=std::uint64_t;
constexpr std::size_t max_events=32768,max_parts=128,max_routes=1024;
/* Editable value-only draft, never an implicit native Program binding. */
constexpr const char*unassigned_program_key="am:unassigned";
enum class Result {ok,invalid,unsupported,limit,stale};
enum class SplitBy {pads,mixer};
enum class BoundaryPolicy {onsets_only};
struct Route {
    std::string program_key;
    unsigned pad=0,source_note=0;
    int mixer_bus=-2; /* -2 unknown, -1 Master Direct, 0..7 A..H. */
};
struct InputEvent {step_event native{};unsigned route=0;};
struct Event {Id id=0;step_event native{};unsigned route=0;};
class Snapshot {
public:
    Id id()const{return id_;}
    std::uint64_t revision()const{return revision_;}
    std::int64_t length()const{return length_;}
    const std::string&source_key()const{return source_key_;}
    const std::vector<Route>&routes()const{return routes_;}
    const std::vector<Event>&events()const{return events_;}
    bool matches(const std::string&key,std::uint64_t revision)const;
    static Result capture(Id,const std::string&,std::uint64_t,std::int64_t,
        const std::vector<Route>&,const std::vector<InputEvent>&,am_midi_choice_get,
        std::shared_ptr<const Snapshot>&);
private:
    Id id_=0;std::uint64_t revision_=0;std::int64_t length_=0;
    std::string source_key_;std::vector<Route> routes_;std::vector<Event> events_;
};
struct Part {Id id=0;std::vector<Id> events;};
class Parts {
public:
    const std::shared_ptr<const Snapshot>&snapshot()const{return snapshot_;}
    const std::vector<Part>&parts()const{return parts_;}
    /* Manual membership is exclusive. Every source occurrence must appear
     * exactly once, including an explicit Unassigned part if desired. */
    static Result manual(std::shared_ptr<const Snapshot>,const std::vector<Part>&,Parts&);
    static Result split(std::shared_ptr<const Snapshot>,SplitBy,Parts&);
private:
    std::shared_ptr<const Snapshot> snapshot_;std::vector<Part> parts_;
};
struct PatternEvent {
    Id id=0,source_snapshot=0,source_event=0;
    step_event native{};unsigned route=0;
};
class Pattern {
public:
    Id id()const{return id_;}
    Id parent()const{return parent_;}
    std::uint64_t revision()const{return revision_;}
    std::int64_t length()const{return length_;}
    const std::vector<Route>&routes()const{return routes_;}
    const std::vector<PatternEvent>&events()const{return events_;}
    static Result empty(Id,std::int64_t,Pattern&);
    static Result variation(const Pattern&,Id,Pattern&);
    /* Validated value import for library/archive/editor transactions. */
    static Result restore(Id,Id,std::uint64_t,std::int64_t,const std::vector<Route>&,
        const std::vector<PatternEvent>&,am_midi_choice_get,Pattern&);
    Result replace(Id,const step_event&,std::uint64_t expected_revision,am_midi_choice_get);
private:
    friend Result extract(const Parts&,Id,std::int64_t,std::int64_t,BoundaryPolicy,Id,Pattern&,struct ExtractionInfo&);
    Id id_=0,parent_=0;std::uint64_t revision_=1;std::int64_t length_=0;
    std::vector<Route> routes_;std::vector<PatternEvent> events_;
};
struct ExtractionInfo {unsigned retained_tails=0,skipped_prior_gates=0;};
/* Half-open range [begin,end), starts rebased to zero; durations unchanged.
 * Prior sample tails cannot be inferred from a trigger's recorded gate. */
Result extract(const Parts&,Id,std::int64_t begin,std::int64_t end,
    BoundaryPolicy,Id pattern_id,Pattern&,ExtractionInfo&);
}
#endif
