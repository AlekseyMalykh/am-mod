/* Execute production controller -> FIFO -> UI/native forwarding. Native
 * parameter/audio receiver is a recorder; no physical ADC/hardware claim. */
#define PHONE_RGB_TEST 1
#include "src/phone_native.c"
#include <assert.h>
static unsigned calls,last[3],faults;
int music_ui_active(void){return 1;}
int note_native_active(void){return 0;}
int step_native_active(void){return 0;}
int workflow_active(void){return 0;}
int note_native_owns(unsigned c){(void)c;return 0;}
int music_ui_owns(unsigned c){(void)c;return 0;}
int step_native_owns(unsigned c){(void)c;return 0;}
int workflow_owns(unsigned k,unsigned c){(void)k;(void)c;return 0;}
uint64_t step_native_ticket(void){return 19;}
uint64_t workflow_ticket(void){return 23;}
void step_native_queue_fault(void){++faults;}
void workflow_fault(void){++faults;}
int music_ui_control(void*e){(void)e;return 0;}
int note_native_control(void*e){(void)e;return 0;}
int step_native_control(void*e,uint64_t route){(void)e;(void)route;return 0;}
static void receive(void*e){unsigned cc=FIELD(e,8,unsigned char);assert(cc>=0x10&&cc<=0x12);last[cc-0x10]=FIELD(e,9,unsigned char);++calls;}
static void stub(uintptr_t off,void*fn){uint32_t words[]={0x58000050,0xd61f0200};memcpy((void*)(base+off),words,8);memcpy((void*)(base+off+8),&fn,8);}
static void drain(void){struct phone_event e;while(phone_events_pop(&e)){assert(e.kind==PHONE_CONTROLLER&&!e.pad);controller_ui(e.native,e.step_ticket);}}
int main(void){
 void*m=mmap(NULL,0x7500000,7,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(m!=MAP_FAILED);base=(uintptr_t)m;
 stub(phone_hooks[H_CONTROLLER].trampoline,receive);__builtin___clear_cache(m,(char*)m+0x7500000);
 ui_known=1;ui_thread=pthread_self();
 for(unsigned cc=0x10;cc<=0x12;++cc)for(unsigned end=0;end<=127;end+=127){
  unsigned char event[16]={0};event[8]=(unsigned char)cc;
  for(unsigned n=0;n<PHONE_EVENT_CAPACITY+32;++n){event[9]=(unsigned char)(1+n%100);controller(event);}
  event[9]=(unsigned char)end;controller(event);drain();
  if(last[cc-0x10]!=end){fprintf(stderr,"REPRODUCED endpoint lost: K%u wanted=%u actual=%u faults=%u calls=%u\n",cc-0x10+1,end,last[cc-0x10],faults,calls);return 1;}
 }
 assert(!faults&&!atomic_load(&input_fault));
 puts("PASS production K1/K2/K3 bursts retain 0/127 endpoints without queue overflow or native bypass overtaking older UI events");
 munmap(m,0x7500000);return 0;
}
