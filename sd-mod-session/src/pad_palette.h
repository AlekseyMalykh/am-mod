#ifndef PAD_PALETTE_H
#define PAD_PALETTE_H
/* One meaning per pad colour in every mod working window (Steps, event
 * editor, Batch Copy/Extract, Stereo->Mono, resample target picker).
 * Musical palettes (Lights, Chords, sequence colours) are separate.
 *
 *   content   blue    pad/step has a sound or events; a copy source
 *   select    white   cursor, chosen value, source selection
 *   playhead  cyan    playback position only
 *   original  amber   the original value being replaced
 *   danger    red     will be erased or overwritten (blinks)
 *   muted     grey    muted / disabled
 *   occupied  white   has content but is not part of the operation (dim)
 *   empty     grey    nothing there (very dim)
 *   context   white   events of other sounds (dim)
 *
 * Red and green are never the only difference: danger blinks, content is
 * steady, muted and occupied are dim. */
#include <stdint.h>
#include <string.h>
#include "music_settings.h"

#define PAD_CONTENT  music_identity_rgb(0)
#define PAD_FREE     music_service_rgb(MUSIC_SERVICE_FREE)
#define PAD_SELECT   music_service_rgb(MUSIC_SERVICE_SELECT)
#define PAD_PLAYHEAD music_service_rgb(MUSIC_SERVICE_PLAYHEAD)
#define PAD_ORIGINAL music_service_rgb(MUSIC_SERVICE_ORIGINAL)
#define PAD_DANGER   music_service_rgb(MUSIC_SERVICE_DANGER)
#define PAD_MUTED    music_service_rgb(MUSIC_SERVICE_MUTED)
#define PAD_OCCUPIED 0xffffffu
#define PAD_EMPTY    0x303030u
#define PAD_CONTEXT  0xffffffu

#define PAD_BRIGHT_SELECT   .90f
#define PAD_BRIGHT_CONTENT  .45f
#define PAD_BRIGHT_ORIGINAL .70f
#define PAD_BRIGHT_DANGER   .80f
#define PAD_BRIGHT_MUTED    .12f
#define PAD_BRIGHT_OCCUPIED .12f
#define PAD_BRIGHT_EMPTY    .08f
#define PAD_BRIGHT_CONTEXT  .035f

/* Danger blink: on for 300 ms, dimmed for 200 ms. */
static inline int pad_blink_on(uint64_t milliseconds){return milliseconds%500u<300u;}

static inline uint64_t pad_light(uint32_t rgb,float brightness)
{
    uint32_t bits;memcpy(&bits,&brightness,4);return (uint64_t)rgb|((uint64_t)bits<<32);
}
/* Selection is white briefly, then reveals the semantic/assigned base.
 * The dim phase also distinguishes selected white pads from ordinary white. */
static inline uint64_t pad_selection_light(uint32_t rgb,float brightness,uint64_t phase)
{ return music_cursor_on(phase,150)?pad_light(PAD_SELECT,PAD_BRIGHT_SELECT):pad_light(rgb,brightness); }
static inline uint64_t pad_source_light(uint32_t rgb,uint64_t phase){
 return music_cursor_on(phase,500)?pad_light(PAD_SELECT,PAD_BRIGHT_SELECT):pad_light(rgb,PAD_BRIGHT_CONTENT);
}
/* Blinking white/identity focus, with equal400ms phases. Phase starts anew
 * when Sample selection changes, rather than depending on global uptime. */
static inline uint64_t pad_focus_light(uint32_t rgb,float brightness,uint64_t phase){
 return music_cursor_on(phase,500)?pad_light(PAD_SELECT,.95f):pad_light(rgb,brightness>.35f?.35f:brightness);
}
/* Only assigned/current destinations get service red/green. */
static inline uint64_t pad_target_light(int used,int assigned,int current,uint64_t phase){
 if(!assigned&&!current)return pad_light(used?PAD_CONTENT:0,used?.18f:0);
 return pad_light(used?PAD_DANGER:PAD_FREE,current?(phase%800u<200u?.90f:.30f):assigned?.48f:.18f);
}
#endif
