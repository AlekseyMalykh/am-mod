#include "compressor.h"
#include <math.h>
#include <stdatomic.h>
#include <string.h>
#define F(p,o,T) (*(T*)((char*)(p)+(o)))
#if ATOMIC_LLONG_LOCK_FREE != 2 || ATOMIC_INT_LOCK_FREE != 2
#error Compressor requires lock-free publication
#endif
/* The primary Colour AudioPluginInstance table has 123 entries. Secondary
 * tables and all other processors remain native. Bound only through the
 * actual AC50CompressorMeterConnector owner, not the first effect observed. */
static uintptr_t table[125],image;
static _Atomic(uintptr_t) owner;
static _Atomic uint64_t settings=UINT64_C(120)<<2|UINT64_C(20)<<10|UINT64_C(48)<<17;
static _Atomic unsigned reduction;
static struct comp_opto opto;
static float mix;
static float wet[2][8192];
static void (*original_process)(void*,void*,void*);
static void (*original_destroy)(void*),(*original_delete)(void*);
static struct comp_config decode(uint64_t v){
 struct comp_config c={-48+(float)((v>>2)&255)*.25f,1+(float)((v>>10)&127)*.1f,-12+(float)((v>>17)&255)*.25f,(unsigned)((v>>1)&1)};return c;
}
unsigned compressor_type(void){return (unsigned)(atomic_load_explicit(&settings,memory_order_acquire)&1);}
int compressor_ready(void){return atomic_load_explicit(&owner,memory_order_acquire)!=0;}
int compressor_select(unsigned type){
 if(type>=COMP_TYPES||(type==COMP_OPTO&&!compressor_ready()))return 0;
 uint64_t v=atomic_load(&settings);atomic_store_explicit(&settings,(v&~UINT64_C(1))|type,memory_order_release);return 1;
}
void compressor_config(struct comp_config*c){if(c)*c=decode(atomic_load(&settings));}
void compressor_knob(unsigned k,unsigned value){
 if(k>2||value>127)return;
 uint64_t v=atomic_load(&settings);unsigned shift=k==0?2:k==1?10:17,bits=k==1?127:255;
 unsigned x=k==0?(value*192+63)/127:k==1?(value*70+63)/127:(value*120+63)/127;
 v=(v&~((uint64_t)bits<<shift))|((uint64_t)x<<shift);atomic_store_explicit(&settings,v,memory_order_release);
}
void compressor_bypass(void){atomic_fetch_xor_explicit(&settings,2,memory_order_release);}
float compressor_reduction(void){unsigned v=atomic_load_explicit(&reduction,memory_order_relaxed);float f;memcpy(&f,&v,4);return f;}
static void failed(void){atomic_fetch_and_explicit(&settings,~UINT64_C(1),memory_order_release);atomic_store(&reduction,0);}
static void process(void*self,void*buffer,void*midi){
 uint64_t v=atomic_load_explicit(&settings,memory_order_acquire);unsigned type=(unsigned)(v&1);
 if(!type&&mix==0){original_process(self,buffer,midi);return;}
 if((uintptr_t)self!=atomic_load_explicit(&owner,memory_order_acquire)||!buffer){failed();original_process(self,buffer,midi);return;}
 int channels=F(buffer,0,int),frames=F(buffer,4,int);float**data=F(buffer,16,float**);
 if(channels<1||channels>2||frames<0||frames>8192||!data||!data[0]||(channels==2&&!data[1])){failed();original_process(self,buffer,midi);return;}
 if(!frames)return;
 struct comp_config c=decode(v);float*dest[]={wet[0],wet[1]};
 if(!comp_opto_process(&opto,&c,data,dest,(unsigned)channels,(size_t)frames)){failed();original_process(self,buffer,midi);return;}
 /* No serial compression: both paths see the same dry buffer during a
  * ten-millisecond switch fade. Steady Opto skips the native processor. */
 if(!type||mix<1)original_process(self,buffer,midi);
 const float step=1.f/(float)(opto.rate*.010);
 for(int i=0;i<frames;++i){mix=type?fminf(1,mix+step):fmaxf(0,mix-step);
  for(int ch=0;ch<channels;++ch)data[ch][i]=mix>=1?wet[ch][i]:data[ch][i]+mix*(wet[ch][i]-data[ch][i]);}
 unsigned bits;float gr=type?opto.gr:0;memcpy(&bits,&gr,4);atomic_store_explicit(&reduction,bits,memory_order_relaxed);
}
static void destroyed(void*self){atomic_store(&owner,0);failed();original_destroy(self);}
static void deleted(void*self){atomic_store(&owner,0);failed();original_delete(self);}
int compressor_native_bind(uintptr_t base,void*processor){
 if(!base||!processor)return 0;
 uintptr_t current=__atomic_load_n((uintptr_t*)processor,__ATOMIC_ACQUIRE);
 if(compressor_ready())return image==base&&(uintptr_t)processor==atomic_load(&owner)&&current==(uintptr_t)(table+2);
 if(image)return 0; /* Never recycle a table potentially retained by audio. */
 if(current!=base+0x6e9d1d0)return 0;
 uintptr_t*n=(uintptr_t*)current;
 if(n[-2]||n[-1]!=base+0x6e9d1a8||n[7]!=base+0x2440820||n[123]!=(uintptr_t)(intptr_t)-0x120||n[124]!=base+0x6e9d1a8)return 0;
 for(unsigned i=0;i<123;++i)if(n[i]<base+0xb54000||n[i]>=base+0x4400000)return 0;
 float rate=F((void*)base,0x7173b80,float);if(!isfinite(rate)||rate<8000||rate>192000)return 0;
 memcpy(table,n-2,sizeof(table));original_process=(void(*)(void*,void*,void*))n[7];
 original_destroy=(void(*)(void*))n[0];original_delete=(void(*)(void*))n[1];
 table[2]=(uintptr_t)destroyed;table[3]=(uintptr_t)deleted;table[9]=(uintptr_t)process;
 comp_opto_init(&opto,rate);mix=0;image=base;
 atomic_store_explicit(&owner,(uintptr_t)processor,memory_order_release);
 __atomic_store_n((uintptr_t*)processor,(uintptr_t)(table+2),__ATOMIC_RELEASE);return 1;
}
