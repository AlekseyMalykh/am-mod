#ifndef AM_MIXER_MODEL_H
#define AM_MIXER_MODEL_H
#include "fx_chain.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Control-owner data only. No UI, native pointers, audio buffers or globals.
 * This core is not yet attached to native project/archive/audio services. */
#define MIXER_TRACKS 8u
#define MIXER_PADS 128u
#define MIXER_NAME_BYTES 32u
#define MIXER_MAX_SCENES 128u
#define MIXER_PAYLOAD_BYTES 65536u
#define MIXER_LEVEL_MIN_DB (-60.f)
#define MIXER_LEVEL_MAX_DB 6.f
#define MIXER_MODEL_VERSION 1u
#define MIXER_COLOURS 12u
enum mixer_result { MIXER_OK, MIXER_INVALID, MIXER_FULL, MIXER_UNSUPPORTED,
                    MIXER_CORRUPT, MIXER_SHORT_BUFFER };
enum mixer_scene_field { MIXER_SCENE_LEVEL=1, MIXER_SCENE_PAN=2,
    MIXER_SCENE_SEND=4, MIXER_SCENE_MUTE=8, MIXER_SCENE_SOLO=16 };
struct mixer_mix {
    float level_db,pan,send; /* -inf = Off; pan -1..1, future send 0..1 */
    unsigned mute,solo;
};
struct mixer_channel {
    char name[MIXER_NAME_BYTES]; /* bounded, valid UTF-8 */
    uint32_t colour; /* RGB, not a native palette/LED pointer */
    struct mixer_mix mix;
    unsigned fx_bypass;
    struct fx_chain_config fx;
};
struct mixer_scene {
    /* Stable Song Part identity assigned by a future Song adapter. Never a
     * Sequence id or transient UI row. Native identity is not established yet. */
    uint64_t part_id;
    unsigned char fields[MIXER_TRACKS];
    struct mixer_mix track[MIXER_TRACKS];
};
struct mixer_project {
    struct mixer_channel track[MIXER_TRACKS],master;
    unsigned char output[MIXER_PADS]; /* 0=Master Direct, 1..8=Track */
    unsigned scene_count;
    struct mixer_scene scene[MIXER_MAX_SCENES];
};
void mixer_defaults(struct mixer_project*);
/* Optional UI palette uses the existing AM Mod hues. Persist actual RGB,
 * including older/custom values; palette indices are never stored. */
uint32_t mixer_colour_at(unsigned index);
int mixer_colour_index(uint32_t rgb);
const char*mixer_colour_name(uint32_t rgb); /* NULL for a custom RGB value. */
int mixer_project_valid(const struct mixer_project*);
enum mixer_result mixer_set_output(struct mixer_project*,unsigned pad,unsigned output);
enum mixer_result mixer_scene_set(struct mixer_project*,const struct mixer_scene*);
int mixer_scene_remove(struct mixer_project*,uint64_t part_id);
/* Always starts from the project mix, then applies this Part's optional
 * fields. Missing scenes do not inherit the previous Part. Never changes
 * project/Pad Output, and does not use UI selection. Returns 1 if applied. */
int mixer_scene_resolve(const struct mixer_project*,uint64_t part_id,
                        struct mixer_mix out[MIXER_TRACKS]);
/* Apply one bounded delta to all finite selected levels. Off stays Off.
 * Returns 0 for invalid/no finite selection; no mutation in that case. */
int mixer_shift_levels(struct mixer_project*,unsigned selected,float requested_db,float *applied_db);
/* Versioned, bounded, printable extension payload: explicit LE scalars,
 * hex envelope and CRC32; no struct dumps or locale-dependent numbers.
 * needed excludes the terminating NUL. NULL/0 queries exact encoded size.
 * Short-buffer/invalid encode does not touch output. Decode is transactional;
 * NULL/0 means an older project with no extension and yields defaults.
 * Unsupported future payloads must be retained by a future archive bridge,
 * never silently rewritten as defaults. Both functions run off the RT path. */
enum mixer_result mixer_encode(const struct mixer_project*,char*,size_t,size_t *needed);
enum mixer_result mixer_decode(const char*,size_t,struct mixer_project*);
#ifdef __cplusplus
}
#endif
#endif
