#define main unused_eq_main
#include "check_fx_native.c"
#undef main
#include "src/pad_eq.h"
#include "src/pad_eq_native.h"
#include "src/music_settings.h"
#define F(p,o,T) (*(T*)((char*)(p)+(o)))
static unsigned char instrument[0x2468],program[0x2900],rate_property[0x60],buffer[0x130],renderer[8];
static uintptr_t program_vtable[67];static float left[2048],right[2048],*channels[]={left,right};
static unsigned passes,source_calls;static int muted;
static void patch(uintptr_t at,void*fn){uint32_t jump[]={0x58000050,0xd61f0200};memcpy((void*)(base+at),jump,8);memcpy((void*)(base+at+8),&fn,8);__builtin___clear_cache((char*)(base+at),(char*)(base+at+16));}
static void inserts(void*p,void*b,unsigned frames,unsigned stage){assert(p==instrument&&b==buffer&&frames==2048&&stage>=1&&stage<=3);++passes;}
static void native_fx(void*p,void*b,unsigned n){assert(p==instrument&&b==buffer&&n==2048);}
static void native_render(void*p,void*b,unsigned frames){assert(p==renderer&&b==buffer&&frames==2048);++source_calls;
 F(b,0x128,unsigned char)=muted;for(unsigned i=0;i<2048;++i)left[i]=right[i]=muted?0:.1f*(float)sin(2*3.141592653589793*1000*i/48000);}
