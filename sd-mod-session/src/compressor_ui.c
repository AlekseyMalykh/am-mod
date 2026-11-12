#define _GNU_SOURCE
#include "shift_badge.h"
#include "compressor.h"
#include "phone_events.h"
#include "music_ui.h"
#ifdef MPC_CHORDS
#include "takeover_native.h"
#endif
#include <math.h>
#include <stdatomic.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>
#include <time.h>
#define F(p,o,T) (*(T*)((char*)(p)+(o)))
#define FN(o,T) ((T)(base+(o)))
#define SCREEN 0x6e1bf30
static uintptr_t base;
static void*root;
static _Atomic unsigned active,owned[112];
static _Atomic uint64_t epoch=1;
static unsigned painted_type=~0u;
static unsigned repeat_seen,repeat_previous,header_dirty;
static int knob_focus=-1;
static struct comp_animation animation;
static double next_frame;
struct knob_anchor {unsigned valid,physical,picked;float parameter;};
static struct knob_anchor anchors[3];
static uint64_t knob_snapshot;
static unsigned knob_layer,knob_mode;
static void reset_knob_anchors(void)
{
 for(unsigned i=0;i<3;++i)anchors[i].valid=0;
}
static float knob_position(const struct comp_config *c,unsigned k,unsigned shifted)
{
 if(shifted){
  if(k==0)return logf(c->attack)/logf(100.f)*127.f;
  if(k==1)return logf(c->release/100.f)/logf(40.f)*127.f;
  return c->knee/12.f*127.f;
 }
 if(k==0)return (c->threshold+48.f)/48.f*127.f;
 if(k==1)return (c->ratio-1.f)/7.f*127.f;
 return (c->gain+6.f)/12.f*127.f;
}
enum { TAKEOVER_PICKUP,TAKEOVER_SCALED,TAKEOVER_JUMP };
static unsigned parameter_takeover(void)
{
 /* AC50 -> AudioIO -> core -> AC50SoftTakeoverProperties. The stock
  * property serializer reads Parameter at +0x1c and Knob FX at +0x3c. */
 void *app=F((void*)base,0x71e3dc8,void*);
 if(!app||F(app,0,uintptr_t)!=base+0x6e07288)return TAKEOVER_JUMP;
 void *audio=F(app,0xd8,void*);
 if(!audio||F(audio,0,uintptr_t)!=base+0x6e0c430)return TAKEOVER_JUMP;
 void *core=F(audio,0x148,void*);
 if(!core||F(core,0,uintptr_t)!=base+0x6e0cf38)return TAKEOVER_JUMP;
 void *properties=F(core,0x5a0,void*);
 if(!properties)return TAKEOVER_JUMP;
 unsigned mode=F(properties,0x1c,unsigned);
 return mode<=TAKEOVER_JUMP?mode:TAKEOVER_JUMP;
}
#define T_SHIFT (UINT64_C(1)<<63)
#define T_REPEAT (UINT64_C(1)<<62)
static double seconds(void){struct timespec t;if(clock_gettime(CLOCK_MONOTONIC,&t))return 0;return t.tv_sec+t.tv_nsec*1e-9;}
void compressor_ui_init(uintptr_t b){base=b;}
/* Capture Shift with each queued movement. A later release must not redirect
 * an already queued Attack movement to Threshold, or merge both sweeps. */
