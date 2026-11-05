#define main previous_knob_suite
#include "check_knob_events.c"
#undef main
int main(void){
 assert(!previous_knob_suite());
 void*m=mmap(NULL,0x7500000,7,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(m!=MAP_FAILED);base=(uintptr_t)m;
 stub(phone_hooks[H_CONTROLLER].trampoline,receive);__builtin___clear_cache(m,(char*)m+0x7500000);
 test_music_owns=1;atomic_store(&native_button_lights[0],3);atomic_store(&native_shift_requested,1);
 unsigned before=calls;unsigned char e[16]={[8]=0x10,[9]=127};controller(e);
 struct phone_event queued;assert(phone_events_pop(&queued)&&queued.pad==2&&queued.down==1&&calls==before);
 test_knob_fx_open=1;controller(e);assert(calls==before+1&&!phone_events_pending());
 test_knob_fx_open=0;controller(e);assert(phone_events_pop(&queued)&&queued.pad==2&&calls==before+1);
 puts("PASS Shift LED cue 3 does not bypass Seq Colour; actual open Knob FX owns K1 and closing restores custom routing");return 0;
}
