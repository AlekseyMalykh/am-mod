#ifndef AM_SYNTH_BINDING_H
#define AM_SYNTH_BINDING_H
#include "synth.h"
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
#define AM_SYNTH_PROGRAM_KEY_BYTES 128u
#define AM_SYNTH_BINDING_PAYLOAD_BYTES 340u
/* Native Program string identity (+dc8), not a display label, pointer, bus,
 * MIDI channel or input pad. Empty carrier is revalidated by native adapter.
 * One project Synth instrument in V1; no runtime voice IDs are serialized. */
struct am_synth_binding {char program_key[AM_SYNTH_PROGRAM_KEY_BYTES];unsigned char carrier,parameters[AM_SYNTH_PARAMS];};
enum am_synth_binding_result {AM_SYNTH_BINDING_OK,AM_SYNTH_BINDING_INVALID,AM_SYNTH_BINDING_BUFFER,AM_SYNTH_BINDING_UNSUPPORTED};
int am_synth_binding_valid(const struct am_synth_binding*);
int am_synth_binding_capture(const char*program_key,unsigned carrier,struct am_synth_binding*);
int am_synth_binding_matches(const struct am_synth_binding*,const char*program_key,unsigned carrier);
enum am_synth_binding_result am_synth_binding_encode(const struct am_synth_binding*,char*out,size_t capacity,size_t*size);
enum am_synth_binding_result am_synth_binding_decode(const char*,size_t,struct am_synth_binding*out);
#ifdef __cplusplus
}
#endif
#endif
