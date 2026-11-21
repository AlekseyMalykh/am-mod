#include "midi_route.h"
#include <string.h>
int am_midi_route_event(unsigned sound,unsigned layer,unsigned port,unsigned channel,
                        const struct am_midi_event*event,struct am_midi_target*out){
 if(!event||!out||sound>=128||layer>AM_MIDI_LAYER_LEVELS||port<AM_MIDI_USB||port>AM_MIDI_EXTERNAL||
    !channel||channel>16||event->off||event->muted||!event->velocity||event->velocity>127||event->source_note>127)return 0;
 struct am_midi_map m;if(!am_midi_layer_resolve(sound,layer,&m))return 0;
 struct am_midi_target target={0};target.port=port;
 unsigned note=event->source_note,ch=channel,kind=m.out_kind;
 if(m.out_port&&m.out_port!=port){memset(&m,0,sizeof(m));kind=AM_MIDI_NATIVE;layer=AM_MIDI_LAYER_PAD;}
 if(kind==AM_MIDI_OFF)return 0;
 if(m.out_channel)ch=m.out_channel;
 if(kind==AM_MIDI_NATIVE||kind==AM_MIDI_NOTE){
  unsigned base=kind==AM_MIDI_NOTE?m.out_number:event->source_note;
  if(layer==AM_MIDI_LAYER_LEVELS){if(!am_midi_event_target(event,base,&note))return 0;}else note=base;
  target.on=(struct am_midi_message){3,{(uint8_t)(0x90|((ch-1)&15)),(uint8_t)note,event->velocity}};
  target.off=(struct am_midi_message){3,{(uint8_t)(0x80|((ch-1)&15)),(uint8_t)note,0}};
 }else if(kind==AM_MIDI_CC){
  target.on=(struct am_midi_message){3,{(uint8_t)(0xb0|((ch-1)&15)),m.out_number,event->velocity}};
  target.off=target.on;target.off.data[2]=0;
 }else if(kind==AM_MIDI_PROGRAM){target.on=(struct am_midi_message){2,{(uint8_t)(0xc0|((ch-1)&15)),m.out_number,0}};}
 else return 0;
 *out=target;return 1;
}
