#include "src/compressor.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static uint32_t pixels[320*164];
static unsigned hash(void){unsigned h=2166136261u;for(unsigned i=0;i<320*164;++i){assert((pixels[i]>>24)==255);h=(h^pixels[i])*16777619u;}return h;}
static void save(const char*name){char path[128];snprintf(path,sizeof(path),"build/compressor/art/%s.ppm",name);FILE*f=fopen(path,"wb");assert(f);fprintf(f,"P6\n320 164\n255\n");for(unsigned i=0;i<320*164;++i){unsigned char rgb[]={pixels[i]>>16,pixels[i]>>8,pixels[i]};fwrite(rgb,1,3,f);}assert(!fclose(f));}
static void frame(struct comp_animation*s,double now,float input,float threshold,float gain,unsigned bypass,float gr,unsigned hit){struct comp_config c=COMP_CONFIG_DEFAULT;c.threshold=threshold;c.gain=gain;c.bypass=bypass;s->reduction=gr;compressor_animation_frame(s,pixels,now,input,&c,hit);}
static unsigned awake_grain(void){unsigned n=0;for(int y=8;y<164;++y)for(int x=40;x<96;++x)n+=pixels[y*320+x]!=0xff000000u;return n;}
static void assert_sleep_background(void){
 unsigned cat=0;
 for(int y=0;y<164;++y)for(int x=0;x<320;++x){
  if(x>=96&&x<250&&y>=70&&y<161)cat+=pixels[y*320+x]!=0xff000000u;
  else assert(pixels[y*320+x]==0xff000000u);
 }
 assert(cat>100); /* The sleeping cat itself must remain visible. */
}
static void audio_frame(struct comp_animation*a,struct comp_opto*dsp,struct comp_config*c,double now,float input){
 float data[1600],out[1600],*in[]={data},*dest[]={out};
 for(unsigned i=0;i<1600;++i)data[i]=input;
 assert(comp_opto_process(dsp,c,in,dest,1,1600));
 a->reduction=dsp->gr;
 compressor_animation_frame(a,pixels,now,input>0?20*log10f(input):-60,c,0);
 assert(fabsf(a->pose-(1-expf(-dsp->gr/3)))<1e-6);
}
static void timings(void){
 struct comp_animation fast,slow;struct comp_opto fd,sd;
 struct comp_config fc=COMP_CONFIG_DEFAULT,sc=COMP_CONFIG_DEFAULT;
 compressor_animation_init(&fast);compressor_animation_init(&slow);comp_opto_init(&fd,48000);comp_opto_init(&sd,48000);
 fc.attack=1;sc.attack=100;
 for(unsigned i=1;i<=2;++i){audio_frame(&fast,&fd,&fc,1+i/30.,.5f);audio_frame(&slow,&sd,&sc,1+i/30.,.5f);}
 assert(fast.pose>slow.pose+.2f);
 audio_frame(&fast,&fd,&fc,1.1,.5f);save("attack-fast");
 audio_frame(&slow,&sd,&sc,1.1,.5f);save("attack-slow");
 fc=sc=(struct comp_config)COMP_CONFIG_DEFAULT;fc.release=100;sc.release=4000;
 comp_opto_init(&fd,48000);comp_opto_init(&sd,48000);
 for(unsigned i=0;i<60;++i){audio_frame(&fast,&fd,&fc,2+i/30.,.5f);audio_frame(&slow,&sd,&sc,2+i/30.,.5f);}
 for(unsigned i=1;i<=6;++i){audio_frame(&fast,&fd,&fc,4+i/30.,0);audio_frame(&slow,&sd,&sc,4+i/30.,0);}
 assert(fast.pose<slow.pose*.5f);save("release-long");
 audio_frame(&fast,&fd,&fc,4.233,0);save("release-short");
 float prior=slow.pose;sc.release=100;
 for(unsigned i=1;i<=6;++i)audio_frame(&slow,&sd,&sc,4.233+i/30.,0);
 assert(slow.pose<prior*.5f);
 /* Sustained actual reduction holds contact; no imaginary new strikes. */
 struct comp_config c=COMP_CONFIG_DEFAULT;
 slow.reduction=6;
 for(unsigned i=0;i<300;++i)compressor_animation_frame(&slow,pixels,5+i/30.,-6,&c,i);
 assert(!slow.striking&&slow.pose>.85f);
 /* Loud bow, low threshold, changing hit counter, Gain/Warm and Ratio1
  * cannot move the paw when the red GR meter says no compression. */
 c.ratio=1;c.threshold=-48;c.gain=6;c.warm=1;slow.reduction=0;
 for(unsigned i=0;i<30;++i){compressor_animation_frame(&slow,pixels,16+i/30.,-1,&c,i+400);assert(slow.pose==0);}
 slow.reduction=NAN;compressor_animation_frame(&slow,pixels,18,-1,&c,500);assert(slow.pose==0);
 slow.reduction=-4;compressor_animation_frame(&slow,pixels,18.1,-1,&c,501);assert(slow.pose==0);
 puts("PASS paw follows actual DSP GR Attack/Release without a second envelope; sustained reduction holds contact, loud input/Ratio1/Gain/Warm and synthetic hits cannot cause false strikes");
}
static unsigned count_colour(int x,uint32_t colour){unsigned n=0;for(int y=19;y<157;++y)n+=pixels[y*320+x]==colour;return n;}
static void meters(void){
 struct comp_meter m={{1,.1f},{.01f,0}};
 uint32_t kitten=pixels[120*320+140];
 compressor_meter_frame(pixels,&m,30);
 assert(count_colour(274,0xff00ffa7u)==138&&count_colour(280,0xff00ffa7u)==92);
 assert(count_colour(296,0xff00ffa7u)==46&&count_colour(302,0xff00ffa7u)==0);
 assert(count_colour(288,0xffd31145u)==69&&pixels[19*320+288]==0xffd31145u&&pixels[156*320+288]==0xff2e2e33u);
 assert(pixels[120*320+140]==kitten&&pixels[100*320+278]==0xff000000u);
 save("native-meter");
 m=(struct comp_meter){{NAN,-1},{INFINITY,0}};compressor_meter_frame(pixels,&m,NAN);
 assert(!count_colour(274,0xff00ffa7u)&&!count_colour(280,0xff00ffa7u)&&!count_colour(296,0xff00ffa7u)&&!count_colour(288,0xffd31145u));
 puts("PASS native-style stereo meter geometry, independent input/output, downward GR, nonfinite sanitising and kitten preservation");
}
int main(void){struct comp_animation a;compressor_animation_init(&a);
 for(unsigned i=0;i<80;++i)frame(&a,1+i/30.,-36,-18,0,0,0,0);
 assert(a.pose==0&&awake_grain()>50);unsigned watch=hash();save("native-watch");
 frame(&a,4,-6,-18,0,0,12,1);
 assert(a.pose>.95f);unsigned swat=hash();assert(watch!=swat);save("native-swat");
 frame(&a,4.01,-6,-18,6,0,12,1);assert(hash()!=swat);save("native-gain");
 frame(&a,5,-6,-18,6,1,0,1);assert(a.pose==0&&a.sleeping);assert(hash()!=swat);save("native-sleep");
 assert_sleep_background();
 for(unsigned i=1;i<8;++i){frame(&a,5+i*.05,-6,-18,6,1,0,1);assert_sleep_background();}
 frame(&a,6,-36,-18,0,0,0,1);assert(!a.sleeping&&a.pose==0&&awake_grain()>50);
 puts("PASS sleep keeps the cat visible on opaque black without grain at all breathing positions; awake grain returns on wake");
 timings();
 meters();
 puts("PASS OptoCat native compositor: watching, signal hit, eye gain, curled sleep, opaque bounds and wake");return 0;}
