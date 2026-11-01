#include "compressor.h"
#include <math.h>
#include <string.h>
void comp_opto_init(struct comp_opto*s,double rate){
 memset(s,0,sizeof(*s));s->rate=rate;s->linear=s->target=1;
}
int comp_opto_process(struct comp_opto*s,const struct comp_config*c,float *const*in,float *const*out,unsigned channels,size_t n){
 if(!s||!c||!in||!out||channels<1||channels>2||!isfinite(s->rate)||s->rate<8000||s->rate>192000||
    !isfinite(c->threshold)||c->threshold< -48||c->threshold>0||!isfinite(c->ratio)||c->ratio<1||c->ratio>8||
    !isfinite(c->gain)||c->gain< -12||c->gain>6||
    !isfinite(c->attack)||c->attack<1||c->attack>100||
    !isfinite(c->release)||c->release<100||c->release>4000||
    !isfinite(c->knee)||c->knee<0||c->knee>12)return 0;
 for(unsigned ch=0;ch<channels;++ch)if(!in[ch]||!out[ch])return 0;
 const float detector=expf(-1.f/(float)(s->rate*.010));
 const float attack=expf(-1.f/(float)(s->rate*c->attack*.001));
 const float release=expf(-1.f/(float)(s->rate*c->release*.00005));
 const float memory_attack=expf(-1.f/(float)(s->rate*c->attack*.020));
 const float memory_release=expf(-1.f/(float)(s->rate*c->release*.001));
 const float smooth=1-expf(-1.f/(float)(s->rate*.003));
 const float slope=1-1/c->ratio;
 for(size_t i=0;i<n;++i){
  float l=in[0][i],r=channels==2?in[1][i]:l;
  if(!isfinite(l))l=0;
  if(!isfinite(r))r=0;
  /* Max-channel RMS detection also works for anti-phase stereo. */
  float peak=fmaxf(fabsf(l),fabsf(r));if(peak>16)peak=16;
  s->rms=detector*s->rms+(1-detector)*peak*peak;
  if(!(s->phase&3u)){
   float level=10*log10f(fmaxf(s->rms,1e-12f)),over=level-c->threshold;
   float half=c->knee*.5f;
   s->desired=over<=-half?0:over>=half?slope*over:slope*(over+half)*(over+half)/(2*c->knee);
  }
  float target=s->desired;
  float a=target>s->fast?attack:release,b=target>s->slow?memory_attack:memory_release;
  s->fast=target+a*(s->fast-target);s->slow=target+b*(s->slow-target);
  s->gr=.7f*s->fast+.3f*s->slow;
  if(!(s->phase++&3u))s->target=c->bypass?1:powf(10,(c->gain-s->gr)/20);
  s->linear+=smooth*(s->target-s->linear);
  out[0][i]=l*s->linear;if(channels==2)out[1][i]=r*s->linear;
 }
 if(c->bypass)s->gr=0;
 if(s->rms<1e-20f)s->rms=0;
 if(s->fast<1e-10f)s->fast=0;
 if(s->slow<1e-10f)s->slow=0;
 return 1;
}
