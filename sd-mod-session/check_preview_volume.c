/* Actual native SampleFileAudition volume listener, queue and worker.
 * Instrument lookup and the final instrument gain receiver are fixtures. */
#define main original_steps_main
#include "check_steps.c"
#undef main
#include "src/preview_gain.h"
static unsigned char audition[0x3f0],queues[0x260],slots_gain[2][0xc0],preview_program[8],preview_instrument[0xa00];
static unsigned gain_calls;static int worker;static float received_gain;
static void*preview_instrument_lookup(void*p,int pad){assert(p==preview_program&&pad==0x14);return preview_instrument;}
static void gain(void*p,float value){assert(worker&&p==preview_instrument&&value>=0.f&&value<=1.f);received_gain=value;++gain_calls;}
static void no_mutex(void*p){assert(p==queues+0x220);}
static int inactive_instrument(void*p){assert(p==preview_instrument+0x480);return 0;}
static void float_property(void*p){FN(0x236f0d0,void(*)(void*))(p);}
int main(int argc,char**argv){assert(argc==2);load_elf(argv[1],0x1800000000ULL);
 struct sigaction a={0};a.sa_sigaction=fault;a.sa_flags=SA_SIGINFO;sigaction(SIGSEGV,&a,NULL);sigaction(SIGBUS,&a,NULL);
 phase="actual preview-only volume queue";
 unsigned char original_gain[16];memcpy(original_gain,(void*)(base+0x276c590),16);
 stub(0x286bbe0,preview_instrument_lookup);stub(0x276c590,gain);stub(0xf40e50,no_mutex);stub(0xf40e90,no_mutex);
 __builtin___clear_cache((char*)base,(char*)(base+0x7500000));
 void*q=queues+0x1d8;FIELD(q,0,uint64_t)=2;FIELD(q,0x30,void*)=slots_gain;
 for(unsigned i=0;i<2;++i){FIELD(slots_gain[i],0,uintptr_t)=base+0xb57a7c;FIELD(slots_gain[i],8,uintptr_t)=base+0x15265b0;}
 FIELD(audition,0x10,void*)=queues;FIELD(audition,0x358,void*)=preview_program;
 FN(0x20bc210,void(*)(void*,float))(audition,.5f);assert(!gain_calls&&!FIELD(audition,0x18,unsigned));
 FIELD(audition,0x3e0,void*)=(void*)1;uint64_t reader=0;
 const float values[]={0.f,.5f,1.f};
 for(unsigned i=0;i<3;++i){FN(0x20bc210,void(*)(void*,float))(audition,values[i]);assert(gain_calls==i&&FIELD(audition,0x18,unsigned)==1);
  worker=1;FN(0x2b59200,void(*)(void*,uint64_t*))(q,&reader);worker=0;
  assert(gain_calls==i+1&&received_gain==values[i]&&!FIELD(audition,0x18,unsigned));}
 /* Restore the actual native instrument gain setter. Now exercise the
  * physical knob binding, native curve conversion, production K3/Main
  * product, native preview listener/worker and actual final amplitude. */
 memcpy((void*)(base+0x276c590),original_gain,16);stub(0x2765f80,inactive_instrument);
 __builtin___clear_cache((char*)base,(char*)(base+0x7500000));
 unsigned char parameter[0x80]={0},control[0x60]={0},binding[0x10]={0};
 float_property(parameter+8);float_property(control);float_property(preview_instrument+0x8b0);FIELD(parameter,0x60,unsigned)=1;
 FN(0x1ba88b0,void(*)(void*,void*,void*))(binding,parameter,control);
 const float physical[]={0.f,.25f,.5f,1.f,0.f};const float expected[]={0.f,.15f,.315f,1.f,0.f};
 for(unsigned i=0;i<5;++i){
  FN(0x13bb930,void(*)(void*,float))(control,physical[i]);
  float main=FIELD(parameter,4,float);fprintf(stderr,"MASTER TEST physical=%.4f native_gain=%.6f expected=%.6f\n",(double)physical[i],(double)main,(double)expected[i]);assert(fabsf(main-expected[i])<1e-5f);
  FIELD((void*)base,0x7236468,uint32_t)=0x3fff64c2; /* pinned native static ctor bbebe8 */
  for(unsigned curve=0;curve<=3;++curve)for(unsigned level=25;level<=100;level+=75){
   FIELD(preview_instrument,0x908,unsigned)=curve;
   float normalized=preview_normalized_gain(main,level,curve);FIELD(preview_instrument,0x900,float)=-1.f;
   FN(0x20bc210,void(*)(void*,float))(audition,normalized);worker=1;FN(0x2b59200,void(*)(void*,uint64_t*))(q,&reader);worker=0;
   float before_master=FIELD(preview_instrument,0x8ac,float);
   assert(fabsf(before_master-(main>0.f?(float)level/100.f:0.f))<1e-5f);
   assert(fabsf(before_master*main-expected[i]*(float)level/100.f)<1e-5f); /* output mix remains a fixture boundary */
  }
 }
 assert(preview_normalized_gain(NAN,100,0)==0.f&&preview_normalized_gain(-1.f,100,1)==0.f&&preview_normalized_gain(1.f,0,2)==0.f&&preview_normalized_gain(1.f,100,99)==0.f);
 puts("PASS actual native Main Volume physical property/1ba8550 gain curve; production K3 attenuation ->20bc210/worker ->276c590 actual pre-master instrument amplitude; zero Main stays silent at K3=100; nonzero Main is not applied twice; final output mixer/hardware remain untested");
 puts("PASS actual SampleFileAudition20bc210 -> native queue/worker20bc160 -> preview instrument20 gain; mute/half/full, no UI-thread gain call, reference lifetime; instrument lookup/audio receiver remain fixtures");
}
