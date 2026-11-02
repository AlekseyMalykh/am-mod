#include "src/compressor.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#define N 48000
static float left[N],right[N],out_l[N],out_r[N],reference[N];
static void tone(double hz,float peak){for(unsigned i=0;i<N;++i){left[i]=(float)(peak*sin(2*3.141592653589793*hz*i/48000));right[i]=-left[i];}}
static void run(struct comp_opto*s,const struct comp_config*c,unsigned frames,unsigned block){
 for(unsigned at=0;at<frames;){unsigned n=frames-at;if(n>block)n=block;
  float*in[]={left+at,right+at},*out[]={out_l+at,out_r+at};assert(comp_opto_process(s,c,in,out,2,n));at+=n;}
}
static double amplitude(double hz,const float*x){double re=0,im=0;
 for(unsigned i=N/2;i<N;++i){double a=2*3.141592653589793*hz*i/48000;re+=x[i]*cos(a);im+=x[i]*sin(a);}
 return 2*hypot(re,im)/(N/2);
}
static float naive(float x){float u=1.5f*x;float y=fabsf(u)<=1?u-u*u*u/3:copysignf(2.f/3,u);return x+.35f*(y/1.5f-x);}
int main(void){
 struct comp_config c=COMP_CONFIG_DEFAULT;c.threshold=0;c.ratio=1;
 struct comp_opto s,t;comp_opto_init(&s,48000);
 tone(1000,.5f);run(&s,&c,N,256);assert(!memcmp(left,out_l,sizeof(left)));
 c.warm=1;comp_opto_init(&s,48000);run(&s,&c,N,256);
 double fundamental=amplitude(1000,out_l),third=amplitude(3000,out_l),mean=0;
 assert(fundamental>.45&&fundamental<.5&&third>.005&&third<.02);
 for(unsigned i=N/2;i<N;++i){assert(out_l[i]==-out_r[i]);mean+=out_l[i];}
 assert(fabs(mean/(N/2))<1e-6);
 memcpy(reference,out_l,sizeof(reference));comp_opto_init(&t,48000);run(&t,&c,N,73);
 for(unsigned i=0;i<N;++i)assert(fabsf(out_l[i]-reference[i])<1e-7);
 printf("WARM_SPECTRUM fundamental=%.8f third=%.8f dc=%.9g\n",fundamental,third,mean/(N/2));
 tone(9000,.5f);comp_opto_init(&s,48000);run(&s,&c,N,256);
 for(unsigned i=0;i<N;++i)reference[i]=naive(left[i]);
 double alias=amplitude(21000,out_l),raw_alias=amplitude(21000,reference);
 assert(alias>0&&alias<raw_alias*.45);
 printf("WARM_ALIAS alias21k=%.8f naive=%.8f reduction_db=%.3f\n",alias,raw_alias,20*log10(alias/raw_alias));
 c=(struct comp_config)COMP_CONFIG_DEFAULT;tone(1000,.5f);
 comp_opto_init(&s,48000);comp_opto_init(&t,48000);run(&s,&c,N,256);c.warm=1;run(&t,&c,N,256);
 assert(s.rms==t.rms&&s.gr==t.gr&&s.linear==t.linear); /* Colour does not change the detector. */
 /* A cold start, sign reversals, clip-boundary crossings and hot controls
  * remain bounded by current/previous dry peaks and the existing Gain cap. */
 float previous=0,limit=powf(10,.3f);comp_opto_init(&s,48000);
 for(unsigned pass=0;pass<8;++pass){
  for(unsigned i=0;i<N;++i){float peak=pass&1?2.f:.05f;left[i]=peak*(i%3==0?-1:1);right[i]=-left[i];}
  c=(struct comp_config)COMP_CONFIG_DEFAULT;c.threshold=pass&1?-48:0;c.ratio=pass&1?8:1;
  c.gain=6;c.warm=pass!=4;c.bypass=pass==6;c.attack=pass&1?100:1;c.release=pass&1?4000:100;
  run(&s,&c,N,pass&1?73:256);
  for(unsigned i=0;i<N;++i){float bound=fmaxf(fabsf(left[i]),fabsf(previous))*limit;
   assert(isfinite(out_l[i])&&fabsf(out_l[i])<=bound+1e-5f&&out_l[i]==-out_r[i]);previous=left[i];}
 }
 c=(struct comp_config)COMP_CONFIG_DEFAULT;c.threshold=0;c.ratio=1;comp_opto_init(&s,48000);
 for(unsigned i=0;i<N;++i)left[i]=right[i]=.8f;
 run(&s,&c,N,256);float prior=out_l[N-1];
 for(unsigned pass=0;pass<4;++pass){c.warm=pass!=2;c.bypass=pass==1;
  run(&s,&c,960,73);
  for(unsigned i=0;i<960;++i){assert(fabsf(out_l[i]-prior)<.001f);prior=out_l[i];}
  assert(s.warm_mix==(c.warm&&!c.bypass?1:0));
  if(c.bypass||!c.warm)assert(fabsf(out_l[959]-.8f)<1e-6);
 }
 /* Constant signals around the join and zero exercise ADAA's stable limits. */
 for(unsigned v=0;v<5;++v){float x=v==0?0:v==1?2.f/3:v==2?-2.f/3:v==3?1e-12f:16;
  for(unsigned i=0;i<N;++i)left[i]=right[i]=x;
  comp_opto_init(&s,48000);c.warm=1;c.bypass=0;run(&s,&c,960,31);
  assert(isfinite(out_l[959])&&fabsf(out_l[959])<=fabsf(x)+1e-6);
  if(x==0)assert(out_l[959]==0);
 }
 for(unsigned rate=0;rate<4;++rate){const double rates[]={8000,44100,96000,192000};
  comp_opto_init(&s,rates[rate]);for(unsigned i=0;i<N;++i)left[i]=.5f;
  float*mono_in[]={left},*mono_out[]={out_l};out_r[0]=123;
  assert(comp_opto_process(&s,&c,mono_in,mono_out,1,4096));
  assert(s.warm_mix==1&&s.warm_previous[1]==0&&out_r[0]==123&&isfinite(out_l[4095])&&out_l[4095]<=.5f);
 }
 float*in[]={left,right},*out[]={out_l,out_r};c.warm=2;out_l[0]=123;
 assert(!comp_opto_process(&s,&c,in,out,2,1)&&out_l[0]==123);
 puts("PASS Warm adds measured harmonics with compensated level and reduced aliasing; clean parity, stereo polarity, block invariance, bounded hot controls/Gain, ten-ms fades, Bypass dry, silence and join stability");
 return 0;
}
