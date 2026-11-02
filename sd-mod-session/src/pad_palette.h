#ifndef PAD_PALETTE_H
#define PAD_PALETTE_H
/* One meaning per pad colour in every mod working window (Steps, event
 * editor, Batch Copy/Extract, Stereo->Mono, resample target picker).
 * Musical palettes (Lights, Chords, sequence colours) are separate.
 *
 *   content   green   pad/step has a sound or events; a copy source
 *   select    white   cursor, chosen value, target
 *   playhead  cyan    playback position only
 *   original  amber   the original value being replaced
 *   danger    red     will be erased or overwritten (blinks)
 *   muted     violet  muted / disabled
 *   occupied  white   has content but is not part of the operation (dim)
 *   empty     grey    nothing there (very dim)
 *   context   white   events of other sounds (dim)
 *
 * Red and green are never the only difference: danger blinks, content is
 * steady, muted and occupied are dim. */
#include <stdint.h>
#include <string.h>

#define PAD_CONTENT  0x32c780u
#define PAD_SELECT   0xffffffu
#define PAD_PLAYHEAD 0x00c8ffu
#define PAD_ORIGINAL 0xffc247u
#define PAD_DANGER   0xff2020u
#define PAD_MUTED    0x8830ffu
#define PAD_OCCUPIED 0xffffffu
#define PAD_EMPTY    0x303030u
#define PAD_CONTEXT  0xffffffu

#define PAD_BRIGHT_SELECT   .90f
#define PAD_BRIGHT_CONTENT  .55f
#define PAD_BRIGHT_ORIGINAL .70f
#define PAD_BRIGHT_DANGER   .80f
#define PAD_BRIGHT_MUTED    .45f
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
{ return phase%640u<240u?pad_light(PAD_SELECT,PAD_BRIGHT_SELECT):pad_light(rgb,brightness); }
#endif
