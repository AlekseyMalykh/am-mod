#include "compressor.h"
#include <math.h>
#include <string.h>
void comp_opto_init(struct comp_opto*s,double rate){
 memset(s,0,sizeof(*s));s->rate=rate;s->linear=s->target=1;
}
int comp_opto_process(struct comp_opto*s,const struct comp_config*c,float *const*in,float *const*out,unsigned channels,size_t n){
 if(!s||!c||!in||!out||channels<1||channels>2||!isfinite(s->rate)||s->rate<8000||s->rate>192000||
    !isfinite(c->threshold)||c->threshold< -48||c->threshold>0||!isfinite(c->ratio)||c->ratio<1||c->ratio>8||
    !isfinite(c->gain)||c->gain< -12||c->gain>18)return 0;
 for(unsigned ch=0;ch<channels;++ch)if(!in[ch]||!out[ch])return 0;
 const float detector=expf(-1.f/(float)(s->rate*.010));
 const float attack=expf(-1.f/(float)(s->rate*.010));
 const float release=expf(-1.f/(float)(s->rate*.060));
 const float memory_attack=expf(-1.f/(float)(s->rate*.200));
 const float memory_release=expf(-1.f/(float)(s->rate*1.200));
 const float smooth=1-expf(-1.f/(float)(s->rate*.003));
 const float slope=1-1/c->ratio;
 for(size_t i=0;i<n;++i){
  float l=in[0][i],r=channels==2?in[1][i]:l;
  if(!isfinite(l))l=0;
  if(!isfinite(r))r=0;
  /* Max-channel RMS detection also works for anti-phase stereo. */
  float peak=fmaxf(fabsf(l),fabsf(r));if(peak>16)peak=16;
  s->rms=detector*s->rms+(1-detector)*peak*peak;
  float level=10*log10f(fmaxf(s->rms,1e-12f)),over=level-c->threshold,target;
  if(over<=-3)target=0;else if(over>=3)target=slope*over;else target=slope*(over+3)*(over+3)/12;
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
