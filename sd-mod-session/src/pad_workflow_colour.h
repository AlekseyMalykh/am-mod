#ifndef PAD_WORKFLOW_COLOUR_H
#define PAD_WORKFLOW_COLOUR_H
#include "pad_palette.h"
#include "music_settings.h"
/* Unselected occupied destinations retain dim identity; empty slots stay off. */
static inline uint64_t pad_target_colour(unsigned pad,int used,int assigned,int current,uint64_t phase){
 if(!assigned&&!current)return pad_light(used?music_identity_rgb(pad<128?(unsigned)music_pad_colour(pad):0):0,used?.18f:0);
 return pad_target_light(used,assigned,current,phase);
}
/* Keep the source visible while choosing a destination. If roles overlap,
 * alternate white source with the red/green destination indication. */
static inline uint64_t pad_operation_colour(unsigned pad,int used,int source,int assigned,int current,uint64_t phase){
 if(source){
  if(!(assigned||current))return pad_source_light(used?music_identity_rgb(pad<128?(unsigned)music_pad_colour(pad):0):0,phase);
  /* With a steady cursor, destination safety has priority on an overlap. */
  if(music_mod_setting(MUSIC_SETTING_BLINK)&&music_cursor_on(phase,500))return pad_light(PAD_SELECT,PAD_BRIGHT_SELECT);
 }
 return pad_target_colour(pad,used,assigned,current,phase);
}
/* Source-page identity only. Target pages use pad_target_light for every pad. */
static inline uint64_t pad_workflow_colour(unsigned pad,int used,int source,int target,
                                           int overwrite,uint64_t phase){
 if(target)return pad_target_light(used||overwrite,1,1,phase);
 uint32_t hue=music_identity_rgb(pad<128?(unsigned)music_pad_colour(pad):0);
 if(source)return pad_source_light(used?hue:0,phase);
 return pad_light(used?hue:0,used?PAD_BRIGHT_CONTENT:0);
}
#endif
