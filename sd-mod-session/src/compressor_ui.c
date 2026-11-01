#define _GNU_SOURCE
#include "compressor.h"
#include "phone_events.h"
#include <stdatomic.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>
#define F(p,o,T) (*(T*)((char*)(p)+(o)))
#define FN(o,T) ((T)(base+(o)))
#define SCREEN 0x6e1bf30
static uintptr_t base;
static void*root;
static _Atomic unsigned active,owned[112];
static _Atomic uint64_t epoch=1;
static unsigned painted_type=~0u;
static int reported;
void compressor_ui_init(uintptr_t b){base=b;}
uint64_t compressor_ui_ticket(void){return atomic_load(&epoch);}
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
int compressor_ui_tick(void){
 unsigned show=(unsigned)visible();
 if(atomic_exchange(&active,show)!=show)atomic_fetch_add(&epoch,1);
 if(show&&painted_type!=compressor_type()){painted_type=compressor_type();repaint();}
 if(show&&compressor_type()==COMP_OPTO)repaint();
 return show!=0;
}
int compressor_ui_owns(unsigned kind,unsigned code){
 if(kind==PHONE_BUTTON&&code<112&&atomic_load(&owned[code]))return 1;
 if(!atomic_load(&active))return 0;
 if(kind==PHONE_BUTTON)return code==0x0d||(compressor_type()==COMP_OPTO&&(code==0x0c||code==0x0e));
 return kind==PHONE_CONTROLLER&&compressor_type()==COMP_OPTO&&code>=0x10&&code<=0x12;
}
int compressor_ui_button(void*e,uint64_t ticket){
 unsigned code=F(e,8,unsigned char),down=F(e,9,unsigned char)!=0;
 if(code>=112)return 0;
 if(atomic_load(&owned[code])){if(!down)atomic_store(&owned[code],0);return 1;}
 if(ticket!=compressor_ui_ticket()||!visible())return 1; /* Never replay a stale Type press in another screen. */
 if(!down)return 0;
 if(code==0x0d){compressor_select(compressor_type()==COMP_DUCK?COMP_OPTO:COMP_DUCK);repaint();}
 else if(compressor_type()==COMP_OPTO&&code==0x0e){compressor_bypass();repaint();}
 else if(compressor_type()==COMP_OPTO&&code==0x0c){/* Native Color belongs to Duck; no misleading Opto coloration. */}
 else return 0;
 atomic_store(&owned[code],1);return 1;
}
int compressor_ui_control(void*e,uint64_t ticket){
 if(ticket!=compressor_ui_ticket()||!visible()||compressor_type()!=COMP_OPTO)return 1;
 unsigned cc=F(e,8,unsigned char),value=F(e,9,unsigned char);
 if(cc<0x10||cc>0x12)return 0;
 compressor_knob(cc-0x10,value);repaint();return 1;
}
static void paint(void*p,void*g){
 if(F(p,0,uintptr_t)!=base+SCREEN)return;
 compressor_phone_wake();
 if(root!=p){root=p;atomic_fetch_add(&epoch,1);}
 if(!compressor_ready()){
  /* Native 15edf90 creates the meter and stores it at screen+2698
   * (15ee064..68). Its +80 is the exact master Colour instance.
   * The fields at +100 are unrelated services. */
  void*meter=F(p,0x2698,void*);
   if(meter&&F(meter,0,uintptr_t)==base+0x6d8fac0){
    if(compressor_native_bind(base,F(meter,0x80,void*))&&!reported){fprintf(stderr,"COMPRESSOR MASTER_BOUND\n");reported=1;}
   }
 }
 compressor_ui_tick();if(!visible())return;
 unsigned type=compressor_type();int ready=compressor_ready();
 colour(g,0xff000000);rect(g,106,0,106,29);
 colour(g,ready?0xffffc247:0xff707070);text(g,"Type",110,1,98,22,14);
 for(unsigned i=0;i<COMP_TYPES;++i){colour(g,i==type?0xffffc247:0xff606060);rect(g,153+(int)i*7,25,5,i==type?3:1);}
 if(!ready){colour(g,0xffffffff);text(g,"OptoCat not ready",106,31,108,15,11);}
 if(type!=COMP_OPTO)return;
 struct comp_config c;compressor_config(&c);char value[64];
 colour(g,0xff000000);rect(g,0,34,320,164);rect(g,0,0,106,29);rect(g,213,0,107,29);
 colour(g,0xff707070);text(g,"--",4,1,98,22,14);
 colour(g,c.bypass?0xffffc247:0xffffffff);text(g,"Bypass",217,1,99,22,14);
 colour(g,0xffffffff);text(g,"OptoCat",20,64,280,34,26);text(g,"Optical leveler",20,106,280,22,15);
 snprintf(value,sizeof(value),"Reduction %.1f dB",compressor_reduction());text(g,value,20,147,280,22,15);
 const char*names[]={"Threshold","Ratio","Gain"};
 for(int k=0;k<3;++k){int x=k==0?0:k==1?106:213,w=k==2?107:106;
  colour(g,0xff000000);rect(g,x,198,w,38);colour(g,0xffffc247);text(g,names[k],x+2,199,w-4,15,12);
  if(k==0)snprintf(value,sizeof(value),"%.1f dB",c.threshold);
  else if(k==1)snprintf(value,sizeof(value),"%.1f:1",c.ratio);
  else snprintf(value,sizeof(value),"%+.1f dB",c.gain);
  colour(g,0xffffffff);text(g,value,x+2,215,w-4,19,15);
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
