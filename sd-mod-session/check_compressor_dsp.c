#include "src/compressor.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static float l[48000],r[48000],a[48000],b[48000];
#ifdef CHECK_LEGACY_OPTO
void comp_opto_init_r26i(struct comp_opto*,double);
int comp_opto_process_r26i(struct comp_opto*,const struct comp_config*,float *const*,float *const*,unsigned,size_t);
static float legacy_a[48000],legacy_b[48000];
#endif
static void tone(unsigned n,float amplitude){for(unsigned i=0;i<n;++i){l[i]=amplitude*sinf((float)i*.13f);r[i]=-l[i];}}
static void run(struct comp_opto*s,struct comp_config*c,unsigned n,unsigned block){
 for(unsigned at=0;at<n;){unsigned frames=n-at;if(frames>block)frames=block;
  float*in[]={l+at,r+at},*out[]={a+at,b+at};assert(comp_opto_process(s,c,in,out,2,frames));at+=frames;}
}
static void controls(void){
 struct comp_config c=COMP_CONFIG_DEFAULT;struct comp_opto x,y;
 for(unsigned i=0;i<48000;++i)l[i]=r[i]=.5f;
 c.attack=1;comp_opto_init(&x,48000);run(&x,&c,960,128);
 c.attack=100;comp_opto_init(&y,48000);run(&y,&c,960,128);
 assert(x.gr>y.gr+1); /* Attack changes the actual reduction envelope. */
 c=(struct comp_config)COMP_CONFIG_DEFAULT;comp_opto_init(&x,48000);run(&x,&c,48000,256);y=x;
 memset(l,0,sizeof(l));memset(r,0,sizeof(r));
 c.release=100;run(&x,&c,9600,128);c.release=4000;run(&y,&c,9600,128);assert(y.gr>x.gr+1);
 for(unsigned i=0;i<48000;++i)l[i]=r[i]=powf(10,(-18-3)/20.f);
 c=(struct comp_config)COMP_CONFIG_DEFAULT;c.knee=0;comp_opto_init(&x,48000);run(&x,&c,48000,256);assert(x.gr==0);
 c.knee=12;comp_opto_init(&y,48000);run(&y,&c,48000,256);assert(y.gr>.2f);
 /* No allowed setting may add more than +6dB, even during transitions. */
 float limit=powf(10,6/20.f);comp_opto_init(&x,48000);
 for(unsigned pass=0;pass<8;++pass){
  tone(48000,pass&1?2.f:.05f);c=(struct comp_config)COMP_CONFIG_DEFAULT;
  c.threshold=pass&1?-48:0;c.ratio=pass&1?8:1;c.gain=6;c.bypass=pass==6;
  c.attack=pass&1?100:1;c.release=pass&1?4000:100;c.knee=pass&1?12:0;
  run(&x,&c,48000,73);
  for(unsigned i=0;i<48000;++i){assert(isfinite(a[i])&&fabsf(a[i])<=fabsf(l[i])*limit+1e-6f);assert(a[i]==-b[i]);}
 }
 float*in[]={l,r},*out[]={a,b};c.gain=6.25f;a[0]=123;assert(!comp_opto_process(&x,&c,in,out,2,1)&&a[0]==123);
 c=(struct comp_config)COMP_CONFIG_DEFAULT;c.attack=NAN;assert(!comp_opto_process(&x,&c,in,out,2,1));
 c.attack=10;c.release=99;assert(!comp_opto_process(&x,&c,in,out,2,1));
 c.release=1200;c.knee=13;assert(!comp_opto_process(&x,&c,in,out,2,1));
 puts("PASS Opto Attack/Release/Knee response, hard-knee zero, +6dB transition ceiling and invalid settings");
}
int main(void){
 struct comp_config c=COMP_CONFIG_DEFAULT;struct comp_opto x,y;
 tone(48000,.8f);comp_opto_init(&x,48000);run(&x,&c,48000,256);assert(x.gr>8&&x.gr<13);
#ifdef CHECK_LEGACY_OPTO
 comp_opto_init_r26i(&y,48000);
 for(unsigned at=0;at<48000;){unsigned n=48000-at;if(n>256)n=256;float*in[]={l+at,r+at},*out[]={legacy_a+at,legacy_b+at};assert(comp_opto_process_r26i(&y,&c,in,out,2,n));at+=n;}
 for(unsigned i=0;i<48000;++i){assert(fabsf(a[i]-legacy_a[i])<2e-6f);assert(fabsf(b[i]-legacy_b[i])<2e-6f);}
 puts("PASS default Opto response matches the approved r26i DSP");
#endif
 for(unsigned i=0;i<48000;++i){assert(isfinite(a[i])&&a[i]==-b[i]);}
 float reference[48000];memcpy(reference,a,sizeof(reference));
 comp_opto_init(&y,48000);run(&y,&c,48000,73);
 for(unsigned i=0;i<48000;++i)assert(fabsf(a[i]-reference[i])<1e-7f);
 c.ratio=1;comp_opto_init(&x,48000);run(&x,&c,48000,256);assert(!memcmp(a,l,sizeof(a)));
 c.gain=-6;comp_opto_init(&x,48000);run(&x,&c,48000,256);assert(fabsf(x.linear-powf(10,-.3f))<.001f);
 c.gain=-6.25f;{float*in[]={l,r},*out[]={a,b};a[0]=123;assert(!comp_opto_process(&x,&c,in,out,2,1)&&a[0]==123);}
 c.gain=6;comp_opto_init(&x,48000);run(&x,&c,48000,256);assert(fabsf(x.linear-powf(10,.3f))<.001f);
 c.bypass=1;run(&x,&c,48000,256);assert(x.gr==0&&fabsf(x.linear-1)<.001f);
 c=(struct comp_config)COMP_CONFIG_DEFAULT;
 tone(48000,.9f);comp_opto_init(&x,48000);comp_opto_init(&y,48000);
 run(&x,&c,960,128);run(&y,&c,48000,128);
 memset(l,0,sizeof(l));memset(r,0,sizeof(r));run(&x,&c,9600,128);run(&y,&c,9600,128);
 assert(y.gr>x.gr+1); /* Optical memory depends on preceding duration. */
 float*in[]={l,r},*out[]={a,b};c.ratio=NAN;a[0]=123;assert(!comp_opto_process(&x,&c,in,out,2,1)&&a[0]==123);
 controls();
 puts("PASS Opto DSP: compression, anti-phase stereo, block invariance, unity, gain, bypass, program-dependent recovery and invalid input");return 0;
}