uint64_t compressor_ui_ticket(void){return atomic_load(&epoch)|(music_ui_shift_active()?T_SHIFT:0)|(music_ui_repeat_active()?T_REPEAT:0);}
static int current_ticket(uint64_t ticket){return (ticket&~(T_SHIFT|T_REPEAT))==atomic_load(&epoch);}
static int visible(void){return root&&FN(0x112fd50,int(*)(void*))(root)&&!FN(0x112f6b0,int(*)(void*))(root);}
static void repaint(void){if(root)FN(0x1149880,void(*)(void*,uint64_t,uint64_t))(root,0,F(root,0x28,uint64_t));}
static void colour(void*g,uint32_t v){FN(0x102d5e0,void(*)(void*,uint32_t))(g,v);}
static void rect(void*g,int x,int y,int w,int h){FN(0x102e000,void(*)(void*,int,int,int,int))(g,x,y,w,h);}
static void text(void*g,const char*s,int x,int y,int w,int h,float size){
 uint64_t str=0,font=0;FN(0xf535c0,void(*)(uint64_t*,const char*))(&str,s);
 FN(0x1082810,void(*)(uint64_t*,int,float))(&font,0,size);FN(0x102dca0,void(*)(void*,const uint64_t*))(g,&font);
 FN(0x10817b0,void(*)(void*,const uint64_t*,int,int,int,int,int,int,float))(g,&str,x,y,w,h,0x24,1,.7f);
 FN(0x1070d60,void(*)(uint64_t*))(&font);FN(0xf0a290,void(*)(uint64_t*))(&str);
}
static void shift_badge(void*g,int x,int held){
 /* Same 10x12 badge and ink as music_ui.c/native K panels. */
 colour(g,held?0xffff4747:0xffffffff);
 SHIFT_BADGE_RECTS(rect,g,x,200);
}
static void bind_master(void){
 if(!root)return;
 void*meter=F(root,0x2698,void*);
 if(meter&&F(meter,0,uintptr_t)==base+0x6d8fac0)
  compressor_native_bind(base,F(meter,0x80,void*));
}
int compressor_ui_tick(void){
 bind_master();
 unsigned show=(unsigned)visible(),repeat=(unsigned)music_ui_repeat_active();
 if(repeat_seen&&repeat_previous&&!repeat)header_dirty=1;
 repeat_seen=1;repeat_previous=repeat;
 if(show&&!repeat&&header_dirty){header_dirty=0;repaint();}
 if(atomic_exchange(&active,show)!=show){atomic_fetch_add(&epoch,1);knob_focus=-1;reset_knob_anchors();
#ifdef MPC_CHORDS
  /* Opto owns K1-K3, so the native bindings never saw those turns. On the
   * way out let the next Scaled binding pick up instead of jumping. */
  if(!show)takeover_native_context_changed();
#endif
 }
 if(show&&painted_type!=compressor_type()){painted_type=compressor_type();reset_knob_anchors();repaint();}
 double now=seconds();
 if(show&&compressor_type()==COMP_OPTO&&now>=next_frame){next_frame=now+1./30;repaint();}
 return show!=0;
}
int compressor_ui_active(void){return atomic_load(&active)!=0;}
int compressor_ui_owns(unsigned kind,unsigned code){
 if(kind==PHONE_BUTTON&&code<112&&atomic_load(&owned[code]))return 1;
 if(!atomic_load(&active))return 0;
 if(kind==PHONE_BUTTON&&code>=0x0c&&code<=0x0e&&music_ui_repeat_active())return 0;
 if(kind==PHONE_BUTTON)return code==0x0d||(compressor_type()==COMP_OPTO&&code==0x0c);
 return kind==PHONE_CONTROLLER&&compressor_type()==COMP_OPTO&&code>=0x10&&code<=0x12;
}
int compressor_ui_button(void*e,uint64_t ticket){
 unsigned code=F(e,8,unsigned char),down=F(e,9,unsigned char)!=0;
 if(code>=112)return 0;
 if(atomic_load(&owned[code])){if(!down)atomic_store(&owned[code],0);return 1;}
 if((ticket&T_REPEAT)&&code>=0x0c&&code<=0x0e)return 0;
 if(!current_ticket(ticket)||!visible())return 1; /* Never replay a stale Type press in another screen. */
 if(!down)return 0;
 if(code==0x0d){compressor_select(compressor_type()==COMP_DUCK?COMP_OPTO:COMP_DUCK);reset_knob_anchors();repaint();}
 else if(compressor_type()==COMP_OPTO&&code==0x0c){compressor_warm();repaint();}
 else return 0;
 atomic_store(&owned[code],1);return 1;
}
int compressor_ui_control(void*e,uint64_t ticket){
 if(!current_ticket(ticket)||!visible()||compressor_type()!=COMP_OPTO)return 1;
 unsigned cc=F(e,8,unsigned char),value=F(e,9,unsigned char);
 if(cc<0x10||cc>0x12)return 0;
 if(value>127)return 1;
 unsigned k=cc-0x10,layer=(ticket&T_SHIFT)!=0;
 knob_focus=(int)k;
 unsigned mode=parameter_takeover(),scaled=value;
 uint64_t snapshot=compressor_project_snapshot();
 if(layer!=knob_layer||mode!=knob_mode||snapshot!=knob_snapshot){
  reset_knob_anchors();knob_layer=layer;knob_mode=mode;knob_snapshot=snapshot;
 }
 if(mode!=TAKEOVER_JUMP){
  struct knob_anchor *anchor=anchors+k;
  if(!anchor->valid){
   struct comp_config c;
   compressor_config(&c);
   *anchor=(struct knob_anchor){1,value,0,knob_position(&c,k,layer)};
   if(mode==TAKEOVER_SCALED||value!=(unsigned)lroundf(anchor->parameter)){repaint();return 1;}
   anchor->picked=1;
  }
  unsigned previous=anchor->physical;
  anchor->physical=value;
  if(mode==TAKEOVER_PICKUP){
   if(!anchor->picked){
    unsigned target=(unsigned)lroundf(anchor->parameter);
    if((previous<target&&value<target)||(previous>target&&value>target))return 1;
    anchor->picked=1;
   }
  }else{
   if(value==previous)return 1;
   /* Native Scaled uses the remaining travel in the current direction
    * and keeps a minimum slope of .15 when reversing near an endpoint. */
   float span=value>previous?127.f-previous:(float)previous;
   float room=value>previous?127.f-anchor->parameter:anchor->parameter;
   anchor->parameter=fmaxf(0,fminf(127,anchor->parameter+
        ((float)value-previous)*fmaxf(.15f,room/span)));
   scaled=(unsigned)lroundf(anchor->parameter);
  }
 }
 if(layer)compressor_shift_knob(k,scaled);else compressor_knob(k,scaled);
 knob_snapshot=compressor_project_snapshot();
 repaint();return 1;
}
int compressor_ui_reset_knob(unsigned column,unsigned shifted){
 if(!visible()||music_ui_repeat_active()||compressor_type()!=COMP_OPTO)return 0;
 if(column>2)return 1;
 knob_focus=(int)column;compressor_reset_knob(column,shifted);reset_knob_anchors();repaint();return 1;
}
static void paint(void*p,void*g){
 if(F(p,0,uintptr_t)!=base+SCREEN)return;
 compressor_phone_wake();
 if(root!=p){root=p;reset_knob_anchors();compressor_animation_init(&animation);atomic_fetch_add(&epoch,1);}
 compressor_ui_tick();if(!visible()||music_ui_repeat_active())return;
 unsigned type=compressor_type();int ready=compressor_ready();
 colour(g,0xff202020);rect(g,107,1,105,28);
 colour(g,ready?0xffffc247:0xff707070);text(g,"Type",110,1,98,22,14);
 for(unsigned i=0;i<COMP_TYPES;++i){colour(g,i==type?0xffffc247:0xff606060);rect(g,153+(int)i*7,25,5,i==type?3:1);}
 if(!ready){colour(g,0xffffffff);text(g,"OptoCat not ready",106,31,108,15,11);return;}
 if(type!=COMP_OPTO)return;
 struct comp_config c;compressor_config(&c);char value[64];
 colour(g,0xff000000);rect(g,0,34,320,164);
 colour(g,0xff202020);rect(g,1,1,105,28);rect(g,215,1,105,28);
 colour(g,c.warm?0xffffc247:0xffffffff);text(g,"Warm",4,1,98,22,14);
 if(c.warm)rect(g,8,25,90,1);
 colour(g,c.bypass?0xffff4040:0xffffffff);text(g,"Bypass",217,1,99,22,14);
 /* Match the native single-option header: x+8, y25, width-16, one pixel. */
 if(c.bypass)rect(g,221,25,91,1);
 uint32_t*pixels=compressor_image_pixels(base);
 float gr=c.bypass?0:compressor_reduction();
 if(pixels){
  /* Native Duck hides activity while bypassed/asleep. Clear the displayed
   * snapshot immediately, including the last GR before the audio fade ends.
   * Keep real input/output telemetry intact for wake-up and diagnostics. */
  struct comp_meter meter={0};if(!c.bypass)compressor_meter_levels(&meter);
  animation.reduction=gr; /* One snapshot for paw, red bar and numeric GR. */
  compressor_animation_frame(&animation,pixels,seconds(),compressor_input_level(),&c,compressor_hits());
  compressor_meter_frame(pixels,&meter,gr);
  colour(g,0xffffffff);compressor_image_draw(g);
 }
 colour(g,0xffb0b0b0);text(g,"OptoCat",4,35,80,14,11);
 snprintf(value,sizeof(value),"GR %.1f dB",gr);text(g,value,4,180,92,16,11);
 int shifted=music_ui_shift_active();
 if(knob_focus>=0&&knob_mode==TAKEOVER_PICKUP&&knob_layer==(unsigned)shifted&&
    anchors[knob_focus].valid&&!anchors[knob_focus].picked){
  colour(g,0xffffc247);text(g,"Turn knob to pick up value",108,180,204,16,11);
 }
 const char*normal_names[]={"Threshold","Ratio","Gain"},*shift_names[]={"Attack","Release","Knee"};
 const char**names=shifted?shift_names:normal_names;
 for(int k=0;k<3;++k){int x=k==0?0:k==1?106:213,w=k==2?107:106;
  int focused=k==knob_focus;
  uint32_t ink=focused?0xffffc247u:0xffffffffu;
  colour(g,0xff000000);rect(g,x,198,w,40);
  colour(g,focused?0xff302412u:0xff202020u);rect(g,x+2,198,w-4,38);
  colour(g,ink);if(focused)rect(g,x+2,198,w-4,1);
  text(g,names[k],x+4,200,w-22,12,12);
  shift_badge(g,k*106+94,shifted);
  if(shifted&&k==0)snprintf(value,sizeof(value),"%.2f ms",c.attack);
  else if(shifted&&k==1)snprintf(value,sizeof(value),"%.0f ms",c.release);
  else if(shifted)snprintf(value,sizeof(value),"%.1f dB",c.knee);
  else if(k==0)snprintf(value,sizeof(value),"%.1f dB",c.threshold);
  else if(k==1)snprintf(value,sizeof(value),"%.1f:1",c.ratio);
  else snprintf(value,sizeof(value),"%+.1f dB",c.gain);
  colour(g,ink);text(g,value,x+4,214,w-8,19,16);
 }
}
static void destroyed(void*p){if(root==p){root=0;atomic_store(&active,0);atomic_fetch_add(&epoch,1);}FN(0x15f8dd0,void(*)(void*))(p);}
static void deleted(void*p){if(root==p){root=0;atomic_store(&active,0);atomic_fetch_add(&epoch,1);}FN(0x15f9a80,void(*)(void*))(p);}
int compressor_ui_install(void){
 if(!base||sysconf(_SC_PAGESIZE)!=4096)return 0;
 uintptr_t*vt=(uintptr_t*)(base+SCREEN);
 if(vt[0]!=base+0x15f8dd0||vt[1]!=base+0x15f9a80||vt[27]!=base+0x10e2e10)return 0;
 void*page=(void*)((base+SCREEN)&~(uintptr_t)4095);
 if(mprotect(page,8192,PROT_READ|PROT_WRITE))return 0;
 vt[0]=(uintptr_t)destroyed;vt[1]=(uintptr_t)deleted;vt[27]=(uintptr_t)paint;
 if(mprotect(page,8192,PROT_READ))_exit(121);
 return 1;
}
