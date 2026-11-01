/* Compile production routing with the permanent compressor feature enabled. */
#define MPC_COMPRESSOR 1
#define main native_knob_main
#include "check_knob_events.c"
#undef main
static int compressor_open;
static uint64_t compressor_epoch=47;
static unsigned opto_calls,opto_value[3];
int compressor_ui_owns(unsigned kind,unsigned cc){return compressor_open&&kind==PHONE_CONTROLLER&&cc>=0x10&&cc<=0x12;}
uint64_t compressor_ui_ticket(void){return compressor_epoch;}
int compressor_ui_control(void*e,uint64_t ticket){
 if(ticket!=compressor_epoch||!compressor_open)return 1;
 opto_value[FIELD(e,8,unsigned char)-0x10]=FIELD(e,9,unsigned char);++opto_calls;return 1;
}
int main(void){
 assert(!native_knob_main()); /* Closed compressor must leave all native knobs alone. */
 void*m=mmap(NULL,0x7500000,7,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(m!=MAP_FAILED);base=(uintptr_t)m;
 stub(phone_hooks[H_CONTROLLER].trampoline,receive);__builtin___clear_cache(m,(char*)m+0x7500000);
 compressor_open=1;unsigned before=calls;unsigned char key[16]={0};
 for(unsigned k=0;k<3;++k){key[8]=(unsigned char)(0x10+k);for(unsigned v=1;v<127;++v){key[9]=(unsigned char)v;controller(key);}}
 struct phone_event e;while(phone_events_pop(&e)){assert(e.pad==64&&e.workflow_ticket==47);compressor_ui_control(e.native,e.workflow_ticket);}
 assert(opto_calls&&opto_value[0]==126&&opto_value[1]==126&&opto_value[2]==126&&calls==before);
 key[8]=0x10;key[9]=127;controller(key);assert(phone_events_pop(&e)&&e.pad==64);
 compressor_open=0;++compressor_epoch;unsigned received=opto_calls;
 compressor_ui_control(e.native,e.workflow_ticket);assert(opto_calls==received&&calls==before);
 key[8]=0x12;key[9]=127;controller(key);drain();assert(calls>before&&last[2]==127);
 munmap(m,0x7500000);
 puts("PASS production K routing with OptoCat enabled: closed compressor leaves native paths intact; Opto owns its knobs and ticket; stale Opto events never reach native parameters; native endpoints resume after closing");
 return 0;
}
