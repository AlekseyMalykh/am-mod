#include "src/compressor.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static float l[48000],r[48000],a[48000],b[48000];
static void tone(unsigned n,float amplitude){for(unsigned i=0;i<n;++i){l[i]=amplitude*sinf((float)i*.13f);r[i]=-l[i];}}
static void run(struct comp_opto*s,struct comp_config*c,unsigned n,unsigned block){
 for(unsigned at=0;at<n;){unsigned frames=n-at;if(frames>block)frames=block;
  float*in[]={l+at,r+at},*out[]={a+at,b+at};assert(comp_opto_process(s,c,in,out,2,frames));at+=frames;}
}
int main(void){
 struct comp_config c={-18,3,0,0};struct comp_opto x,y;
 tone(48000,.8f);comp_opto_init(&x,48000);run(&x,&c,48000,256);assert(x.gr>8&&x.gr<13);
 for(unsigned i=0;i<48000;++i){assert(isfinite(a[i])&&a[i]==-b[i]);}
 float reference[48000];memcpy(reference,a,sizeof(reference));
 comp_opto_init(&y,48000);run(&y,&c,48000,73);
 for(unsigned i=0;i<48000;++i)assert(fabsf(a[i]-reference[i])<1e-7f);
 c.ratio=1;comp_opto_init(&x,48000);run(&x,&c,48000,256);assert(!memcmp(a,l,sizeof(a)));
 c.gain=6;comp_opto_init(&x,48000);run(&x,&c,48000,256);assert(fabsf(x.linear-powf(10,.3f))<.001f);
 c.bypass=1;run(&x,&c,48000,256);assert(x.gr==0&&fabsf(x.linear-1)<.001f);
 c=(struct comp_config){-18,3,0,0};
 tone(48000,.9f);comp_opto_init(&x,48000);comp_opto_init(&y,48000);
 run(&x,&c,960,128);run(&y,&c,48000,128);
 memset(l,0,sizeof(l));memset(r,0,sizeof(r));run(&x,&c,9600,128);run(&y,&c,9600,128);
 assert(y.gr>x.gr+1); /* Optical memory depends on preceding duration. */
 float*in[]={l,r},*out[]={a,b};c.ratio=NAN;a[0]=123;assert(!comp_opto_process(&x,&c,in,out,2,1)&&a[0]==123);
 puts("PASS Opto DSP: compression, anti-phase stereo, block invariance, unity, gain, bypass, program-dependent recovery and invalid input");return 0;
}
