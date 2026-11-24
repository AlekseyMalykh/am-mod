#ifndef AM_PATTERN_LIBRARY_H
#define AM_PATTERN_LIBRARY_H
#include "pattern_model.h"
namespace am_pattern {
constexpr unsigned library_limit=32;
constexpr std::int64_t library_tick_limit=INT64_C(960)*128*64;
struct LibraryEntry {Pattern pattern;std::string name;bool part=false;};
struct Library {
    Id next=1;std::uint64_t revision=1;std::vector<LibraryEntry>entries;
    Result split(std::shared_ptr<const Snapshot>,const std::vector<Id>*,SplitBy,am_midi_choice_get);
    Result window(unsigned,std::int64_t,std::int64_t,am_midi_choice_get);
    Result variation(unsigned);
    Result empty(std::int64_t,const std::vector<Route>&,am_midi_choice_get);
    Result edit(unsigned,std::uint64_t,const std::vector<step_event>&,const std::vector<step_event>&,am_midi_choice_get);
    Result erase(unsigned);
};
/* Versioned, bounded value codec; no native pointers or ABI padding. */
std::string library_encode(const Library&);
Result library_decode(const std::string&,am_midi_choice_get,Library&);
}
#endif
