#include "usb_audio.h"
#include <stdatomic.h>
#include <stdint.h>
#include <math.h>
#if ATOMIC_INT_LOCK_FREE != 2
#error USB audio requires lock-free counters
#endif
static float pcm[AM_USB_CAPACITY][AM_USB_CHANNELS];
static unsigned tags[AM_USB_CAPACITY];
static _Atomic unsigned head,tail,generation,enabled;
static unsigned producer_generation,consumer_generation;
void usb_audio_reset(void){atomic_fetch_add_explicit(&generation,1,memory_order_release);}
void usb_audio_enable(unsigned on){atomic_store_explicit(&enabled,on!=0,memory_order_release);usb_audio_reset();}
unsigned usb_audio_enabled(void){return atomic_load_explicit(&enabled,memory_order_acquire);}
void usb_audio_capture(const float *const c[AM_USB_CHANNELS],unsigned n){
 if(!atomic_load_explicit(&enabled,memory_order_acquire))return;
 unsigned g=atomic_load_explicit(&generation,memory_order_acquire);
 unsigned h=atomic_load_explicit(&head,memory_order_relaxed);
 /* A reset is acknowledged by the consumer, which alone owns tail. Until
  * then publish no new project data into the old queue. */
 if(g!=producer_generation){producer_generation=g;return;}
 unsigned t=atomic_load_explicit(&tail,memory_order_acquire);
 if(!c||!n||n>AM_USB_CAPACITY||h-t>AM_USB_CAPACITY-n)return;
 for(unsigned f=0;f<n;++f){tags[(h+f)%AM_USB_CAPACITY]=g;
  for(unsigned ch=0;ch<AM_USB_CHANNELS;++ch){float v=c[ch]?c[ch][f]:0;pcm[(h+f)%AM_USB_CAPACITY][ch]=isfinite(v)?v:0;}}
 atomic_store_explicit(&head,h+n,memory_order_release);
}
unsigned usb_audio_peek(float *out,unsigned n){
 unsigned g=atomic_load_explicit(&generation,memory_order_acquire);
 unsigned h=atomic_load_explicit(&head,memory_order_acquire);
 unsigned t=atomic_load_explicit(&tail,memory_order_relaxed);
 if(g!=consumer_generation){consumer_generation=g;atomic_store_explicit(&tail,h,memory_order_release);return 0;}
 if(!out||n>AM_USB_CAPACITY)return 0;
 unsigned available=h-t;if(available>AM_USB_CAPACITY)return 0;
 while(available&&tags[t%AM_USB_CAPACITY]!=g){++t;--available;atomic_store_explicit(&tail,t,memory_order_release);}
 if(n>available)n=available;
 for(unsigned f=0;f<n;++f){if(tags[(t+f)%AM_USB_CAPACITY]!=g)return f;
  for(unsigned ch=0;ch<AM_USB_CHANNELS;++ch)out[f*AM_USB_CHANNELS+ch]=pcm[(t+f)%AM_USB_CAPACITY][ch];}
 if(g!=atomic_load_explicit(&generation,memory_order_acquire))return 0;
 return n;
}
void usb_audio_consume(unsigned n){
 unsigned t=atomic_load_explicit(&tail,memory_order_relaxed),h=atomic_load_explicit(&head,memory_order_acquire);
 if(n<=h-t)atomic_store_explicit(&tail,t+n,memory_order_release);
}
