#include "src/fx_own.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#define printf(...) (fprintf(stderr,__VA_ARGS__),fflush(stderr))
#define RATE 48000.0
#define TAU 6.283185307179586
static float defaults[7][8]={
 {.35f,.6f,.5f,.8f,.5f,.5f,.1f,0},  /* shimmer: +12 */
 {.75f,.5f,1,0,0,0,0,0},            /* pitch +12 */
 {.4f,.3f,.5f,.7f,0,0,0,0},         /* reverse */
 {.35f,.6f,0,.5f,0,0,0,0},          /* tremolo */
 {.5f,.5f,1,0,0,0,0,0},             /* slow attack */
 {.4f,.5f,0,0,0,0,0,0},             /* ring */
 {.6f,.5f,1,0,0,0,0,0}};            /* crush */
static double goertzel(const float *x,size_t n,double hz){
    double w=TAU*hz/RATE,c=2*cos(w),s1=0,s2=0;for(size_t i=0;i<n;++i){double s=x[i]+c*s1-s2;s2=s1;s1=s;}
    return sqrt(s1*s1+s2*s2-c*s1*s2)*2/n;}
static double band(const float*x,size_t n,double c,double w){double e=0;for(double f=c-w;f<=c+w;f+=20){double g=goertzel(x,n,f);e+=g*g;}return sqrt(e);}
static void run(void*fx,float*buf,size_t frames){for(size_t at=0;at<frames;at+=256){size_t n=frames-at<256?frames-at:256;fx_own_process(fx,buf+at*2,n);}}
static float *sine(size_t frames,double hz,float amp){float*b=malloc(frames*2*sizeof(float));assert(b);
    for(size_t i=0;i<frames;++i)b[i*2]=b[i*2+1]=amp*(float)sin(TAU*hz*i/RATE);return b;}
