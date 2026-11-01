/* Native knobs must not depend on a busy or unserviced mod UI timer. */
#define main queue_regression_main
#include "check_knob_events.c"
#undef main
static pthread_t producer;
static unsigned delivered;
static int wrong_thread,wrong_order;
static void native_receive(void*event){
 unsigned index=delivered++,k=index/256,point=index%256;
 if(!pthread_equal(pthread_self(),producer))wrong_thread=1;
 if(FIELD(event,8,unsigned char)!=0x10+k||FIELD(event,9,unsigned char)!=(point<128?point:255-point))wrong_order=1;
}
static void*produce(void*unused){
 (void)unused;producer=pthread_self();unsigned char event[16]={0};
 for(unsigned k=0;k<3;++k){event[8]=(unsigned char)(0x10+k);
  for(unsigned i=0;i<256;++i){event[9]=(unsigned char)(i<128?i:255-i);controller(event);}
 }
 return NULL;
}
int main(void){
 assert(!queue_regression_main());
 void*m=mmap(NULL,0x7500000,7,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(m!=MAP_FAILED);base=(uintptr_t)m;
 stub(phone_hooks[H_CONTROLLER].trampoline,native_receive);__builtin___clear_cache(m,(char*)m+0x7500000);
 pthread_t worker;assert(!pthread_create(&worker,NULL,produce,NULL));assert(!pthread_join(worker,NULL));
 if(delivered!=768||phone_events_pending()||wrong_thread||wrong_order){
  fprintf(stderr,"REPRODUCED native K input rerouted through UI timer: delivered=%u expected=768 queued=%u wrong_thread=%d wrong_order=%d\n",delivered,phone_events_pending(),wrong_thread,wrong_order);return 1;
 }
#ifndef NATIVE_PATH_BASELINE
 /* Include the interval after a UI action is popped but before it finishes. */
 unsigned char event[16]={0};event[8]=0x11;event[9]=127;
 atomic_store(&ui_dispatching,1);controller(event);assert(phone_events_pending()==1&&delivered==768);
 struct phone_event queued;assert(phone_events_pop(&queued)&&!queued.pad&&queued.native[8]==0x11&&queued.native[9]==127);
 atomic_store(&ui_dispatching,0);
 /* Naming also remains an explicit UI boundary. */
 atomic_store(&naming_active,1);controller(event);assert(phone_events_pop(&queued)&&!queued.pad&&delivered==768);atomic_store(&naming_active,0);
#endif
 munmap(m,0x7500000);
 puts("PASS native K1/K2/K3: all 768 positions delivered in order on the original input thread while Sample observer is active and UI timer is unserviced; FIFO/in-flight actions and naming remain boundaries");return 0;
}
