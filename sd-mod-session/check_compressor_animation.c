#include "src/compressor.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint32_t pixels[320*164];
static unsigned hash(void){unsigned h=2166136261u;for(unsigned i=0;i<320*164;++i){assert((pixels[i]>>24)==255);h=(h^pixels[i])*16777619u;}return h;}
static void save(const char*name){char path[128];snprintf(path,sizeof(path),"build/compressor/art/%s.ppm",name);FILE*f=fopen(path,"wb");assert(f);fprintf(f,"P6\n320 164\n255\n");for(unsigned i=0;i<320*164;++i){unsigned char rgb[]={pixels[i]>>16,pixels[i]>>8,pixels[i]};fwrite(rgb,1,3,f);}assert(!fclose(f));}
int main(void){struct comp_animation a;compressor_animation_init(&a);
 for(unsigned i=0;i<80;++i)compressor_animation_frame(&a,pixels,1+i/30.,-36,-18,0,0,0);
 assert(a.pose==0);unsigned watch=hash();save("native-watch");
 compressor_animation_frame(&a,pixels,4,-6,-18,0,0,1);
 compressor_animation_frame(&a,pixels,4.10,-6,-18,0,0,1);
 compressor_animation_frame(&a,pixels,4.20,-6,-18,0,0,1);
 assert(a.pose>.95f);unsigned swat=hash();assert(watch!=swat);save("native-swat");
 compressor_animation_frame(&a,pixels,4.21,-6,-18,18,0,1);assert(hash()!=swat);save("native-gain");
 compressor_animation_frame(&a,pixels,5,-6,-18,18,1,1);assert(a.pose==0&&a.sleeping);assert(hash()!=swat);save("native-sleep");
 compressor_animation_frame(&a,pixels,6,-36,-18,0,0,1);assert(!a.sleeping&&a.pose==0);
 puts("PASS OptoCat native compositor: watching, signal hit, eye gain, curled sleep, opaque bounds and wake");return 0;}
