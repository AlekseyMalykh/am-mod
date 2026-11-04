#include "src/pad_workflow_colour.h"
#include <assert.h>
#include <stdio.h>
#include "check_palette_receiver.h"
static unsigned colours[128];
int music_pad_lighting_mode(void){return 1;}
int music_pad_colour(unsigned pad){assert(pad<128);return (int)colours[pad];}
const uint32_t music_sequence_colours[MUSIC_SEQUENCE_COLOURS]={0x40ff40,0x32c780,0xffc247,0x8830ff,0xff20b0,0xff0000,0xff6000,0xffe000,0x2030ff,0x98ff00,0x00ffb0,0xffffff,0x00c8ff,0xff70b8};
int main(void){
 for(unsigned n=0;n<128;++n)for(unsigned c=0;c<MUSIC_SEQUENCE_COLOURS;++c){colours[n]=c;
  for(unsigned phase=0;phase<2000;phase+=40){
   assert(pad_workflow_colour(n,1,0,0,0,phase)==pad_light(music_identity_rgb(c),PAD_BRIGHT_CONTENT));
   assert(pad_workflow_colour(n,1,1,0,0,phase)==pad_source_light(music_identity_rgb(c),phase));
   assert((uint32_t)pad_workflow_colour(n,0,0,0,0,phase)==0);
   assert((uint32_t)pad_workflow_colour(n,0,0,1,0,phase)==PAD_FREE);
   assert((uint32_t)pad_workflow_colour(n,1,1,1,0,phase)==PAD_DANGER);
   for(unsigned used=0;used<2;++used)for(unsigned assigned=0;assigned<2;++assigned){
    assert((uint32_t)pad_operation_colour(n,used,1,assigned,0,phase)==(music_cursor_on(phase,500)?PAD_SELECT:assigned?(used?PAD_DANGER:PAD_FREE):used?music_identity_rgb(c):0));
    uint64_t background=pad_target_light(used,assigned,0,phase);
    assert((uint32_t)background==(assigned?(used?PAD_DANGER:PAD_FREE):used?PAD_CONTENT:0));
    uint64_t candidate=pad_target_colour(n,used,assigned,0,phase);
    assert((uint32_t)candidate==(assigned?(used?PAD_DANGER:PAD_FREE):used?music_identity_rgb(c):0));
    assert((uint32_t)pad_target_light(used,assigned,1,phase)==(used?PAD_DANGER:PAD_FREE));
   }
   assert(colours[n]==c);
  }
 }
 for(unsigned i=0;i<MUSIC_IDENTITY_CHOICES;++i){unsigned id=music_colour_choice(i);assert(id<MUSIC_SEQUENCE_COLOURS);for(unsigned j=0;j<i;++j)assert(music_colour_choice(j)!=id);}
 puts("PASS eight identity colours; all legacy IDs render safely; all128 pads, sources, free/occupied targets including source=target; metadata unchanged");return 0;
}
