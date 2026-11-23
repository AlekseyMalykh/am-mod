#include "synth_route.h"
#include <stdatomic.h>
#include <string.h>
#if ATOMIC_CHAR_LOCK_FREE!=2 || ATOMIC_LONG_LOCK_FREE!=2 || ATOMIC_LLONG_LOCK_FREE!=2 || ATOMIC_INT_LOCK_FREE!=2
#error Synth route publication requires lock-free atomics
#endif
static _Atomic unsigned ready,carrier;
static _Atomic unsigned char key[128];
static _Atomic uint64_t serial,generation,stop_serial=1;
void am_synth_route_stop(void){
 uint64_t s=atomic_load(&stop_serial);
 unsigned advanced=0;
 for(unsigned attempt=0;attempt<64&&s!=UINT64_MAX;++attempt)if(atomic_compare_exchange_weak(&stop_serial,&s,s+1)){advanced=1;break;}
 if(!advanced){atomic_store(&ready,0);atomic_store(&stop_serial,UINT64_MAX);}
 (void)am_synth_owned_reset();
}
uint64_t am_synth_route_stop_serial(void){return atomic_load_explicit(&stop_serial,memory_order_acquire);}
void am_synth_route_clear(void){if(atomic_exchange_explicit(&ready,0,memory_order_acq_rel))(void)am_synth_owned_reset();}
int am_synth_route_read(uint64_t expected,struct am_synth_route_snapshot*out){
 if(!expected||!out||!atomic_load_explicit(&ready,memory_order_acquire))return 0;
 for(unsigned attempt=0;attempt<4;++attempt){
  uint64_t before=atomic_load_explicit(&serial,memory_order_acquire);if(before&1)continue;
  struct am_synth_route_snapshot value={{0},0,0,0,0};value.generation=atomic_load_explicit(&generation,memory_order_relaxed);
  value.carrier=atomic_load_explicit(&carrier,memory_order_relaxed);
  for(unsigned i=0;i<128;++i)value.program_key[i]=(char)atomic_load_explicit(key+i,memory_order_relaxed);
  atomic_thread_fence(memory_order_seq_cst);
  if(before!=atomic_load_explicit(&serial,memory_order_acquire)||!atomic_load_explicit(&ready,memory_order_acquire))continue;
  value.serial=before;value.stop=am_synth_route_stop_serial();if(value.generation!=expected||value.stop==UINT64_MAX)return 0;
  *out=value;return 1;
 }
 return 0;
}
int am_synth_route_publish(const struct am_synth_binding*b,uint64_t g){
 if(!g||!am_synth_binding_valid(b))return 0;
 struct am_synth_route_snapshot old;
 if(am_synth_route_read(g,&old)&&old.carrier==b->carrier&&!strcmp(old.program_key,b->program_key))return 1;
 uint64_t s=atomic_load_explicit(&serial,memory_order_relaxed);if((s&1)||s>UINT64_MAX-2){am_synth_route_clear();return 0;}
 atomic_store_explicit(&ready,0,memory_order_release);atomic_store_explicit(&serial,s+1,memory_order_release);
 /* Pair with the reader's acquire fence through any copied atomic field:
  * observing a new field must also expose the preceding odd version. */
 atomic_thread_fence(memory_order_release);
 unsigned end=0;for(unsigned i=0;i<128;++i){unsigned char c=end?0:(unsigned char)b->program_key[i];end|=!c;atomic_store_explicit(key+i,c,memory_order_relaxed);}
 atomic_store_explicit(&carrier,b->carrier,memory_order_relaxed);atomic_store_explicit(&generation,g,memory_order_relaxed);
 atomic_store_explicit(&serial,s+2,memory_order_release);atomic_store_explicit(&ready,1,memory_order_release);return 1;
}
