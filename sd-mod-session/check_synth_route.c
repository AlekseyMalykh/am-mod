#include "src/synth_route.h"
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
static struct am_synth_binding first,second;
static _Atomic unsigned done,reads;
static void*observe(void*unused){
 (void)unused;
 do{for(unsigned i=0;i<2;++i){struct am_synth_route_snapshot s;
  if(am_synth_route_read(i?11:10,&s)){
   const struct am_synth_binding*b=i?&second:&first;
   assert(s.generation==(i?11:10)&&s.carrier==b->carrier&&!strcmp(s.program_key,b->program_key));
   assert(!(s.serial&1)&&s.serial&&s.stop);atomic_fetch_add(&reads,1);
  }
 }}while(!atomic_load(&done));return NULL;
}
void am_synth_route_probe(void){
 assert(am_synth_binding_capture("First Program long identity 12345678901234567890",15,&first));
 assert(am_synth_binding_capture("Second",127,&second));
 assert(am_synth_route_publish(&first,10));struct am_synth_route_snapshot s,before;
 assert(am_synth_route_read(10,&before)&&!am_synth_route_read(11,&s));
 first.parameters[1]^=1;assert(am_synth_route_publish(&first,10)&&am_synth_route_read(10,&s)&&s.serial==before.serial);
 am_synth_route_clear();assert(!am_synth_route_read(10,&s)&&am_synth_route_stop_serial()==before.stop);
 assert(am_synth_route_publish(&second,11)&&am_synth_route_read(11,&s)&&s.serial>before.serial);
 uint64_t stop=s.stop;am_synth_route_stop();assert(am_synth_route_read(11,&s)&&s.stop==stop+1);
 pthread_t reader;assert(!pthread_create(&reader,NULL,observe,NULL));
 for(unsigned i=0;i<20000;++i)assert(am_synth_route_publish(i&1?&second:&first,i&1?11:10));
 atomic_store(&done,1);assert(!pthread_join(reader,NULL));assert(atomic_load(&reads));am_synth_route_clear();
 puts("PASS Synth route: bounded atomic snapshots, concurrent key/carrier/generation coherence, parameter identity, binding loss versus transport Stop");
}
