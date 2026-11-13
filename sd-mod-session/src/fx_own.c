#include "fx_own.h"
#include "compressor.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#define TAU 6.283185307179586
#define PARAMS 8u
#ifndef SHIM_AMOUNT
#define SHIM_AMOUNT 2.0f
#endif
struct pitchc {float *buf;unsigned size,mask,w;double phase;};
struct fx_own {
    uint32_t id;double rate;float p[PARAMS];
    struct pitchc pitch[2];
    /* Shimmer FDN */
    float *line[4];unsigned len[4],pos[4];float lp[4],gain[4],damp;
    float *pre;unsigned pre_len,pre_pos;
    float shim_prev,in_gain,shim_gain;
    /* Reverse delay tone */
    float tone[2];
    /* Tremolo / ring phase, swell state, crusher hold */
    double phase;float gain_state,env;int open;
    unsigned hold_count;float hold[2];
    struct comp_opto opto;struct comp_config opto_config;
};
static int pow2_at_least(unsigned n){unsigned s=64;while(s<n)s<<=1;return (int)s;}
static float flush(float x){return fabsf(x)<1e-20f?0.f:x;}
static int pitch_init(struct pitchc *s,unsigned min_size)
{
    unsigned size=(unsigned)pow2_at_least(min_size);
    s->buf=calloc(size,sizeof(float));if(!s->buf)return 0;
    s->size=size;s->mask=size-1;s->w=0;s->phase=0;return 1;
}
static float pitch_tap(const struct pitchc *s,double delay)
{
    double pos=(double)s->w-delay;
    while(pos<0)pos+=s->size;
    unsigned i=(unsigned)pos&s->mask;double f=pos-floor(pos);
    return (float)(s->buf[i]*(1-f)+s->buf[(i+1)&s->mask]*f);
}
/* Two crossfaded read heads; ratio 2 = octave up, 0.5 = octave down, -1 = reversed. */
static float pitch_tick(struct pitchc *s,float in,double ratio,double window)
{
    s->buf[s->w]=in;
    s->phase+=(1.0-ratio)/window;s->phase-=floor(s->phase);
    double p2=s->phase+.5;if(p2>=1)p2-=1;
    double g1=.5-.5*cos(TAU*s->phase),g2=.5-.5*cos(TAU*p2);
    float o=(float)(pitch_tap(s,1+s->phase*window)*g1+pitch_tap(s,1+p2*window)*g2);
    s->w=(s->w+1)&s->mask;return o;
}
void fx_own_opto_config(const float *p,struct comp_config *c)
{
    float v[7];for(unsigned i=0;i<7;++i)v[i]=fminf(1.f,fmaxf(0.f,p[i]));
    memset(c,0,sizeof(*c));
    c->threshold=-48.f+48.f*v[0];c->ratio=1.f+7.f*v[1];c->gain=-6.f+12.f*v[2];
    c->attack=powf(100.f,v[3]);c->release=100.f*powf(40.f,v[4]);c->knee=12.f*v[5];
    c->warm=v[6]>=.5f?1u:0u;c->bypass=0;
}
int fx_own_is(uint32_t id){return id>=FX_OWN_SHIMMER&&id<FX_OWN_SHIMMER+FX_OWN_COUNT;}
int fx_own_pitch_semitones(float v){v=fminf(1.f,fmaxf(0.f,v));return (int)lroundf(v*48.f)-24;}
int fx_own_shimmer_semitones(float v){static const int steps[5]={-12,5,7,12,19};v=fminf(1.f,fmaxf(0.f,v));int i=(int)(v*4.999f);return steps[i];}
void fx_own_destroy(void *object)
{
    struct fx_own *s=object;if(!s)return;
    for(unsigned i=0;i<2;++i)free(s->pitch[i].buf);
    for(unsigned i=0;i<4;++i)free(s->line[i]);
    free(s->pre);free(s);
}
void *fx_own_create(uint32_t id,double rate,const float *parameters)
{
    if(!fx_own_is(id)||!parameters||!isfinite(rate)||rate<8000||rate>192000)return NULL;
    struct fx_own *s=calloc(1,sizeof(*s));if(!s)return NULL;
    s->id=id;s->rate=rate;
    for(unsigned i=0;i<PARAMS;++i)s->p[i]=fminf(1.f,fmaxf(0.f,parameters[i]));
    int ok=1;
    if(id==FX_OWN_PITCH)for(unsigned c=0;c<2;++c)ok&=pitch_init(&s->pitch[c],(unsigned)(rate*.12));
    else if(id==FX_OWN_REVERSE)for(unsigned c=0;c<2;++c)ok&=pitch_init(&s->pitch[c],(unsigned)(rate*1.6)+8);
    else if(id==FX_OWN_SHIMMER){
        ok&=pitch_init(&s->pitch[0],(unsigned)(rate*.12));
        static const unsigned base[4]={1171,1559,1877,2237};
        float size=.6f+1.4f*s->p[4];
        for(unsigned i=0;i<4;++i){
            s->len[i]=(unsigned)lround(base[i]*rate/48000.0*size);if(s->len[i]<8)s->len[i]=8;
            s->line[i]=calloc(s->len[i]+1,sizeof(float));ok&=s->line[i]!=NULL;
            double rt60=.3*pow(40.0,s->p[1]);
            s->gain[i]=(float)pow(10.0,-3.0*s->len[i]/(rate*rt60));
        }
        { float g=0; for(unsigned i=0;i<4;++i)g+=s->gain[i]; g*=.25f; s->in_gain=sqrtf(fmaxf(.02f,1.f-g*g))*1.8f;s->shim_gain=(1.f-g)*SHIM_AMOUNT; }
        s->damp=.1f+.85f*s->p[5];
        s->pre_len=(unsigned)(rate*.2*s->p[6])+1;s->pre=calloc(s->pre_len,sizeof(float));ok&=s->pre!=NULL;
    }
    if(id==FX_OWN_OPTO){comp_opto_init(&s->opto,rate);fx_own_opto_config(s->p,&s->opto_config);}
    if(!ok){fx_own_destroy(s);return NULL;}
    return s;
}
static void shimmer(struct fx_own *s,float *x,size_t frames)
{
    double ratio=pow(2.0,fx_own_shimmer_semitones(s->p[3])/12.0);
    float mix=s->p[0],amount=s->p[2],dc=1.f-s->damp;
    for(size_t n=0;n<frames;++n){
        float l=x[n*2],r=x[n*2+1],in=.5f*(l+r);
        float delayed=s->pre[s->pre_pos];s->pre[s->pre_pos]=in;if(++s->pre_pos>=s->pre_len)s->pre_pos=0;
        float y[4];for(unsigned i=0;i<4;++i)y[i]=s->line[i][s->pos[i]];
        float wet=.25f*(y[0]+y[1]+y[2]+y[3]);
        float pitched=pitch_tick(&s->pitch[0],wet,ratio,s->rate*.04);
        float inject=tanhf((delayed*s->in_gain+amount*s->shim_gain*pitched)*.7f);
        for(unsigned i=0;i<4;++i)s->lp[i]=flush(s->lp[i]+(y[i]-s->lp[i])*dc);
        float a=s->lp[0],b=s->lp[1],c=s->lp[2],d=s->lp[3];
        float f[4]={.5f*(a+b+c+d),.5f*(a-b+c-d),.5f*(a+b-c-d),.5f*(a-b-c+d)};
        for(unsigned i=0;i<4;++i){
            s->line[i][s->pos[i]]=flush(inject*.6f+s->gain[i]*f[i]);
            if(++s->pos[i]>=s->len[i])s->pos[i]=0;
        }
        float oct=(.1f+1.2f*s->p[2])*pitched;float wl=.5f*(y[0]+y[2])+oct,wr=.5f*(y[1]+y[3])+oct;
        x[n*2]=l*(1-mix)+wl*mix*2.5f;x[n*2+1]=r*(1-mix)+wr*mix*2.5f;
    }
}
static void pitch_shift(struct fx_own *s,float *x,size_t frames)
{
    double semis=fx_own_pitch_semitones(s->p[0])+(s->p[1]-.5)*1.0;
    double ratio=pow(2.0,semis/12.0);float mix=s->p[2];
    for(size_t n=0;n<frames;++n)for(unsigned c=0;c<2;++c){
        float in=x[n*2+c],o=pitch_tick(&s->pitch[c],in,ratio,s->rate*.04);
        x[n*2+c]=in*(1-mix)+o*mix;
    }
}
static void reverse(struct fx_own *s,float *x,size_t frames)
{
    double window=s->rate*(.08+1.42*s->p[0]);
    float fb=.85f*s->p[1],mix=s->p[2],tone=.1f+.9f*s->p[3];
    for(size_t n=0;n<frames;++n)for(unsigned c=0;c<2;++c){
        float in=x[n*2+c];
        float o=pitch_tick(&s->pitch[c],in+fb*s->tone[c],-1.0,window);
        s->tone[c]=flush(s->tone[c]+(o-s->tone[c])*tone);
        x[n*2+c]=in*(1-mix)+o*mix;
    }
}
static void tremolo(struct fx_own *s,float *x,size_t frames)
{
    double hz=.5*pow(28.0,s->p[0]),step=TAU*hz/s->rate;float depth=s->p[1],shape=s->p[2],spread=s->p[3]*3.14159265f;
    for(size_t n=0;n<frames;++n){
        s->phase+=step;if(s->phase>TAU)s->phase-=TAU;
        for(unsigned c=0;c<2;++c){
            float lfo=sinf((float)s->phase+(c?spread:0.f));
            float sq=tanhf(lfo*(1.f+14.f*shape))/tanhf(1.f+14.f*shape);
            float u=.5f+.5f*(lfo*(1-shape)+sq*shape);
            x[n*2+c]*=1.f-depth*(1.f-u);
        }
    }
}
static void swell(struct fx_own *s,float *x,size_t frames)
{
    float attack=1.f/(float)(s->rate*(.02*pow(100.0,s->p[0]))),close=1.f/(float)(s->rate*.012);
    float open_at=powf(10.f,(-60.f+45.f*(1.f-s->p[1]))/20.f),mix=s->p[2];
    float rel=expf(-1.f/(float)(s->rate*.06));
    for(size_t n=0;n<frames;++n){
        float peak=fmaxf(fabsf(x[n*2]),fabsf(x[n*2+1]));
        s->env=peak>s->env?peak:rel*s->env+(1-rel)*peak;s->env=flush(s->env);
        if(!s->open&&s->env>open_at)s->open=1;
        else if(s->open&&s->env<open_at*.4f)s->open=0;
        s->gain_state=s->open?fminf(1.f,s->gain_state+attack):fmaxf(0.f,s->gain_state-close);
        float g=1.f-mix*(1.f-s->gain_state);
        x[n*2]*=g;x[n*2+1]*=g;
    }
}
static void ring(struct fx_own *s,float *x,size_t frames)
{
    double hz=20.0*pow(250.0,s->p[0]),step=TAU*hz/s->rate;float mix=s->p[1];
    for(size_t n=0;n<frames;++n){
        s->phase+=step;if(s->phase>TAU)s->phase-=TAU;
        float c=sinf((float)s->phase);
        for(unsigned k=0;k<2;++k)x[n*2+k]=x[n*2+k]*(1-mix)+x[n*2+k]*c*mix;
    }
}
static void crush(struct fx_own *s,float *x,size_t frames)
{
    int bits=2+(int)lroundf(14.f*s->p[0]);float levels=(float)(1u<<(bits-1)),mix=s->p[2];
    unsigned factor=(unsigned)lround(pow(32.0,s->p[1]));if(!factor)factor=1;
    for(size_t n=0;n<frames;++n){
        if(s->hold_count==0){
            for(unsigned c=0;c<2;++c){float v=fminf(1.f,fmaxf(-1.f,x[n*2+c]));s->hold[c]=floorf(v*levels+.5f)/levels;}
        }
        s->hold_count=(s->hold_count+1)%factor;
        for(unsigned c=0;c<2;++c)x[n*2+c]=x[n*2+c]*(1-mix)+s->hold[c]*mix;
    }
}
static float opto(struct fx_own *s,float *x,size_t frames)
{
    float lo=1.f;float a[256],b[256];
    for(size_t at=0;at<frames;){
        size_t n=frames-at<256?frames-at:256;
        for(size_t i=0;i<n;++i){a[i]=x[(at+i)*2];b[i]=x[(at+i)*2+1];}
        float *in[2]={a,b};
        if(comp_opto_process(&s->opto,&s->opto_config,in,in,2,n)){
            for(size_t i=0;i<n;++i){x[(at+i)*2]=a[i];x[(at+i)*2+1]=b[i];}
            float g=powf(10.f,-s->opto.gr/20.f);if(g<lo)lo=g;
        }
        at+=n;
    }
    return lo;
}
float fx_own_process(void *object,float *samples,size_t frames)
{
    struct fx_own *s=object;if(!s||!samples)return 1.f;
    switch(s->id){
    case FX_OWN_OPTO:return opto(s,samples,frames);
    case FX_OWN_SHIMMER:shimmer(s,samples,frames);break;
    case FX_OWN_PITCH:pitch_shift(s,samples,frames);break;
    case FX_OWN_REVERSE:reverse(s,samples,frames);break;
    case FX_OWN_TREMOLO:tremolo(s,samples,frames);break;
    case FX_OWN_SWELL:swell(s,samples,frames);break;
    case FX_OWN_RING:ring(s,samples,frames);break;
    case FX_OWN_CRUSH:crush(s,samples,frames);break;
    default:break;
    }
    return 1.f;
}
