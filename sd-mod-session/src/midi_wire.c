#include "midi_wire.h"
#include <string.h>
#include <stdatomic.h>
static _Thread_local struct am_midi_wire current;
static _Atomic uint64_t next_id;
uint64_t am_midi_wire_id(void){
 uint64_t before=atomic_load(&next_id);
 for(unsigned attempt=0;attempt<256;++attempt){
  if(before==UINT64_MAX)return 0;
  if(atomic_compare_exchange_weak(&next_id,&before,before+1))return before+1;
 }
 return 0;
}
static int wire_valid(const struct am_midi_wire*w){return w&&w->id&&w->sound<128&&w->layer<2&&w->delta>=-120&&w->delta<=247;}
int am_midi_wire_wrap(void *record,unsigned char storage[AM_MIDI_WIRE_BYTES],const struct am_midi_wire *w){
 if(!record||!storage||!wire_valid(w))return 0;
 unsigned char *p=record;int size;memcpy(&size,p+16,4);unsigned kind=p[0]&0xf0;
 if(size!=3||(kind!=0x80&&kind!=0x90&&kind!=0xa0)||p[1]>127||p[2]>127)return 0;
 const unsigned char header[]={0xf0,0x7d,0x41,0x4d,0x4c,2};memcpy(storage,header,sizeof header);
 storage[6]=p[0]&127;storage[7]=p[1];storage[8]=p[2];storage[9]=(unsigned char)w->sound;storage[10]=(unsigned char)w->layer;
 unsigned delta=(unsigned)(w->delta+120);storage[11]=delta&127;storage[12]=delta>>7;
 for(unsigned i=0;i<10;++i)storage[13+i]=(unsigned char)((w->id>>(i*7))&127);
 unsigned checksum=0;for(unsigned i=1;i<23;++i)checksum^=storage[i];storage[23]=(unsigned char)checksum;storage[24]=0xf7;
 const void *data=storage;memcpy(p,&data,sizeof data);size=AM_MIDI_WIRE_BYTES;memcpy(p+16,&size,4);return 1;
}
int am_midi_wire_read(const void *message,struct am_midi_wire *out,struct am_midi_message *original){
 if(!message||!out)return 0;
 const unsigned char *p=message,*data;int size;memcpy(&size,p+16,4);
 if(size!=AM_MIDI_WIRE_BYTES)return 0;
 memcpy(&data,p,sizeof data);if(!data)return 0;
 const unsigned char header[]={0xf0,0x7d,0x41,0x4d,0x4c,2};
 if(memcmp(data,header,sizeof header)||data[24]!=0xf7||data[22]>1)return 0;
 unsigned checksum=0;for(unsigned i=1;i<23;++i){if(data[i]>127)return 0;checksum^=data[i];}if(data[23]!=checksum)return 0;
 unsigned kind=data[6]&0x70;if(kind!=0&&kind!=0x10&&kind!=0x20)return 0;
 uint64_t id=0;for(unsigned i=0;i<10;++i)id|=(uint64_t)data[13+i]<<(i*7);
 struct am_midi_wire w={id,data[9],data[10],(int)(data[11]|data[12]<<7)-120};if(!wire_valid(&w))return 0;
 *out=w;if(original)*original=(struct am_midi_message){3,{(uint8_t)(data[6]|128),data[7],data[8]}};return 1;
}
void am_midi_wire_begin(const struct am_midi_wire *w,struct am_midi_wire *previous){
 *previous=current;current=w?*w:(struct am_midi_wire){0};
}
void am_midi_wire_end(const struct am_midi_wire *previous){current=*previous;}
int am_midi_wire_current(struct am_midi_wire *out){if(!out||!current.id)return 0;*out=current;return 1;}
int am_midi_wire_send(struct am_midi_voices *voices,const struct am_midi_wire *w,
 unsigned port,const struct am_midi_message *in,am_midi_queue queue,void *user){
 if(!voices||!w||!w->id||!in||!queue||port<AM_MIDI_USB||port>AM_MIDI_EXTERNAL)return 0;
 unsigned kind=in->data[0]&0xf0;
 if(in->size!=3||(kind!=0x80&&kind!=0x90&&kind!=0xa0))return 0;
 if(am_midi_voices_retry(voices,queue,user)<0)return -1;
 int result;
 if(kind==0x80||(kind==0x90&&!in->data[2]))result=am_midi_voice_off(voices,w->id,queue,user);
 else if(kind==0xa0)result=am_midi_voice_pressure(voices,w->id,in->data[2],queue,user);
 else{
  struct am_midi_event event={.source_note=in->data[1],.velocity=in->data[2]};
  event.tune=(int16_t)(w->delta>120?120:w->delta);
  event.slice=(uint16_t)(w->delta>120?w->delta-120+1:0);
  struct am_midi_target target;
  if(!am_midi_route_event(w->sound,w->layer,port,1+(in->data[0]&15),&event,&target))return 1;
  result=am_midi_voice_target_on(voices,w->id,0,&target,queue,user);
 }
 return result<0?-1:1;
}
