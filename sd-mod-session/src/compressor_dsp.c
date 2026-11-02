#include "compressor.h"
#include <math.h>
#include <string.h>
void comp_opto_init(struct comp_opto*s,double rate){
 memset(s,0,sizeof(*s));s->rate=rate;s->linear=s->target=1;
}
/* Mild cubic soft saturation, drive1.5 with reciprocal output compensation.
 * f is odd, monotonic and non-expansive. First-order ADAA integrates f over
 * the previous/current sample interval to reduce aliasing. The central
 * polynomial is factored analytically to avoid subtractive cancellation;
 * crossing intervals use a double-precision continuous antiderivative.
 * Theory: Bilbao et al., doi:10.1109/LSP.2017.2675541. */
static double warm_primitive(double x){
 double u=fabs(x*1.5);
 return u<=1 ? (.5*u*u-u*u*u*u/12)/2.25 : ((2./3)*u-.25)/2.25;
}
static float warm_sample(float sample,float previous){
 double x=sample,p=previous;
 if(fabs(x)<=2./3 && fabs(p)<=2./3)
  return (float)((x+p)*(.5-(2.25/12)*(x*x+p*p)));
 if(x>=2./3 && p>=2./3)return 4.f/9;
 if(x<=-2./3 && p<=-2./3)return -4.f/9;
 double delta=x-p;
 if(fabs(delta)<1e-7){
  double u=(x+p)*.75;
  return (float)(fabs(u)<=1 ? (u-u*u*u/3)/1.5 : copysign(4./9,u));
 }
 return (float)((warm_primitive(x)-warm_primitive(p))/delta);
}
int comp_opto_process(struct comp_opto*s,const struct comp_config*c,float *const*in,float *const*out,unsigned channels,size_t n){
 if(!s||!c||!in||!out||channels<1||channels>2||!isfinite(s->rate)||s->rate<8000||s->rate>192000||
    !isfinite(c->threshold)||c->threshold< -48||c->threshold>0||!isfinite(c->ratio)||c->ratio<1||c->ratio>8||
    !isfinite(c->gain)||c->gain< -6||c->gain>6||
    !isfinite(c->attack)||c->attack<1||c->attack>100||
    !isfinite(c->release)||c->release<100||c->release>4000||
    !isfinite(c->knee)||c->knee<0||c->knee>12||c->warm>1)return 0;
 for(unsigned ch=0;ch<channels;++ch)if(!in[ch]||!out[ch])return 0;
 const float detector=expf(-1.f/(float)(s->rate*.010));
 const float attack=expf(-1.f/(float)(s->rate*c->attack*.001));
 const float release=expf(-1.f/(float)(s->rate*c->release*.00005));
 const float memory_attack=expf(-1.f/(float)(s->rate*c->attack*.020));
 const float memory_release=expf(-1.f/(float)(s->rate*c->release*.001));
 const float smooth=1-expf(-1.f/(float)(s->rate*.003));
 const float slope=1-1/c->ratio;
 const float warm_step=1.f/(float)(s->rate*.010);
 const float warm_target=c->warm&&!c->bypass?1:0;
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
  /* Warm is a separate colour stage; the dry RMS detector/GR and all knob
   * ranges stay unchanged. Mix35% colour, with a ten-ms toggle/Bypass fade.
   * Never boost the driven signal or use an automatic loudness increase. */
  s->warm_mix=warm_target>s->warm_mix?fminf(warm_target,s->warm_mix+warm_step):fmaxf(warm_target,s->warm_mix-warm_step);
  float wl=l,wr=r;
  if(s->warm_mix>0){
   wl+=s->warm_mix*.35f*(warm_sample(l,s->warm_previous[0])-l);
   if(channels==2)wr+=s->warm_mix*.35f*(warm_sample(r,s->warm_previous[1])-r);
  }
  s->warm_previous[0]=l;s->warm_previous[1]=channels==2?r:0;
  out[0][i]=wl*s->linear;if(channels==2)out[1][i]=wr*s->linear;
 }
 if(c->bypass)s->gr=0;
 if(s->rms<1e-20f)s->rms=0;
 if(s->fast<1e-10f)s->fast=0;
 if(s->slow<1e-10f)s->slow=0;
 return 1;
}