static double rms(void){double e=0;for(unsigned i=1024;i<2048;++i)e+=left[i]*left[i];return sqrt(e/1024);}
static FILE*trace;
static unsigned layout,first_curve,segments,has_freq,has_type,has_value;
static void rect(void*u,int x,int y,int w,int h,uint32_t colour){assert(!u&&x>=0&&y>=0&&w>0&&h>0&&x+w<=320&&y+h<=240);if(layout&&y<198){assert(x>=14&&x+w<=306);}
if(layout&&x==42&&w==1&&colour==0xffffc247){assert(h==1);++first_curve;}if(layout&&x>=217&&y==235&&h==2)++segments;if(trace)fprintf(trace,"R %d %d %d %d %08x\n",x,y,w,h,colour);}
static void text(void*u,const char*s,int x,int y,int w,int h,unsigned z,uint32_t c,int centre){assert(!u&&s&&x>=0&&y>=0&&x+w<=320&&y+h<=240&&z>=10);if(layout&&y<198){assert(x>=14&&x+w<=306);}
if(layout){if(!strcmp(s,"Freq"))has_freq=1;if(!strcmp(s,"Type"))has_type=1;if(x>=217&&y==214&&*s)has_value=1;}if(trace)fprintf(trace,"T %d %d %d %d %u %08x %d %s\n",x,y,w,h,z,c,centre,s);}
static unsigned char layout_screen[0xa000];static unsigned native_side_mask;
static void capture_native_bounds(void*p,uint64_t xy,uint64_t wh){
 uintptr_t offset=(uintptr_t)p-(uintptr_t)layout_screen;
 if(offset==0x2088||offset==0x2868){
  assert((int)xy==(offset==0x2088?5:311)&&(int)(xy>>32)==81);
  assert((int)wh==4&&(int)(wh>>32)==108);native_side_mask|=offset==0x2088?1:2;
 }
}
static void native_side_bounds(void){
 patch(0x1160fc0,capture_native_bounds);
 F(layout_screen,0x28,int)=320;F(layout_screen,0x2c,int)=240;
 FN(0x1660370,void(*)(void*))(layout_screen);assert(native_side_mask==3);
 puts("PASS actual native Sample resized places volume/pan side indicators at x5/x311 y81 h108; EQ primitives never cover either strip");
}
#ifndef PAD_EQ_PROOF_MAIN
#define PAD_EQ_PROOF_MAIN main
#endif
int PAD_EQ_PROOF_MAIN(int argc,char**argv){
 assert(argc==3);struct sigaction sa={.sa_sigaction=fault,.sa_flags=SA_SIGINFO};sigaction(SIGSEGV,&sa,NULL);sigaction(SIGILL,&sa,NULL);sigaction(SIGABRT,&sa,NULL);load(argv[1]);
 phase="single-point config/persistence";
 struct music_project*p=calloc(1,sizeof(*p)),*copy=calloc(1,sizeof(*copy));char*payload=malloc(MUSIC_PAYLOAD_BYTES);assert(p&&copy&&payload);music_reset();
 uint64_t v=pad_eq_default();assert(pad_eq_valid(v)&&pad_eq_valid(0)&&!pad_eq_valid(UINT64_MAX));
 for(unsigned type=0;type<PAD_EQ_TYPES;++type){for(unsigned f=0;f<128;++f){uint64_t e=pad_eq_edit(v,PAD_EQ_TYPE,type*128/PAD_EQ_TYPES+1,0);e=pad_eq_edit(e,PAD_EQ_FREQUENCY,f,0);e=pad_eq_edit(e,PAD_EQ_Q,127-f,0);e=pad_eq_edit(e,PAD_EQ_GAIN,f,0);struct fx_eq_config cfg;pad_eq_config(e,48000,&cfg);assert(fx_eq_valid(&cfg,48000));}}
 v=pad_eq_edit(v,PAD_EQ_GAIN,22,0);assert(music_set_pad_eq(18,v)&&music_get_pad_eq(18)==v&&pad_eq_published(18)==v);music_snapshot(p);int n=music_encode(p,payload,MUSIC_PAYLOAD_BYTES);assert(n&&music_decode(payload,n,copy)&&copy->pad_eq[18]==v);music_reset();assert(!pad_eq_published(18));music_replace(copy);assert(pad_eq_published(18)==v);
 assert(music_decode("MPCS13 0 0 0\n",13,copy)&&!copy->pad_eq[18]);
 puts("PASS one-point settings: six types, every knob extreme, per-pad persistence with MPCS14, legacy MPCS13 defaults, published snapshot and unrelated pads unchanged");
 phase="post-native instrument processing";patch(0x276a310,inserts);patch(0x2760ae0,native_render);patch(0x2769d60,native_fx);
 F(instrument,0x638,void*)=rate_property;F(rate_property,0x50,float)=48000;F(instrument,0x918,float)=1;F(instrument,0x2310,void*)=program;F(instrument,0x910,void*)=renderer;F(instrument,0x2168,unsigned)=18;
 F(program,0,void*)=program_vtable;program_vtable[66]=base+0x18de470;F(program,0xdc8,const char*)="test-program-id";
 F(buffer,0,int)=2;F(buffer,4,int)=2048;F(buffer,16,float**)=channels;
 assert(pad_eq_native_install(base)&&pad_eq_native_install(base)&&!pad_eq_native_install(base+4096));pad_eq_native_program(program);
 pad_eq_publish(18,0);FN(0x2802f90,void(*)(void*,void*,unsigned,unsigned))(instrument,buffer,2048,1);double dry=rms();assert(dry>.06&&dry<.08&&passes==3&&source_calls==1);
 pad_eq_publish(18,v);FN(0x2802f90,void(*)(void*,void*,unsigned,unsigned))(instrument,buffer,2048,1);double cut=rms();assert(cut<dry*.5&&cut>0);
 struct pad_eq_native_stats stats;pad_eq_native_stats_read(&stats);assert(stats.calls==2&&stats.configured==1&&stats.matched==1&&stats.processed==1);pad_eq_native_report();
 uint64_t bypass=pad_eq_edit(v,PAD_EQ_BYPASS,127,0);pad_eq_publish(18,bypass);for(unsigned i=0;i<3;++i)FN(0x2802f90,void(*)(void*,void*,unsigned,unsigned))(instrument,buffer,2048,1);assert(fabs(rms()-dry)<1e-6);
 F(instrument,0x2168,unsigned)=19;FN(0x2802f90,void(*)(void*,void*,unsigned,unsigned))(instrument,buffer,2048,1);assert(fabs(rms()-dry)<1e-6);F(instrument,0x2168,unsigned)=18;
 pad_eq_publish(18,v);F(program,0xdc8,const char*)="other-program";FN(0x2802f90,void(*)(void*,void*,unsigned,unsigned))(instrument,buffer,2048,1);assert(fabs(rms()-dry)<1e-6);F(program,0xdc8,const char*)="test-program-id";
 muted=1;FN(0x2802f90,void(*)(void*,void*,unsigned,unsigned))(instrument,buffer,2048,1);assert(!rms());muted=0;pad_eq_native_clear();FN(0x2802f90,void(*)(void*,void*,unsigned,unsigned))(instrument,buffer,2048,1);assert(fabs(rms()-dry)<1e-6);
 printf("PASS actual native2802f90 through relocated PAC/stack/guard prologue, native stage order preserved, post-processing Bell cut RMS %.6f -> %.6f, bypass, other pad/program and silence; native generator/inserts are fixtures\n",dry,cut);
 trace=fopen(argv[2],"w");assert(trace);struct resample_painter painter={NULL,rect,text};pad_eq_paint(v,18,0,0,1,&painter);fclose(trace);trace=NULL;
 layout=1;for(unsigned type=0;type<PAD_EQ_TYPES;++type)for(int shift=0;shift<2;++shift){uint64_t e=pad_eq_edit(v,PAD_EQ_TYPE,type*128/PAD_EQ_TYPES+1,0);e=pad_eq_edit(e,PAD_EQ_FREQUENCY,0,0);first_curve=segments=has_freq=has_type=has_value=0;pad_eq_paint(e,18,0,shift,2,&painter);assert(first_curve==1&&segments==7&&has_freq&&has_type&&has_value);}layout=0;
 puts("PASS EQ layout: first graph point has no artificial vertical join, native side strips untouched by every graph/text primitive, all six types at minimum frequency, Shift retains K3 type/value, seven type segments and compact Freq");native_side_bounds();free(p);free(copy);free(payload);return 0;
}
