#ifndef PAD_WORKFLOW_COLOUR_H
#define PAD_WORKFLOW_COLOUR_H
#include "pad_palette.h"
#include "music_settings.h"
/* Existing sample metadata is read only. Service cursors briefly overlay
 * the hue; overwrite warnings remain red and pulse even for red assignments. */
static inline uint64_t pad_workflow_colour(unsigned pad,int used,int source,int target,
                                           int overwrite,uint64_t phase){
 int assigned=pad<128?music_pad_colour(pad):0;
 uint32_t hue=assigned?music_sequence_colours[assigned]:source?PAD_CONTENT:PAD_OCCUPIED;
 uint32_t rgb=used||source?hue:0;
 float strength=source||assigned?PAD_BRIGHT_CONTENT:PAD_BRIGHT_OCCUPIED;
 if(target){
  if(overwrite){rgb=PAD_DANGER;strength=pad_blink_on(phase)?PAD_BRIGHT_SELECT:.25f;}
  else{rgb=assigned&&phase%640>=240?hue:PAD_SELECT;strength=PAD_BRIGHT_SELECT;}
 }
 return pad_light(rgb,strength);
}
#endif
