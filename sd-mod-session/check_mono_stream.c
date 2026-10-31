#define CHOP_PCM_MAIN base_pcm_main
#include "check_chop_pcm.c"
int main(int argc,char**argv){
 assert(!base_pcm_main(argc,argv));phase="real mono channel extraction";
 for(unsigned channel=0;channel<2;++channel){
  unsigned char*source=calloc(1,0x28);uintptr_t none[2]={0};assert(source);
  FN(0x20c74f0,void(*)(void*,const uintptr_t*))(source,none);FIELD(source,0,void*)=reader_vtable;
  void*d=FIELD(source,8,void*);FN(0x20c56d0,void(*)(void*,int))(d,16);FN(0x20c56b0,void(*)(void*,int))(d,2);FN(0x20c56f0,void(*)(void*,int))(d,48000);
  unsigned char*mono=calloc(1,0x58);assert(mono);uintptr_t owned[2]={base+0x20c58d0,(uintptr_t)source};
  FN(0x20d5d20,void(*)(void*,uintptr_t*,unsigned))(mono,owned,channel);assert(!owned[1]);
  d=FIELD(mono,8,void*);assert(FIELD(d,0,uint16_t)==1&&FIELD(d,4,unsigned)==16&&FIELD(d,8,unsigned)==48000);
  for(unsigned n=0;n<16;++n){struct pad_native_block block={0};check_read_frame(mono,1,&block,base+0x20c5f60);assert(block.frames==1&&block.pcm[0]==pcm[n*2+channel]);pad_native_block_finish(&block);}
  assert(FN(0x20c58f0,int(*)(void*))(mono));
  ((void(*)(void*))FIELD((void*)FIELD(mono,0,uintptr_t),8,uintptr_t))(mono);
 }
 puts("PASS native MonoConvertingStream: Left/Right exact PCM, one channel, complete frame count/rate, finite EOF and owned stream destruction");return 0;
}
