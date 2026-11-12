#include "knob_reset.h"
#include <stdatomic.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#define F(p,o,T) (*(T*)((char*)(p)+(o)))
#define FN(o,T) ((T)(image+(o)))
static uintptr_t image;
static _Atomic uint64_t modifiers;
static void*sample_root;
static uint64_t consumed_gesture;static unsigned consumed_mask;
void knob_reset_init(uintptr_t b){image=b;}
void knob_reset_button(unsigned code,unsigned down){
 uint64_t v=atomic_load(&modifiers);
 if(code==0x23){ /* Physical PAD BANK: GlobalControls+0x2300; both edges remain native. */
  if(down&&!(v&2))v=((v+4)&~UINT64_C(3))|(v&1)|2;
  else if(!down)v&=~UINT64_C(2);
 }else if(code==0x31)v=(v&~UINT64_C(1))|!!down;
 else return;
 atomic_store(&modifiers,v);
}
uint64_t knob_reset_ticket(void){return atomic_load(&modifiers);}
int knob_reset_claim(unsigned cc){return cc>=0x10&&cc<=0x12&&(knob_reset_ticket()&2);}
int knob_reset_begin(uint64_t ticket,unsigned cc){
 if(!(ticket&2)||cc<0x10||cc>0x12)return 0;
 uint64_t gesture=ticket>>2;
 if(gesture!=consumed_gesture){consumed_gesture=gesture;consumed_mask=0;}
 unsigned bit=1u<<(cc-0x10+((ticket&1)?3:0));
 if(consumed_mask&bit)return 0;
 consumed_mask|=bit;return 1;
}
void knob_reset_observe(void*p){
 if(!image||!p||F(p,0,uintptr_t)!=image+0x706e5d0||F(p,0x24,int)!=198||F(p,0x2c,int)!=38)return;
 int x=F(p,0x20,int),w=F(p,0x28,int);if(x<0||x>214||w<100||w>107)return;
 void*r=p;
 for(unsigned depth=0;r&&depth<32;++depth,r=F(r,0x18,void*))
  if(F(r,0,uintptr_t)==image+0x6e212a0){sample_root=r;break;}
}
void knob_reset_forget(void*screen){
 if(sample_root==screen)sample_root=NULL;
}
/* Fresh native Sample project instrument defaults, mapped to the exact UI
 * controls. These are values, not physical-knob positions or session history.
 * A generic Slider's double-click value is unset on the MPC, so zero is NOT
 * a valid universal fallback (End, Pan, Cutoff and Volume all differ). */
static int default_value(uintptr_t root,unsigned offset,double*out){
 if(root==0x6e212a0){
  static const struct {unsigned offset;double value;} values[]={
   {0x2bb8,0},{0x3258,100},{0x38f8,0}, /* Trim */
   {0x2f08,0},{0x35a8,0},{0x3c48,0}, /* Zoom */
   {0x3f98,0},{0x42e8,50}, /* Chop type / threshold */
   {0x4638,.7079457640647888},{0x4988,.5},{0x4cd8,.7079457640647888},
   {0x5028,2},{0x5378,6},{0x56c8,0},{0x5a18,1},{0x5d68,127},
   {0x60b8,0},{0x6408,0},{0x6758,49},{0x6aa8,0}, /* Tune; Warp Off */
   {0x6df8,1},{0x7148,0},{0x7498,0},{0x77e8,0},
   {0x7b38,127},{0x7e88,0},{0x81d8,0},
   {0x8528,2},{0x8878,6},{0x8bc8,0},{0x8f18,1},{0x9268,0}
  };
  for(unsigned i=0;i<sizeof(values)/sizeof(*values);++i)if(values[i].offset==offset){*out=values[i].value;return 1;}
 }
 return 0;
}
int knob_reset_native(unsigned column,unsigned shifted){
 (void)shifted;if(column>2||!image||!sample_root)return 0;
 void*root=sample_root;
 if(F(root,0,uintptr_t)!=image+0x6e212a0||!FN(0x112fd50,int(*)(void*))(root)||FN(0x112f6b0,int(*)(void*))(root))return 0;
 void*p=NULL;double value=0;
 /* Resolve the presently visible footer after queued Shift has reached native
  * controls. Never use a previous paint's hidden alternate or a stale pointer. */
 for(unsigned off=0x2bb8;off<=0x9268;off+=0x350){
  void*c=(char*)root+off;
  if(F(c,0,uintptr_t)!=image+0x706e5d0||F(c,0x24,int)!=198||
     !FN(0x112fd50,int(*)(void*))(c)||!FN(0x10f98d0,int(*)(void*))(c)||FN(0x112f6b0,int(*)(void*))(c))continue;
  int x=F(c,0x20,int),w=F(c,0x28,int);
  if(x<0||w<100||w>107||(unsigned)(x+w/2)*3/320!=column)continue;
  if(default_value(0x6e212a0,off,&value)){p=c;break;}
 }
 if(!p)return 0;
 void*impl=F(p,0x160,void*);if(!impl)return 0;
 double lo=F(impl,0x98,double),hi=F(impl,0xa0,double);
 if(!isfinite(lo)||!isfinite(hi)||value<lo||value>hi)return 0;
 /* JUCE Slider::setValue(double, sendNotificationSync). The native Value
  * binding owns parameter mutation, notifications and history. */
 enum { SEND_NOTIFICATION_SYNC=2 };
 FN(0x11c1d60,void(*)(void*,double,int))(p,value,SEND_NOTIFICATION_SYNC);
 fprintf(stderr,"KNOB_RESET NATIVE column=%u value=%.6g\n",column,value);return 1;
}
