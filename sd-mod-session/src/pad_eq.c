#define _GNU_SOURCE
#include "pad_eq.h"
#include "music_settings.h"
#include <stdatomic.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#define VALID (UINT64_C(1)<<31)
static _Atomic uint64_t settings[128];
static const char*const types[]={"Bell","Low Pass","High Pass","Notch","Low Shelf","High Shelf"};
static const unsigned eq_types[]={FX_EQ_BELL,FX_EQ_LOW_PASS,FX_EQ_HIGH_PASS,FX_EQ_NOTCH,FX_EQ_LOW_SHELF,FX_EQ_HIGH_SHELF};
uint64_t pad_eq_default(void){return VALID|UINT64_C(72)|(UINT64_C(64)<<7)|(UINT64_C(47)<<14)|(UINT64_C(1)<<24);}
int pad_eq_valid(uint64_t v){return !v||((v&VALID)&&!(v&~UINT64_C(0x81ffffff))&&((v>>21)&7)<PAD_EQ_TYPES);}
uint64_t pad_eq_edit(uint64_t v,int field,unsigned value,int relative){
    if(!pad_eq_valid(v)||field<0||field>PAD_EQ_BYPASS||value>127)return v;
    if(!v)v=pad_eq_default();
    unsigned shift=field==PAD_EQ_FREQUENCY?0:field==PAD_EQ_GAIN?7:field==PAD_EQ_Q?14:field==PAD_EQ_TYPE?21:24;
    unsigned mask=field==PAD_EQ_TYPE?7:field==PAD_EQ_BYPASS?1:127,max=field==PAD_EQ_TYPE?PAD_EQ_TYPES-1:mask;
    int current=(int)((v>>shift)&mask),n=relative?current+(value>64?(int)value-128:(int)value):(int)(value*(max+1)/128);
    if(n<0)n=0;
    if((unsigned)n>max)n=(int)max;
    v=(v&~((uint64_t)mask<<shift))|((uint64_t)n<<shift);
    if(field!=PAD_EQ_BYPASS)v&=~(UINT64_C(1)<<24);
    return v;
}
void pad_eq_config(uint64_t v,double rate,struct fx_eq_config*out){
    if(!v||!pad_eq_valid(v))v=pad_eq_default();
    memset(out,0,sizeof(*out));
    unsigned type=(v>>21)&7;out->count=1;out->bypass=(unsigned)((v>>24)&1);
    double hz=20*pow(1000,(v&127)/127.),gain=((int)((v>>7)&127)-64)*18./63.,q=.2*pow(80,((v>>14)&127)/127.);
    if(hz>rate*.45)hz=rate*.45;
    if(gain< -18)gain=-18;
    if(type==PAD_EQ_HIGH_PASS||type==PAD_EQ_LOW_PASS||type==PAD_EQ_NOTCH)gain=0;
    out->band[0]=(struct fx_eq_band){hz,gain,q,eq_types[type],1};
}
void pad_eq_publish(unsigned pad,uint64_t v){if(pad<128&&pad_eq_valid(v))atomic_store_explicit(settings+pad,v,memory_order_release);}
uint64_t pad_eq_published(unsigned pad){return pad<128?atomic_load_explicit(settings+pad,memory_order_acquire):0;}
void pad_eq_publish_all(const uint64_t v[128]){for(unsigned i=0;i<128;++i)pad_eq_publish(i,v[i]);}
struct SoundState{uintptr_t instrument;uint64_t setting;double rate;struct fx_eq eq;};
struct Lane{_Atomic long thread;struct SoundState sound[128];};
static struct Lane lanes[8];
int pad_eq_audio(unsigned pad,uintptr_t instrument,double rate,float**channels,unsigned count,unsigned frames,int silent){
    if(pad>=128||!instrument||!channels||count<1||count>2||!channels[0]||(count==2&&!channels[1])||frames>8192||!isfinite(rate)||rate<8000||rate>192000)return 0;
    uint64_t v=pad_eq_published(pad);if(!v)return 1;
    long thread=syscall(SYS_gettid);struct Lane*lane=NULL;
    for(unsigned i=0;i<8;++i){long owner=atomic_load_explicit(&lanes[i].thread,memory_order_acquire);if(owner==thread){lane=lanes+i;break;}
        if(!owner&&atomic_compare_exchange_strong_explicit(&lanes[i].thread,&owner,thread,memory_order_acq_rel,memory_order_acquire)){lane=lanes+i;break;}}
    if(!lane)return 0;
    struct SoundState*s=lane->sound+pad;
    if(s->instrument!=instrument||s->rate!=rate){struct fx_eq_config config;pad_eq_config(v,rate,&config);s->instrument=instrument;s->rate=rate;s->setting=v;if(!fx_eq_init(&s->eq,rate,&config))return 0;}
    else if(s->setting!=v){struct fx_eq_config config;pad_eq_config(v,rate,&config);if(!fx_eq_update(&s->eq,&config))return 0;s->setting=v;}
    if(silent){memset(s->eq.history,0,sizeof(s->eq.history));return 1;}
    float block[256*2];for(unsigned at=0;at<frames;){unsigned n=frames-at;if(n>256)n=256;
        for(unsigned i=0;i<n;++i){block[2*i]=channels[0][at+i];block[2*i+1]=channels[count==2?1:0][at+i];}
        fx_eq_process(&s->eq,block,n);
        for(unsigned i=0;i<n;++i){channels[0][at+i]=block[2*i];if(count==2)channels[1][at+i]=block[2*i+1];}at+=n;
    }return 1;
}

