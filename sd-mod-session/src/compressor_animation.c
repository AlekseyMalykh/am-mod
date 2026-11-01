#include "compressor.h"
#include "optocat_atlas.inc"
#include <math.h>
#include <string.h>
static uint32_t background[320*164];
static int background_ready;
static unsigned random_state=12743;
static float random_value(void){random_state=random_state*1664525u+1013904223u;return (float)((double)random_state/4294967296.);}
static float clamp(float x,float a,float b){return fminf(b,fmaxf(a,x));}
static void put(uint32_t*p,int x,int y,unsigned grey){if(x>=0&&x<320&&y>=0&&y<164)p[y*320+x]=0xff000000u|grey*0x010101u;}
static void line(uint32_t*p,float x0,float y0,float x1,float y1,unsigned v){
 int n=(int)ceilf(fmaxf(fabsf(x1-x0),fabsf(y1-y0)));if(n<1)n=1;
 for(int i=0;i<=n;++i){float t=(float)i/n;put(p,(int)lroundf(x0+(x1-x0)*t),(int)lroundf(y0+(y1-y0)*t),v);}
}
static void sprite(uint32_t*p,unsigned frame,int x,int y,int w,int h){
 unsigned pixel=0,limit=(unsigned)(w*h);
 for(unsigned i=optocat_offsets[frame];i<optocat_offsets[frame+1];++i){
  unsigned packed=optocat_atlas[i],n=(packed>>4)+1,shade=packed&15;
  unsigned grey=shade?(shade-1)*255/14:0;
  while(n--&&pixel<limit){if(shade)put(p,x+(int)(pixel%(unsigned)w),y+(int)(pixel/(unsigned)w),grey);++pixel;}
 }
}
void compressor_animation_init(struct comp_animation*s){memset(s,0,sizeof(*s));s->level=-60;s->hit_time=-10;}
void compressor_animation_frame(struct comp_animation*s,uint32_t*p,double now,float input,float threshold,float gain,unsigned bypass,unsigned hit){
 if(!s||!p||!isfinite(now))return;
 if(!background_ready){
  for(unsigned i=0;i<320*164;++i)background[i]=0xff000000;
  for(int y=8;y<164;++y)for(int x=35;x<288;++x){float r=hypotf((x-160)/125.f,(y-98)/80.f);
   if(r<1&&random_value()<(1-r)*.48f){unsigned v=(unsigned)lroundf(23+random_value()*32);background[y*320+x]=0xff000000u|v*0x010101u;}}
  background_ready=1;
 }
 memcpy(p,background,sizeof(background));
 float dt=s->time>0?(float)clamp((float)(now-s->time),0,.10f):.033f;s->time=now;
 input=isfinite(input)?clamp(input,-60,0):-60;threshold=clamp(threshold,-48,0);gain=clamp(gain,0,18);
 if(bypass){s->pose=0;s->last_hit=hit;s->hit_time=-10;s->sleeping=1;
  sprite(p,OPTOCAT_SLEEP,96,70+(sin(now*2.2)>.65?1:0),154,90);return;}
 if(s->sleeping){s->sleeping=0;s->level=input;s->last_hit=hit;s->hit_time=-10;}
 s->level+=(input-s->level)*(1-expf(-dt/.065f));
 if(hit!=s->last_hit){s->last_hit=hit;s->hit_time=now;}
 float age=(float)(now-s->hit_time),swat=age<0?0:age<.11f?sinf(age/.11f*1.57079633f):age<.24f?1:age<.43f?1-(age-.24f)/.19f:0;
 float target=fmaxf(s->level>threshold?.30f:0,swat);s->pose+=(target-s->pose)*(1-expf(-dt/.025f));
 unsigned reach=(unsigned)lroundf(clamp(s->pose,0,1)*8),look=(unsigned)lroundf(clamp((s->level+48)/48,0,1)*6),eye=(unsigned)lroundf(gain/6);
 float by=154-clamp((s->level+48)/48,0,1)*71,th=154-(threshold+48)/48*71;
 for(int x=183;x<240;x+=5)line(p,(float)x,th,(float)x+1,th,75);
 line(p,238,th-3,242,th,150);line(p,242,th,238,th+3,150);
 sprite(p,(eye*7+look)*9+reach,96,18,OPTOCAT_SPRITE_W,OPTOCAT_SPRITE_H);
 float impact=age>=.10f&&age<.43f?sinf((age-.10f)/.33f*3.14159265f):0,bx=194+impact*4;
 float px=207,py=19;
 for(int i=1;i<=16;++i){float t=(float)i/16,u=1-t,x=u*u*207+2*u*t*(201+impact*8)+t*t*bx,y=u*u*19+2*u*t*((19+by)/2)+t*t*by;line(p,px,py,x,y,135);px=x;py=y;}
 sprite(p,OPTOCAT_BOW,(int)lroundf(bx)-14,(int)lroundf(by)-10,29,25);
 if(impact>.3f){line(p,200,by-13,204,by-18,170);line(p,207,by-3,213,by-4,150);}
}
