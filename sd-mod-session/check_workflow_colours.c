#include "src/pad_workflow_colour.h"
#include <assert.h>
#include <stdio.h>
static unsigned colours[128];
int music_pad_colour(unsigned pad){assert(pad<128);return (int)colours[pad];}
const uint32_t music_sequence_colours[MUSIC_SEQUENCE_COLOURS]={0x40ff40,0x32c780,0xffc247,0x8830ff,0xff20b0,0xff0000,0xff6000,0xffe000,0x2030ff,0x98ff00,0x00ffb0,0xffffff,0x00c8ff};
int main(void){
 for(unsigned n=0;n<128;++n)for(unsigned c=0;c<MUSIC_SEQUENCE_COLOURS;++c){
  colours[n]=c;
  for(unsigned phase=0;phase<1280;phase+=80){
   uint64_t used=pad_workflow_colour(n,1,0,0,0,phase);
   assert((uint32_t)used==(c?music_sequence_colours[c]:PAD_OCCUPIED));
   assert((uint32_t)pad_workflow_colour(n,1,1,0,0,phase)==(c?music_sequence_colours[c]:PAD_CONTENT));
   assert((uint32_t)pad_workflow_colour(n,0,0,0,0,phase)==0);
   assert((uint32_t)pad_workflow_colour(n,0,0,1,0,phase)==(c&&phase%640>=240?music_sequence_colours[c]:PAD_SELECT));
   assert((uint32_t)pad_workflow_colour(n,1,0,1,1,phase)==PAD_DANGER);
   assert(colours[n]==c);
  }
 }
 puts("PASS workflow hue policy: 128 pads/all13 choices, source/occupied/empty, white-hue target cycle, red overwrite warning and metadata unchanged");
 return 0;
}