/* ---- Spectrum of the edited pad (all Sample B3 pages) -------------------
 * Audio thread: post-EQ mono samples of the watched pad go into a ring.
 * UI thread: the EQ page asks for the pad, takes the last 4096 samples,
 * windows them (Hann), runs a radix-2 FFT and draws bars on a log axis
 * behind the response curve, falling 1.5 dB per frame. */
#define SPEC_N 4096
#define SPEC_RING 8192
static float spec_ring[SPEC_RING];
static _Atomic unsigned spec_pos;
static _Atomic int spec_pad=-1;
static _Atomic uint64_t spec_rate_bits,spec_frames;
static uint64_t spec_seen_ms,spec_painted_frames;static int spec_visible_bars;
static uint64_t spec_now_ms(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000u+(uint64_t)t.tv_nsec/1000000u;}
void pad_eq_spectrum_watch(int pad){
    if(pad<0||pad>=128){atomic_store_explicit(&spec_pad,-1,memory_order_relaxed);return;}
    atomic_store_explicit(&spec_pad,pad,memory_order_relaxed);spec_seen_ms=spec_now_ms();
}
int pad_eq_spectrum_wanted(unsigned pad){return atomic_load_explicit(&spec_pad,memory_order_relaxed)==(int)pad;}
void pad_eq_spectrum_capture(unsigned pad,float**channels,unsigned count,unsigned frames,double rate){
    if(!pad_eq_spectrum_wanted(pad)||!channels||!channels[0]||count<1||count>2||frames>8192)return;
    unsigned at=atomic_load_explicit(&spec_pos,memory_order_relaxed);
    for(unsigned i=0;i<frames;++i){float v=channels[0][i];if(count==2&&channels[1])v=.5f*(v+channels[1][i]);spec_ring[(at+i)&(SPEC_RING-1)]=v;}
    uint64_t bits;memcpy(&bits,&rate,8);atomic_store_explicit(&spec_rate_bits,bits,memory_order_relaxed);
    atomic_store_explicit(&spec_pos,at+frames,memory_order_release);
    atomic_fetch_add_explicit(&spec_frames,frames,memory_order_relaxed);
}
/* Ask for a repaint while the page is open and new audio arrived. */
int pad_eq_spectrum_dirty(void){
    if(atomic_load_explicit(&spec_pad,memory_order_relaxed)<0)return 0;
    if(spec_now_ms()-spec_seen_ms>600){atomic_store_explicit(&spec_pad,-1,memory_order_relaxed);return 0;}
    return atomic_load_explicit(&spec_frames,memory_order_relaxed)!=spec_painted_frames||spec_visible_bars;
}
static void fft(float*re,float*im,unsigned n){
    for(unsigned i=1,j=0;i<n;++i){unsigned bit=n>>1;for(;j&bit;bit>>=1)j^=bit;j^=bit;if(i<j){float t=re[i];re[i]=re[j];re[j]=t;t=im[i];im[i]=im[j];im[j]=t;}}
    for(unsigned len=2;len<=n;len<<=1){double a=-2*M_PI/len;float wr=(float)cos(a),wi=(float)sin(a);
        for(unsigned i=0;i<n;i+=len){float cr=1,ci=0;for(unsigned k=0;k<len/2;++k){
            float ur=re[i+k],ui=im[i+k],vr=re[i+k+len/2]*cr-im[i+k+len/2]*ci,vi=re[i+k+len/2]*ci+im[i+k+len/2]*cr;
            re[i+k]=ur+vr;im[i+k]=ui+vi;re[i+k+len/2]=ur-vr;im[i+k+len/2]=ui-vi;
            float nr=cr*wr-ci*wi;ci=cr*wi+ci*wr;cr=nr;}}}
}
void pad_eq_spectrum_paint(unsigned pad,int left,int top,int width,int height,uint32_t tint,const struct resample_painter*p){
    if(pad>=128||!p||!p->rect||width<1||height<1)return;
    if(music_pad_lighting_mode()&&music_mod_setting(MUSIC_SETTING_SPECTRUM)){
        uint32_t hue=music_identity_rgb((unsigned)music_pad_colour(pad));
        /* Spectrum stays behind native response/envelope lines. */
        tint=(tint&0xff000000u)|(((hue>>16)&255)*3/5<<16)|(((hue>>8)&255)*3/5<<8)|((hue&255)*3/5);
    }
    static float shown[320];static int shown_pad=-1;
    if((int)pad!=shown_pad){shown_pad=(int)pad;for(int i=0;i<320;++i)shown[i]=-96;}
    pad_eq_spectrum_watch((int)pad);
    uint64_t frames=atomic_load_explicit(&spec_frames,memory_order_relaxed);
    int fresh=frames!=spec_painted_frames;spec_painted_frames=frames;
    if(width>320)width=320;
    if(fresh){
        static float re[SPEC_N],im[SPEC_N];unsigned end=atomic_load_explicit(&spec_pos,memory_order_acquire);
        for(unsigned i=0;i<SPEC_N;++i){float w=.5f-.5f*(float)cos(2*M_PI*i/(SPEC_N-1));re[i]=spec_ring[(end-SPEC_N+i)&(SPEC_RING-1)]*w;im[i]=0;}
        fft(re,im,SPEC_N);
        uint64_t bits=atomic_load_explicit(&spec_rate_bits,memory_order_relaxed);double rate;memcpy(&rate,&bits,8);
        if(!(rate>=8000&&rate<=192000))rate=44100;
        for(int x=0;x<width;++x){
            double lo=20*pow(1000,x/(double)width),hi=20*pow(1000,(x+1)/(double)width);
            unsigned a=(unsigned)(lo*SPEC_N/rate),b=(unsigned)(hi*SPEC_N/rate);if(a<1)a=1;if(b<a+1)b=a+1; /* no DC bin */if(b>SPEC_N/2)b=SPEC_N/2;
            float peak=0;for(unsigned k=a;k<b;++k){float m=re[k]*re[k]+im[k]*im[k];if(m>peak)peak=m;}
            float db=peak>0?10.f*log10f(peak)-60.f:-96.f; /* Hann window sum N/2: full scale near 0 dB */
            shown[x]=db>shown[x]?db:shown[x]-1.5f;
        }
    }else for(int x=0;x<width;++x)shown[x]-=1.5f;
    spec_visible_bars=0;
    for(int x=0;x<width;++x){
        float db=shown[x];if(db< -72)continue;if(db>0)db=0;spec_visible_bars=1;
        int h=(int)((db+72)*height/72);if(h<1)continue;
        p->rect(p->user,left+x,top+height-h,1,h,tint);
    }
}
void pad_eq_paint(uint64_t v,unsigned pad,int shift,int focus,const struct resample_painter*p){
    if(!p||!p->text||!p->rect)return;
    struct fx_eq_config cfg;pad_eq_config(v,48000,&cfg);unsigned type=v?((v>>21)&7):0;
    const uint32_t white=0xffffffffu,amber=0xffffc247u,grey=0xff777777u;char s[64];
    /* Keep the native Sample side strips (volume/pan) visible. Only the
     * centre graph and our complete K footer belong to this overlay. */
    p->rect(p->user,14,33,292,165,0xff000000u);p->rect(p->user,0,198,320,42,0xff000000u);snprintf(s,sizeof(s),"%c%02u  EQ%s",'A'+pad/16,pad%16+1,cfg.bypass?"  BYPASS":"");p->text(p->user,s,18,36,284,24,18,white,0);
    int left=42,top=72,width=260,height=105;
    pad_eq_spectrum_paint(pad,left,top,width,height,0xff5a4419u,p);
    for(int db=-18;db<=18;db+=9){int y=top+(18-db)*height/36;p->rect(p->user,left,y,width,1,db?0xff252525:0xff666666);snprintf(s,sizeof(s),"%+d",db);p->text(p->user,s,16,y-6,23,13,10,grey,1);}
    int previous=-1;for(int x=0;x<width;++x){double hz=20*pow(1000,x/(double)(width-1)),db=fx_eq_response_db(&cfg,48000,hz);if(db>18)db=18;if(db< -18)db=-18;int y=top+(int)lround((18-db)*height/36);if(previous<0)previous=y;
        int a=previous<y?previous:y,b=previous>y?previous:y;p->rect(p->user,left+x,a,1,b-a+1,cfg.bypass?grey:amber);previous=y;}
    double point_db=cfg.band[0].gain_db;if(type==PAD_EQ_LOW_PASS||type==PAD_EQ_HIGH_PASS||type==PAD_EQ_NOTCH)point_db=0;
    int x=left+(int)lround(log(cfg.band[0].hz/20)/log(1000)*(width-1)),y=top+(int)lround((18-point_db)*height/36);p->rect(p->user,x-2,y-2,5,5,amber);
    p->text(p->user,"20",left,180,30,12,10,grey,0);p->text(p->user,"1k",174,180,30,12,10,grey,0);p->text(p->user,"20k",270,180,32,12,10,grey,0);
    const char*names[]={"Freq",shift?"Q":"Gain","Type"};char values[3][32];snprintf(values[0],32,"%.0f Hz",cfg.band[0].hz);
    if(shift)snprintf(values[1],32,"%.2f",cfg.band[0].q);else if(type==PAD_EQ_LOW_PASS||type==PAD_EQ_HIGH_PASS||type==PAD_EQ_NOTCH)strcpy(values[1],"--");else snprintf(values[1],32,"%+.1f dB",cfg.band[0].gain_db);
    snprintf(values[2],32,"%s",cfg.bypass?"Bypass":types[type]);
    uint64_t actual=v?v:pad_eq_default();
    for(unsigned k=0;k<3;++k){
        int x=k==2?213:(int)k*106,w=k==2?107:106;
        int enabled=!(k==1&&!shift&&(type==PAD_EQ_LOW_PASS||type==PAD_EQ_HIGH_PASS||type==PAD_EQ_NOTCH));
        uint32_t ink=!enabled?grey:(int)k==focus?amber:white;
        p->text(p->user,names[k],x+4,200,k==1?w-22:w-8,12,12,ink,1);
        p->text(p->user,values[k],x+4,214,w-8,19,16,ink,1);
        if((int)k==focus&&enabled){p->rect(p->user,x,198,w,2,ink);p->rect(p->user,x,198,2,34,ink);p->rect(p->user,x+w-2,198,2,34,ink);}
        if(k==2){
            unsigned selected=cfg.bypass?0:type+1;
            for(unsigned n=0;n<PAD_EQ_TYPES+1;++n){int a=(int)n*(w-8)/(PAD_EQ_TYPES+1),b=(int)(n+1)*(w-8)/(PAD_EQ_TYPES+1);p->rect(p->user,x+4+a,235,b-a-1,2,n==selected?ink:0xff3b3b3bu);}
        }else{
            unsigned value=k==0?(unsigned)(actual&127):(unsigned)((actual>>(shift?14:7))&127);
            p->rect(p->user,x+4,235,w-8,2,0xff3b3b3bu);
            int fill=(int)(value*(unsigned)(w-8)/127);if(enabled&&fill)p->rect(p->user,x+4,235,fill,2,ink);
        }
    }
    {   /* Shift badge as the native K panel draws it: 10x12 frame with "S". */
        uint32_t c=shift?0xffff4747u:white;int bx=200,top=200;
        p->rect(p->user,bx,top,10,1,c);p->rect(p->user,bx,top+11,10,1,c);p->rect(p->user,bx,top,1,12,c);p->rect(p->user,bx+9,top,1,12,c);
        p->text(p->user,"S",bx+1,top+1,8,10,10,c,1);
    }
}
