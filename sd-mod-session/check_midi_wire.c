#include "src/midi_wire.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
int main(void){
 uint64_t old=am_midi_wire_id();assert(old);
 for(unsigned i=0;i<70000;++i){uint64_t id=am_midi_wire_id();assert(id>old);old=id;}
 unsigned char message[32]={0x92,50,101},storage[AM_MIDI_WIRE_BYTES];int size=3;double stamp=123.75;unsigned milliseconds=1234002,offset=96;
 memcpy(message+8,&stamp,8);memcpy(message+16,&size,4);memcpy(message+24,&milliseconds,4);memcpy(message+28,&offset,4);
 unsigned char before[32];memcpy(before,message,32);
 for(unsigned sound=0;sound<128;++sound)for(int delta=-120;delta<=247;++delta){
  struct am_midi_wire w={UINT64_C(0xfedcba9876543210),sound,sound&1,delta},decoded;struct am_midi_message musical;
  memcpy(message,before,32);assert(am_midi_wire_wrap(message,storage,&w)&&am_midi_wire_read(message,&decoded,&musical));
  assert(decoded.id==w.id&&decoded.sound==sound&&decoded.layer==(sound&1)&&decoded.delta==delta);
  assert(musical.size==3&&!memcmp(musical.data,before,3)&&!memcmp(message+8,before+8,8)&&!memcmp(message+24,before+24,8));
 }
 storage[23]^=1;struct am_midi_wire read;assert(!am_midi_wire_read(message,&read,NULL));
 struct am_midi_wire first={old,3,1,-12},nested={old,7,0,120},prior,inner;
 am_midi_wire_begin(&first,&prior);assert(am_midi_wire_current(&read)&&read.sound==3);
 am_midi_wire_begin(&nested,&inner);assert(am_midi_wire_current(&read)&&read.sound==7);
 am_midi_wire_end(&inner);assert(am_midi_wire_current(&read)&&read.sound==3);
 am_midi_wire_end(&prior);assert(!am_midi_wire_current(&read));
 puts("PASS MIDI wire envelope: full 64-bit IDs, all 128 sources and 368 offsets, original MIDI/time preserved, nested context, corruption rejected and no short-ID reuse");return 0;
}
