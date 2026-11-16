#include "src/usb_audio.h"
#include <assert.h>
#include <math.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
static float input[18][64],output[8192*18];
static const float*taps[18];
static _Atomic int finished;
static void*producer(void*unused){(void)unused;
 for(unsigned block=0;block<20000;++block){for(unsigned c=0;c<18;++c)for(unsigned f=0;f<64;++f)input[c][f]=(float)(block*64+f+c);usb_audio_capture(taps,64);}
 atomic_store(&finished,1);return NULL;
}
int main(void){
 for(unsigned c=0;c<18;++c){taps[c]=input[c];for(unsigned f=0;f<64;++f)input[c][f]=(float)(100*c+f);}
 usb_audio_enable(1);usb_audio_capture(taps,64);assert(!usb_audio_peek(output,64));
 usb_audio_capture(taps,64);assert(usb_audio_peek(output,64)==64);
 for(unsigned f=0;f<64;++f)for(unsigned c=0;c<18;++c)assert(output[f*18+c]==input[c][f]);
 usb_audio_consume(17);assert(usb_audio_peek(output,64)==47);assert(output[0]==17);usb_audio_consume(47);assert(!usb_audio_peek(output,64));
 for(unsigned i=0;i<128;++i)usb_audio_capture(taps,64);usb_audio_capture(taps,64);assert(usb_audio_peek(output,8192)==8192);usb_audio_consume(8192);
 input[1][1]=NAN;usb_audio_capture(taps,64);assert(usb_audio_peek(output,64)==64);assert(output[19]==0);usb_audio_consume(64);
 usb_audio_capture(taps,64);usb_audio_reset();assert(!usb_audio_peek(output,64));usb_audio_capture(taps,64);usb_audio_capture(taps,64);assert(usb_audio_peek(output,64)==64);usb_audio_consume(64);
 usb_audio_enable(0);usb_audio_capture(taps,64);assert(!usb_audio_peek(output,64));
 usb_audio_enable(1);usb_audio_capture(taps,64);usb_audio_peek(output,64);
 pthread_t thread;assert(!pthread_create(&thread,NULL,producer,NULL));float previous=-1;
 for(;;){unsigned n=usb_audio_peek(output,64);if(!n){if(atomic_load(&finished))break;continue;}
  for(unsigned f=0;f<n;++f){float v=output[f*18];assert(v>previous);previous=v;for(unsigned c=1;c<18;++c)assert(output[f*18+c]==v+c);}
  usb_audio_consume(n);
 }
 pthread_join(thread,NULL);assert(previous>=0);
 puts("PASS USB 18-channel layout, partial consumption, full queue, NaN, reset and concurrent producer/consumer");
}
