#include "src/compressor.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint32_t pixels[320*164];
static unsigned hash(void){unsigned h=2166136261u;for(unsigned i=0;i<320*164;++i){assert((pixels[i]>>24)==255);h=(h^pixels[i])*16777619u;}return h;}
static void save(const char*name){char path[128];snprintf(path,sizeof(path),"build/compressor/art/%s.ppm",name);FILE*f=fopen(path,"wb");assert(f);fprintf(f,"P6\n320 164\n255\n");for(unsigned i=0;i<320*164;++i){unsigned char rgb[]={pixels[i]>>16,pixels[i]>>8,pixels[i]};fwrite(rgb,1,3,f);}assert(!fclose(f));}
static void frame(struct comp_animation*s,double now,float input,float threshold,float gain,unsigned bypass,unsigned hit){struct comp_config c=COMP_CONFIG_DEFAULT;c.threshold=threshold;c.gain=gain;c.bypass=bypass;compressor_animation_frame(s,pixels,now,input,&c,hit);}
static void timings(void){
 struct comp_animation fast,slow;struct comp_config c=COMP_CONFIG_DEFAULT;
 compressor_animation_init(&fast);compressor_animation_init(&slow);fast.time=slow.time=1;
 for(unsigned i=1;i<=2;++i){c.attack=1;compressor_animation_frame(&fast,pixels,1+i/30.,-6,&c,1);c.attack=100;compressor_animation_frame(&slow,pixels,1+i/30.,-6,&c,1);}
 assert(fast.pose>.9f&&slow.pose<.6f);
 compressor_animation_frame(&fast,pixels,1.067,-6,&c,1);save("attack-fast");
 compressor_animation_frame(&slow,pixels,1.067,-6,&c,1);save("attack-slow");
 fast.pose=slow.pose=1;fast.striking=slow.striking=0;fast.time=slow.time=2;fast.level=slow.level=-60;
 for(unsigned i=1;i<=6;++i){c.release=100;compressor_animation_frame(&fast,pixels,2+i/30.,-60,&c,1);c.release=4000;compressor_animation_frame(&slow,pixels,2+i/30.,-60,&c,1);}
 assert(fast.pose<.01f&&slow.pose>.8f);
 save("release-long");c.release=100;compressor_animation_frame(&fast,pixels,2.201,-60,&c,1);save("release-short");
 /* Editing Release during recovery changes speed without starting a hit. */
 float prior=slow.pose;compressor_animation_frame(&slow,pixels,2.233,-60,&c,1);assert(slow.pose<prior*.5f&&!slow.striking);
 c=(struct comp_config)COMP_CONFIG_DEFAULT;
 for(unsigned i=0;i<300;++i)compressor_animation_frame(&slow,pixels,3+i/30.,-6,&c,1);
 assert(!slow.striking&&slow.pose<.301f); /* Steady input does not retrigger. */
 puts("PASS paw Attack rise, Release return, live timing changes and no repeated strikes on steady input");
}
int main(void){struct comp_animation a;compressor_animation_init(&a);
 for(unsigned i=0;i<80;++i)frame(&a,1+i/30.,-36,-18,0,0,0);
 assert(a.pose==0);unsigned watch=hash();save("native-watch");
 frame(&a,4,-6,-18,0,0,1);
 assert(a.pose>.95f);unsigned swat=hash();assert(watch!=swat);save("native-swat");
 frame(&a,4.01,-6,-18,6,0,1);assert(hash()!=swat);save("native-gain");
 frame(&a,5,-6,-18,6,1,1);assert(a.pose==0&&a.sleeping);assert(hash()!=swat);save("native-sleep");
 frame(&a,6,-36,-18,0,0,1);assert(!a.sleeping&&a.pose==0);
 timings();
 puts("PASS OptoCat native compositor: watching, signal hit, eye gain, curled sleep, opaque bounds and wake");return 0;}
