extern "C" {
#include "src/midi_tools.h"
}
#define mixer_archive_native_probe prior_mixer_archive_native_probe
#include "check_mixer_archive_native.cc"
#undef mixer_archive_native_probe
extern "C" int mixer_archive_native_probe(uintptr_t base){
 assert(!prior_mixer_archive_native_probe(base));
 struct am_midi_map m={1,1,2,60,2,1,3,67};assert(am_midi_map_set(127,&m));
 struct am_midi_map levels={};levels.out_kind=AM_MIDI_NOTE;levels.out_number=60;levels.out_channel=7;
 assert(am_midi_layer_set(127,&levels,1));
 char bytes[AM_MIDI_PAYLOAD];assert(am_midi_encode(bytes,sizeof bytes)>0);std::string midi(bytes);
 std::string serialized=save();assert(serialized.find("\"amModMidiMapV1\"")!=std::string::npos);
 music_project musical{};music_defaults(&musical);std::vector<char>data(MUSIC_PAYLOAD_BYTES);int length=music_encode(&musical,data.data(),data.size());assert(length>0);std::string music(data.data(),length);
 am_midi_reset();Input load("",music);load.add("amModMidiMapV1",midi);read(load);struct am_midi_map result;assert(am_midi_map_get(127,&result)&&!std::memcmp(&result,&m,sizeof m));
 unsigned enabled=0;assert(am_midi_layer_get(127,&result,&enabled)&&enabled&&!std::memcmp(&result,&levels,sizeof levels));
 succeed=0;Input failed("",music);read(failed);succeed=1;assert(am_midi_map_get(127,&result)&&!std::memcmp(&result,&m,sizeof m));
 Input old_project("",music);read(old_project);assert(am_midi_map_get(127,&result)&&result.in_kind==0&&result.out_kind==0);
 assert(am_midi_layer_get(127,&result,&enabled)&&!enabled);
 assert(am_midi_map_set(127,&m));std::string bad="AMM1 broken";Input corrupt("",music);corrupt.add("amModMidiMapV1",bad);read(corrupt);assert(am_midi_map_get(127,&result)&&!result.in_kind&&!result.out_kind);
 /* Independent optional sequence mutes must coexist with AMM2 and the
  * expanded palette, and commit only after a successful native read. */
 assert(music_sequence_pad_toggle(3,17)==1);
 std::string with_mutes=save();assert(with_mutes.find("mpcSampleModSeqMutesV1")!=std::string::npos);
 music_project snapshot{};music_snapshot(&snapshot);char masks[SEQ_MUTES_PAYLOAD_BYTES];int size=music_seqmutes_encode(&snapshot,masks,sizeof masks);assert(size>0);
 std::string mute_payload(masks,size);Input muted("",music);muted.add("mpcSampleModSeqMutesV1",mute_payload);muted.add("amModMidiMapV1",midi);read(muted);
 assert(music_sequence_pad_muted(3,17));assert(am_midi_layer_get(127,&result,&enabled)&&enabled);
 succeed=0;Input failed_mutes("",music);read(failed_mutes);succeed=1;assert(music_sequence_pad_muted(3,17));
 std::string bad_mutes="AMSM1 broken";Input corrupt_mutes("",music);corrupt_mutes.add("mpcSampleModSeqMutesV1",bad_mutes);read(corrupt_mutes);assert(!music_sequence_pad_muted(3,17));
 assert(music_sequence_pad_toggle(3,17)==1);Input legacy_mutes("",music);read(legacy_mutes);assert(!music_sequence_pad_muted(3,17));
 std::puts("PASS MIDI extension through production native JSON archive: write, read commit, failed read, old project and corrupt payload");return 0;
}
