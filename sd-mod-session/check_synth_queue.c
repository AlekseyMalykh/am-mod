/* Trial MPSC transport, real queue implementation; no audio-thread locks. */
#define MPC_PAD_MIDI_RECORDING_TRIAL 1
#include "src/synth.c"
#include <assert.h>
#include <limits.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#define PRODUCERS 4u
#define COUNT 20000u
static int read_value(unsigned*out){struct synth_command command;if(!pop_command(&command))return 0;*out=command.value;return 1;}
static void*producer(void*arg){
 unsigned id=(unsigned)(uintptr_t)arg;
 for(unsigned n=1;n<=COUNT;++n){struct synth_command command={(id<<24)|n,id+1,((uint64_t)id<<32)|n};
  while(!try_push_command(command,0))sched_yield();}
 return NULL;
}
static void reset_queue(unsigned start){
 atomic_store(&event_head,start);atomic_store(&event_tail,start);
 for(unsigned i=0;i<QUEUE;++i){unsigned ticket=start+i;atomic_store(&event_queue[ticket%QUEUE].sequence,ticket);}
}
int main(void){
 unsigned value=0;assert(!read_value(&value));
 /* Reserved, unpublished slot: audio must return immediately. */
 atomic_store(&event_head,1);assert(!read_value(&value));
 event_queue[0].command=(struct synth_command){123,0,0};atomic_store(&event_queue[0].sequence,1);
 assert(read_value(&value)&&value==123);assert(!read_value(&value));
 reset_queue(UINT_MAX-31u);
 for(unsigned n=0;n<256;++n){assert(try_push(n,0));assert(read_value(&value)&&value==n);}
 reset_queue(0);for(unsigned n=0;n<QUEUE-16;++n)assert(try_push(n,16));
 assert(!try_push(999,16));for(unsigned n=0;n<16;++n)assert(try_push(n,0));assert(!try_push(999,0));
 for(unsigned n=0;n<QUEUE;++n)assert(read_value(&value));
 assert(!read_value(&value));
 /* Legacy/rollback Off must remain finite even with every slot full. */
 for(unsigned n=0;n<QUEUE;++n)assert(try_push(2u<<16|71u,0));
 voices[0].note=60;voices[0].env=ENV_SUSTAIN;am_synth_note(60,0,0);
 assert(atomic_load(&forced_off[1])&(1u<<28));drain();
 assert(voices[0].env==ENV_RELEASE&&!atomic_load(&forced_off[1]));
 reset_queue(0);atomic_store(&event_head,1); /* producer0 paused after claim */
 assert(try_push(1u<<16|100u<<8|60u,0));
 for(unsigned n=2;n<QUEUE;++n)assert(try_push(2u<<16|71u,0));
 am_synth_note(60,0,0);drain();assert(atomic_load(&forced_off[1])&(1u<<28));
 event_queue[0].command=(struct synth_command){2u<<16|71u,0,0};atomic_store(&event_queue[0].sequence,1);drain();
 assert(!atomic_load(&forced_off[1]));
 for(unsigned i=0;i<VOICES;++i)assert(voices[i].note!=60||voices[i].env==ENV_OFF||voices[i].env==ENV_RELEASE);
 pthread_t threads[PRODUCERS];unsigned last[PRODUCERS]={0};
 for(unsigned p=0;p<PRODUCERS;++p)assert(!pthread_create(threads+p,NULL,producer,(void*)(uintptr_t)p));
 for(unsigned n=0;n<PRODUCERS*COUNT;){
  struct synth_command command;if(!pop_command(&command)){sched_yield();continue;}value=command.value;
  unsigned id=value>>24,serial=value&0xffffffu;assert(id<PRODUCERS);assert(serial==++last[id]);++n;
  assert(command.epoch==id+1&&command.owner==(((uint64_t)id<<32)|serial));
 }
 for(unsigned p=0;p<PRODUCERS;++p){assert(!pthread_join(threads[p],NULL));assert(last[p]==COUNT);}
 assert(!read_value(&value));puts("PASS Synth MPSC: 80000 commands, ordered/exactly once; unpublished/full/reserve/wrap");
}
