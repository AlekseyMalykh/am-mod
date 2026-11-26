#ifndef AM_PATTERN_LIBRARY_H
#define AM_PATTERN_LIBRARY_H
#include "pattern_model.h"
#include <array>
namespace am_pattern {
constexpr unsigned library_limit=32;
constexpr std::int64_t library_tick_limit=INT64_C(960)*128*64;
/* link>=0: a live Sequence block mirroring stock Seq link+1; mask bit = muted pad. */
struct LibraryEntry {Pattern pattern;std::string name;bool part=false;int link=-1;std::array<unsigned char,16>mask{};};
struct Library {
    Id next=1;std::uint64_t revision=1;std::vector<LibraryEntry>entries;
    Result split(std::shared_ptr<const Snapshot>,const std::vector<Id>*,SplitBy,am_midi_choice_get);
    Result window(unsigned,std::int64_t,std::int64_t,am_midi_choice_get);
    Result variation(unsigned);
    Result empty(std::int64_t,const std::vector<Route>&,am_midi_choice_get);
    Result subset(unsigned,const std::vector<unsigned char>&,bool,am_midi_choice_get,Id&);
    Result edit(unsigned,std::uint64_t,const std::vector<step_event>&,const std::vector<step_event>&,am_midi_choice_get);
    Result erase(unsigned);
};
/* Versioned, bounded value codec; no native pointers or ABI padding. */
std::string library_encode(const Library&);
Result library_decode(const std::string&,am_midi_choice_get,Library&);
}
#endif
