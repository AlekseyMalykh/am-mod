/* Native TrimProcessingStream over a deterministic interleaved PCM source.
 * Native metadata copy, seek/range/EOF and stream lifetime are exercised;
 * this does not emulate SamplePool/file persistence or the device audio API. */
#define main original_steps_main
#include "check_steps.c"
#undef main
#include "src/pad_native_block.h"
static float pcm[32];static uintptr_t reader_vtable[8];static unsigned released,reads;
static void reader_delete(void*p){void*r=FIELD(p,0x10,void*);if(r)FN(0x123bca0,void(*)(void*))(r);free(p);++released;}
static int64_t reader_position(void*p){return FIELD(p,0x20,int64_t);}
static int reader_seek(void*p,int64_t frame){if(frame<0||frame>=16)return 0;FIELD(p,0x20,int64_t)=frame;return 1;}
void pcm_frame(void*p,int64_t requested,struct pad_native_block*out){int64_t n=FIELD(p,0x20,int64_t);assert(requested>0&&n>=0&&n<16);memset(out,0,sizeof(*out));out->frames=1;out->pcm=pcm+n*2;out->manage=base+0x20c54d0;FIELD(p,0x20,int64_t)=n+1;++reads;}
extern void check_pcm_frame(void),check_read_frame(void*,int64_t,struct pad_native_block*,uintptr_t);
#ifndef CHOP_PCM_MAIN
#define CHOP_PCM_MAIN main
#endif
int CHOP_PCM_MAIN(int argc,char**argv){assert(argc==2);load_elf(argv[1],0x1800000000ULL);
 FILE*native=fopen(argv[1],"rb");assert(native);assert(!fseek(native,0x123bca0,SEEK_SET));
 assert(fread((void*)(base+0x123bca0),1,16,native)==16);fclose(native);
 __builtin___clear_cache((char*)(base+0x123bca0),(char*)(base+0x123bcb0));
 struct sigaction a={0};a.sa_sigaction=fault;a.sa_flags=SA_SIGINFO;sigaction(SIGSEGV,&a,NULL);sigaction(SIGBUS,&a,NULL);
 phase="actual TrimProcessingStream PCM range";
 for(unsigned i=0;i<16;++i){pcm[i*2]=(float)i+.25f;pcm[i*2+1]=-(float)i-.5f;}
 for(unsigned i=0;i<8;++i)reader_vtable[i]=FIELD((void*)base,0x6e7d788+i*8,uintptr_t);
 reader_vtable[1]=(uintptr_t)reader_delete;reader_vtable[3]=(uintptr_t)reader_position;
 reader_vtable[5]=(uintptr_t)check_pcm_frame;reader_vtable[6]=(uintptr_t)reader_seek;
 for(unsigned trial=0;trial<3;++trial){unsigned start=trial==0?4:trial==1?0:15,end=trial==0?12:trial==1?16:16;
  unsigned char*source=calloc(1,0x28);assert(source);uintptr_t none[2]={0};
  FN(0x20c74f0,void(*)(void*,const uintptr_t*))(source,none);
  FIELD(source,0,void*)=reader_vtable;void*data=FIELD(source,8,void*);
  FN(0x20c56d0,void(*)(void*,int))(data,16);FN(0x20c56b0,void(*)(void*,int))(data,2);FN(0x20c56f0,void(*)(void*,int))(data,48000);
  unsigned char*trim=calloc(1,0x38);assert(trim);uintptr_t owned[2]={base+0x20c58d0,(uintptr_t)source};
  FN(0x20d9fd0,void(*)(void*,void*,int64_t,int64_t))(trim,owned,start,end);
  assert(!owned[1]&&FIELD(trim,0,uintptr_t)==base+0x6e7de10);
  void*cropped=FIELD(trim,8,void*);assert(cropped!=data&&FIELD(cropped,4,unsigned)==end-start&&FIELD(cropped,0,uint16_t)==2&&FIELD(cropped,8,unsigned)==48000);
  assert(FIELD(data,4,unsigned)==16&&FIELD(data,0,uint16_t)==2&&FIELD(data,8,unsigned)==48000);
  for(unsigned i=0;i<end-start;++i){
   assert(!FN(0x20c58f0,int(*)(void*))(trim));struct pad_native_block frame={0};
   check_read_frame(trim,1,&frame,base+0x20c5f60);assert(frame.frames==1);
   const float*v=frame.pcm;assert(v[0]==pcm[(start+i)*2]&&v[1]==pcm[(start+i)*2+1]);pad_native_block_finish(&frame);
  }
  assert(FN(0x20c58f0,int(*)(void*))(trim));
  assert(!FN(0x20d9b20,int(*)(void*,int64_t))(trim,-1)&&!FN(0x20d9b20,int(*)(void*,int64_t))(trim,end-start));
  assert(FN(0x20d9b20,int(*)(void*,int64_t))(trim,0));struct pad_native_block frame={0};check_read_frame(trim,1,&frame,base+0x20c5f60);
  assert(frame.pcm[0]==pcm[start*2]);pad_native_block_finish(&frame);
  FN(0x20da200,void(*)(void*))(trim);assert(released==trial+1);
 }
 assert(reads==8+16+1+3);
 puts("PASS actual native TrimProcessingStream ctor, owned metadata, stereo PCM range/first/last/full/one-frame, EOF, seek bounds and destructors; deterministic source frames are fixtures, SamplePool and saved-file persistence remain untested");
 return 0;
}
