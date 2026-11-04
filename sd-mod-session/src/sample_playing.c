#define _GNU_SOURCE
#include "sample_playing.h"
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>
#define F(p,o,T) (*(T*)((char*)(p)+(o)))
#define FN(o,T) ((T)(image+(o)))
static uintptr_t image;
static int (*original)(void*,unsigned);
static _Atomic(uintptr_t) published_program;
static _Atomic uint64_t playing[2];
static _Atomic unsigned serial;
static unsigned reports;
static void publish(void*program,unsigned pad,int active){
 atomic_fetch_add_explicit(&serial,1,memory_order_acq_rel);
 if(atomic_load(&published_program)!=(uintptr_t)program){
  atomic_store(&playing[0],0);atomic_store(&playing[1],0);atomic_store(&published_program,(uintptr_t)program);
 }
 if(pad<128){uint64_t bit=UINT64_C(1)<<(pad%64);
  if(active)atomic_fetch_or(&playing[pad/64],bit);else atomic_fetch_and(&playing[pad/64],~bit);}
 atomic_fetch_add_explicit(&serial,1,memory_order_release);
}
static int refresh(void*layer,unsigned pad){
 /* Exact model and dynamic cast used by1539610. No component or
  * mutable voice list is accessed by the UI/RGB callbacks. */
 void*things=layer?F(layer,0xc8,void*):NULL;
 void*program=things?F(things,0xb8,void*):NULL;
 void*ac50=NULL;
 if(program){void*from=F((void*)image,0x71657d8,void*),*to=F((void*)image,0x71658a8,void*);
  ac50=FN(0xb51d78,void*(*)(void*,void*,void*,intptr_t))(program,from,to,0);}
 int active=0;unsigned count=0;
 if(ac50&&pad<128){void*instrument=FN(0x286bbe0,void*(*)(void*,int))(ac50,(int)pad);
  if(instrument){
   /* Match the instrument independently of the note used by 1539610.
    * Stock 286c720 tests the voice's rendering layers with 221b220.
    * 221af00 instead tests the note's lifetime: note-off can end that
    * lifetime while a loop is still rendering. Read on the lighting owner. */
   uintptr_t begin=F(ac50,0x2838,uintptr_t),end=F(ac50,0x2840,uintptr_t);
   if(begin&&end>=begin&&(end-begin)%8==0&&end-begin<=1024*8){
    count=(unsigned)((end-begin)/8);
    for(unsigned i=0;i<count;++i){void*voice=F((void*)begin,i*8,void*);
     if(voice&&F(voice,0x8f8,void*)==instrument&&FN(0x221b220,int(*)(void*))(voice)){active=1;break;}}
   }
  }}
 int before=sample_playing_active(program,pad);
 publish(program,pad,active);
 if(reports<8||(before!=active&&reports<40)){
  ++reports;fprintf(stderr,"SAMPLE_PLAYING PAD pad=%u active=%d voices=%u model=%d\n",pad,active,count,ac50!=NULL);
 }
 return original(layer,pad);
}
int sample_playing_active(const void*program,unsigned pad){
 if(!program||pad>=128)return 0;
 for(unsigned attempt=0;attempt<3;++attempt){
  unsigned before=atomic_load_explicit(&serial,memory_order_acquire);if(before&1)continue;
  uintptr_t owner=atomic_load(&published_program);uint64_t bits=atomic_load(&playing[pad/64]);
  if(before==atomic_load_explicit(&serial,memory_order_acquire))return owner==(uintptr_t)program&&!!(bits&(UINT64_C(1)<<(pad%64)));
 }
 return 0;
}
int sample_playing_install(uintptr_t b){
 uintptr_t*slot=(uintptr_t*)(b+0x6e10080);if(*slot!=b+0x1539610)return 0;
 image=b;original=(int(*)(void*,unsigned))*slot;
 void*page=(void*)((uintptr_t)slot&~(uintptr_t)4095);
 if(mprotect(page,4096,PROT_READ|PROT_WRITE))return 0;
 *slot=(uintptr_t)refresh;if(mprotect(page,4096,PROT_READ))_exit(121);
 fprintf(stderr,"SAMPLE_PLAYING NATIVE_LIGHTING_OWNER_SNAPSHOT_INSTALLED\n");return 1;
}
