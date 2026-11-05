#define main native_takeover_suite
#include "check_scaled_native.c"
#undef main
#include "src/takeover_native.h"
static void receive_knob(void*e){movement(((unsigned char*)e)[9]);}
int main(int argc,char**argv){
 assert(!native_takeover_suite(argc,argv));
 setup(1);movement(10);float before=parameter;movement(100);
 assert(parameter!=before); /* Old native history interprets custom EQ travel as a big movement. */
 assert(takeover_native_check(base));
 for(unsigned knob=0;knob<3;++knob){
  setup(1);movement(10);before=parameter;takeover_native_context_changed();
  unsigned char event[16]={[8]=0x10+knob,[9]=100};
  takeover_native_forward(event,receive_knob);assert(parameter==before);
  event[9]=101;takeover_native_forward(event,receive_knob);assert(parameter>=before&&parameter-before<.05f);
 }
 for(int mode=0;mode<=2;mode+=2){
  setup(mode);movement(10);movement(127);float expected=parameter;unsigned expected_writes=writes;
  setup(mode);takeover_native_context_changed();
  unsigned char event[16]={[8]=0x10,[9]=10};takeover_native_forward(event,receive_knob);
  event[9]=127;takeover_native_forward(event,receive_knob);
  assert(parameter==expected&&writes==expected_writes); /* Pickup crossing and Jump retain native behaviour. */
 }
 puts("PASS actual native Scaled EQ handoff: no first-movement jump, small following delta, three knobs; Pickup and Jump preserved");return 0;
}