static float *mono(const float*b,size_t frames){float*m=malloc(frames*sizeof(float));assert(m);for(size_t i=0;i<frames;++i)m[i]=b[i*2];return m;}
int main(void){
    assert(!fx_own_is(0)&&!fx_own_is(0x41435663u)&&fx_own_is(FX_OWN_SHIMMER)&&fx_own_is(FX_OWN_CRUSH));
    assert(!fx_own_create(FX_OWN_PITCH,100,defaults[1])&&!fx_own_create(FX_OWN_PITCH,RATE,NULL)&&!fx_own_create(1,RATE,defaults[1]));
    /* Every effect: finite, bounded, audible, alive for 4 s of mixed signal, and CPU cost reported. */
    for(unsigned k=0;k<7;++k){
        void*fx=fx_own_create(FX_OWN_SHIMMER+k,RATE,defaults[k]);assert(fx);
        size_t frames=(size_t)(RATE*4);float*b=sine(frames,440,.5f);srand(7);
        for(size_t i=0;i<frames;++i){float n=((float)rand()/RAND_MAX-.5f)*.2f;b[i*2]+=n;b[i*2+1]+=n;}
        clock_t t0=clock();run(fx,b,frames);double sec=(double)(clock()-t0)/CLOCKS_PER_SEC;
        double energy=0,peak=0;for(size_t i=0;i<frames*2;++i){assert(isfinite(b[i]));energy+=b[i]*b[i];if(fabs(b[i])>peak)peak=fabs(b[i]);}
        fprintf(stderr,"k=%u energy %.1f peak %.2f\n",k,energy,peak);assert(energy>1&&peak<4.0);
        printf("effect %u: peak %.2f, host cost %.1f ns/frame (%.2f%% of one host core at 48 kHz)\n",k,peak,sec/frames*1e9,sec/(frames/RATE)*100);
        fx_own_destroy(fx);free(b);
    }
    /* Pitch Shifter +12: energy moves from 1 kHz to 2 kHz. */
    {void*fx=fx_own_create(FX_OWN_PITCH,RATE,defaults[1]);size_t n=(size_t)RATE;float*b=sine(n,1000,.5f);run(fx,b,n);
     float*m=mono(b+(size_t)(RATE*.5)*2,n/2);double one=goertzel(m,n/2,1000),two=goertzel(m,n/2,2000);
     printf("pitch +12: 1 kHz %.3f, 2 kHz %.3f\n",one,two);assert(two>.15&&two>4*one);free(m);free(b);fx_own_destroy(fx);
     assert(fx_own_pitch_semitones(.5f)==0&&fx_own_pitch_semitones(1)==24&&fx_own_pitch_semitones(0)==-24&&fx_own_pitch_semitones(.75f)==12);}
    /* Pitch Shifter -12 */
    {float d[8]={.25f,.5f,1};void*fx=fx_own_create(FX_OWN_PITCH,RATE,d);size_t n=(size_t)RATE;float*b=sine(n,1000,.5f);run(fx,b,n);
     float*m=mono(b+(size_t)(RATE*.5)*2,n/2);double half=goertzel(m,n/2,500),one=goertzel(m,n/2,1000);
     printf("pitch -12: 500 Hz %.3f, 1 kHz %.3f\n",half,one);assert(half>.15&&half>4*one);free(m);free(b);fx_own_destroy(fx);}
    /* Shimmer: a 1 kHz burst grows an octave-up tail around 2 kHz; amount 0 (a plain reverb) does not. */
    {double tail[2],rms[2];
     for(unsigned on=0;on<2;++on){
         float p[8]={.35f,.6f,on?.8f:0.f,.8f,.5f,.5f,.1f,0};void*fx=fx_own_create(FX_OWN_SHIMMER,RATE,p);
         size_t n=(size_t)(RATE*4);float*b=calloc(n*2,sizeof(float));
         for(size_t i=0;i<(size_t)(RATE*.3);++i)b[i*2]=b[i*2+1]=.5f*(float)sin(TAU*1000*i/RATE);run(fx,b,n);
         size_t from=(size_t)(RATE*.8),len=(size_t)(RATE*2.4);float*m=mono(b+from*2,len);
         tail[on]=band(m,len,2000,300);double r=0;for(size_t i=0;i<len;++i)r+=m[i]*m[i];rms[on]=sqrt(r/len);
         free(m);free(b);fx_own_destroy(fx);}
     printf("shimmer tail near 2 kHz: off %.5f, on %.5f (rms %.4f / %.4f)\n",tail[0],tail[1],rms[0],rms[1]);
     assert(tail[1]>2.0*tail[0]&&rms[1]>.003&&rms[0]>.0005);
     assert(fx_own_shimmer_semitones(0)==-12&&fx_own_shimmer_semitones(.6f)==7&&fx_own_shimmer_semitones(1)==19&&fx_own_shimmer_semitones(.8f)==12);}
    /* Shimmer stability at maximum settings: decays after 30 s of loud noise. */
    {float mx[8]={1,1,1,.8f,1,0,0,0};void*fx=fx_own_create(FX_OWN_SHIMMER,RATE,mx);assert(fx);srand(5);
     for(size_t blocks=0;blocks<(size_t)(RATE*30/256);++blocks){float b[512];for(unsigned i=0;i<256;++i)b[i*2]=b[i*2+1]=((float)rand()/RAND_MAX-.5f)*1.6f;fx_own_process(fx,b,256);
         for(unsigned i=0;i<512;++i){assert(isfinite(b[i])&&fabsf(b[i])<8);}}
     float last=0;size_t total=(size_t)(RATE*60/256);for(size_t blocks=0;blocks<total;++blocks){float b[512]={0};fx_own_process(fx,b,256);if(blocks>=total-(size_t)(RATE*5/256))for(unsigned i=0;i<512;++i)last=fmaxf(last,fabsf(b[i]));}
     printf("max shimmer, last 5 s of 60 s silence: %.6f\n",last);assert(last<.01);fx_own_destroy(fx);}
    /* Reverse Delay: two bursts (500 Hz then 1500 Hz, 150 ms apart) come back in the opposite order, closer together. */
    {float d[8]={.4f,0,1,1};void*fx=fx_own_create(FX_OWN_REVERSE,RATE,d);size_t n=(size_t)(RATE*3);float*b=calloc(n*2,sizeof(float));assert(b);
     for(size_t i=0;i<(size_t)(RATE*.03);++i){b[(size_t)(RATE*1.0+i)*2]=.8f*(float)sin(TAU*500*i/RATE);b[(size_t)(RATE*1.15+i)*2]=.8f*(float)sin(TAU*1500*i/RATE);}
     for(size_t i=0;i<n;++i)b[i*2+1]=b[i*2];run(fx,b,n);
     float*m=mono(b,n);size_t hop=(size_t)(RATE*.005),win=(size_t)(RATE*.03);double best1500=0,t1500=0;
     for(size_t at=(size_t)(RATE*1.2);at+win<(size_t)(RATE*2.6);at+=hop){double g=goertzel(m+at,win,1500);if(g>best1500){best1500=g;t1500=(double)at/RATE;}}
     double best500=0,t500=0;
     for(size_t at=(size_t)((t1500+.03)*RATE);at<(size_t)((t1500+.2)*RATE)&&at+win<n;at+=hop){double g=goertzel(m+at,win,500);if(g>best500){best500=g;t500=(double)at/RATE;}}
     printf("reverse: strongest 1500 Hz burst at %.3f s, following 500 Hz burst at %.3f s (gap %.3f s, reversed: 1500 first, 0.150 apart; forward would put 500 first)\n",t1500,t500,t500-t1500);
     assert(best1500>.02&&best500>.02&&t500-t1500>.12&&t500-t1500<.18);
     free(m);free(b);fx_own_destroy(fx);}
    /* Tremolo depth. */
    {float d[8]={.5f,1,0,0};void*fx=fx_own_create(FX_OWN_TREMOLO,RATE,d);size_t n=(size_t)(RATE*2);float*b=sine(n,440,.5f);run(fx,b,n);
     float lo=9,hi=0;for(size_t i=0;i<n;i+=480){float p=0;for(size_t j=0;j<480&&i+j<n;++j)p=fmaxf(p,fabsf(b[(i+j)*2]));if(p<lo)lo=p;if(p>hi)hi=p;}
     printf("tremolo: %.3f..%.3f\n",lo,hi);assert(lo<.1f&&hi>.45f);free(b);fx_own_destroy(fx);}
    /* Slow Attack: onset ramps in. */
    {float d[8]={.6f,.5f,1};void*fx=fx_own_create(FX_OWN_SWELL,RATE,d);size_t n=(size_t)(RATE*3);float*b=calloc(n*2,sizeof(float));
     for(size_t i=(size_t)(RATE*.1);i<n;++i)b[i*2]=b[i*2+1]=.5f*(float)sin(TAU*330*i/RATE);run(fx,b,n);
     float a=0,z=0;for(size_t i=(size_t)(RATE*.1);i<(size_t)(RATE*.15);++i)a=fmaxf(a,fabsf(b[i*2]));
     for(size_t i=(size_t)(RATE*2.5);i<n;++i)z=fmaxf(z,fabsf(b[i*2]));
     printf("swell: onset %.3f, steady %.3f\n",a,z);assert(a<.1f&&z>.45f);free(b);fx_own_destroy(fx);}
    /* Ring Mod: sidebands at |f±fc|. */
    {float d[8]={.4f,1};void*fx=fx_own_create(FX_OWN_RING,RATE,d);size_t n=(size_t)RATE;float*b=sine(n,1000,.5f);run(fx,b,n);
     double fc=20*pow(250.0,.4);float*m=mono(b,n);double lo=goertzel(m,n,fabs(1000-fc)),hi=goertzel(m,n,1000+fc),carrier=goertzel(m,n,1000);
     printf("ring: carrier %.3f, sidebands %.3f/%.3f at fc %.0f Hz\n",carrier,lo,hi,fc);assert(lo>.1&&hi>.1&&carrier<.02);free(m);free(b);fx_own_destroy(fx);}
    /* Bit Crusher: few distinct levels. */
    {float d[8]={0,0,1};void*fx=fx_own_create(FX_OWN_CRUSH,RATE,d);size_t n=4800;float*b=sine(n,200,.9f);run(fx,b,n);
     float seen[64];unsigned count=0;for(size_t i=0;i<n;++i){unsigned f=0;for(unsigned j=0;j<count;++j)if(fabsf(seen[j]-b[i*2])<1e-6f)f=1;if(!f){assert(count<64);seen[count++]=b[i*2];}}
     printf("crush 2-bit: %u distinct levels\n",count);assert(count<=5);free(b);fx_own_destroy(fx);}
    puts("PASS Am MOD own effects: Shimmer (octave tail, amount 0 removes it, max settings decay), Pitch +/-12, Reverse swell, Tremolo, Slow Attack, Ring Mod, Bit Crusher, bounds and finite output");
    return 0;
}
