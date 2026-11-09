#ifndef AM_SPECTRUM_PLOT_H
#define AM_SPECTRUM_PLOT_H
#include <math.h>
static inline void spectrum_fft(float*re,float*im,unsigned n){
    for(unsigned i=1,j=0;i<n;++i){unsigned bit=n>>1;for(;j&bit;bit>>=1)j^=bit;j^=bit;if(i<j){float t=re[i];re[i]=re[j];re[j]=t;t=im[i];im[i]=im[j];im[j]=t;}}
    for(unsigned len=2;len<=n;len<<=1){double a=-2*3.141592653589793/len;float wr=(float)cos(a),wi=(float)sin(a);
        for(unsigned i=0;i<n;i+=len){float cr=1,ci=0;for(unsigned k=0;k<len/2;++k){
            float ur=re[i+k],ui=im[i+k],vr=re[i+k+len/2]*cr-im[i+k+len/2]*ci,vi=re[i+k+len/2]*ci+im[i+k+len/2]*cr;
            re[i+k]=ur+vr;im[i+k]=ui+vi;re[i+k+len/2]=ur-vr;im[i+k+len/2]=ui-vi;
            float nr=cr*wr-ci*wi;ci=cr*wi+ci*wr;cr=nr;}}}
}

static inline float spectrum_column(const float*re,const float*im,unsigned n,double rate,int x,int width)
{
    double lo=20*pow(1000,x/(double)width),hi=20*pow(1000,(x+1)/(double)width);
    unsigned a=(unsigned)(lo*n/rate),b=(unsigned)(hi*n/rate);
    if(a<1)a=1;
    if(b<a+1)b=a+1;
    if(b>n/2)b=n/2;
    float peak=0;
    for(unsigned k=a;k<b;++k){
        float magnitude=re[k]*re[k]+im[k]*im[k];
        if(magnitude>peak)peak=magnitude;
    }
    return peak>0?10.f*log10f(peak)-60.f+20.f*log10f(4096.f/n):-96.f;
}
#endif
