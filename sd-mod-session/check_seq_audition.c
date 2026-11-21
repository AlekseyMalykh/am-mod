#include "seq_audition.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define F(p,o,t) (*(t*)((char*)(p)+(o)))
static char prog[16],aprog[16];
static char*pad_inst,*aud_inst;
static int released[8];
static int64_t frames_of(uintptr_t s){return s==0x1000?10000:s==0x2000?3000:s==0x3000?50000:0;}
static void*instr(void*p,unsigned pad){if(p==prog&&pad==5)return pad_inst;if(p==aprog&&pad==0x14)return aud_inst;return NULL;}
static int get(void*inst,uintptr_t out[2]){uintptr_t s=F(inst,0x22e8,uintptr_t)?F(F(inst,0x22e8,char*),0x4a8,uintptr_t):0;if(!s){out[0]=out[1]=0;return 0;}out[0]=s;out[1]=s+1;released[(s)>>12]--;return 1;}
static void rel(uintptr_t c){released[(c-1)>>12]++;}
static int chopped(uintptr_t s){(void)s;return 0;}
static int loop(void*l){return F(l,0x740+0x188+0x50,unsigned)!=0;}
static int rootf(uintptr_t s){return s==0x1000?0:s==0x2000?2:-1;}
static int not_ready;static int readyf(uintptr_t s){(void)s;return !not_ready;}
static uint64_t now=1000;static uint64_t nowf(void){return now;}
static char*mk(void*owner,uintptr_t sample,int64_t f,int64_t l){
 char*i=calloc(1,0x2400),*layers=calloc(8,0xb30);F(i,0x22e8,char*)=layers;F(i,0x22f0,char*)=layers+8*0xb30;F(i,0x2310,void*)=owner;
 F(layers,0x4a8,uintptr_t)=sample;F(layers,0x740+0x78,int64_t)=f;F(layers,0x740+0xd0,int64_t)=l;return i;}
static void set(char*i,uintptr_t s,int64_t f,int64_t l){char*L=F(i,0x22e8,char*);F(L,0x4a8,uintptr_t)=s;F(L,0x740+0x78,int64_t)=f;F(L,0x740+0xd0,int64_t)=l;}
#define L(i) F(i,0x22e8,char*)
int main(void){
 struct seq_audition_ops o={instr,get,rel,frames_of,chopped,loop,nowf,rootf,readyf};seq_audition_set_ops(&o);
 int64_t mf,ml;
 assert(seq_audition_plan(100,900,1000,0,50,60,&mf,&ml)&&mf==0&&ml==50);
 assert(!seq_audition_plan(100,900,800,0,50,60,&mf,&ml));assert(!seq_audition_plan(0,0,10,0,5,6,&mf,&ml));
 pad_inst=mk(prog,0x1000,100,9000);aud_inst=mk(aprog,0,0,0);char msg[64]="";
 assert(seq_audition_toggle(prog,5,aprog,msg,sizeof msg)==1);assert(seq_audition_state()==1);/* waiting for a file */
 assert(F(L(pad_inst),0x4a8,uintptr_t)==0x1000);
 set(aud_inst,0x2000,0,3000);not_ready=1;seq_audition_tick(prog,aprog,1);
 assert(seq_audition_state()==1&&F(L(pad_inst),0x4a8,uintptr_t)==0x1000);/* data still loading: no swap */
 not_ready=0;seq_audition_tick(prog,aprog,1);
 assert(seq_audition_state()==2);assert(F(L(pad_inst),0x4a8,uintptr_t)==0x2000&&F(L(pad_inst),0x740+0x78,int64_t)==0&&F(L(pad_inst),0x740+0xd0,int64_t)==3000);
 set(aud_inst,0x3000,200,40000);seq_audition_tick(prog,aprog,1);/* new file while running */
 assert(F(L(pad_inst),0x4a8,uintptr_t)==0x3000&&F(L(pad_inst),0x740+0x78,int64_t)==200&&F(L(pad_inst),0x740+0xd0,int64_t)==40000);
 seq_audition_save_begin();assert(seq_audition_state()==3);
 assert(F(L(pad_inst),0x4a8,uintptr_t)==0x1000&&F(L(pad_inst),0x740+0x78,int64_t)==100&&F(L(pad_inst),0x740+0xd0,int64_t)==9000);
 seq_audition_tick(prog,aprog,1);assert(F(L(pad_inst),0x4a8,uintptr_t)==0x1000);/* held */
 seq_audition_save_end();seq_audition_tick(prog,aprog,1);assert(seq_audition_state()==2&&F(L(pad_inst),0x4a8,uintptr_t)==0x3000);
 /* candidate refs are only freed after the grace period */
 now+=31000;seq_audition_tick(prog,aprog,1);
 seq_audition_off("test");assert(F(L(pad_inst),0x4a8,uintptr_t)==0x1000&&F(L(pad_inst),0x740+0xd0,int64_t)==9000&&!seq_audition_active());
 now+=31000;seq_audition_tick(prog,aprog,1);
 {int held=0;for(int i=0;i<8;i++){assert(released[i]<=0);held-=released[i];}assert(held>=1&&held<=4);}/* newest retired candidates stay referenced, the rest were returned */
 /* conflict: the pad was reloaded while swapped -> nothing is written back */
 assert(seq_audition_toggle(prog,5,aprog,msg,sizeof msg)==1);assert(F(L(pad_inst),0x4a8,uintptr_t)==0x3000);
 set(pad_inst,0x2000,0,2500);seq_audition_off("load");assert(F(L(pad_inst),0x4a8,uintptr_t)==0x2000&&F(L(pad_inst),0x740+0xd0,int64_t)==2500);
 /* stale program: no write */
 set(pad_inst,0x1000,100,9000);assert(seq_audition_toggle(prog,5,aprog,msg,sizeof msg)==1);
 seq_audition_tick((void*)&msg,aprog,1);assert(!seq_audition_active());assert(F(L(pad_inst),0x4a8,uintptr_t)==0x3000);
 /* browser exit restores */
 set(pad_inst,0x1000,100,9000);assert(seq_audition_toggle(prog,5,aprog,msg,sizeof msg)==1);seq_audition_tick(prog,aprog,0);
 assert(F(L(pad_inst),0x4a8,uintptr_t)==0x1000&&!seq_audition_active());
 /* Orig Note: pad C, file D -> -2 semitones; K1 sets another file note */
 set(pad_inst,0x1000,100,9000);set(aud_inst,0x2000,0,3000);
 assert(seq_audition_toggle(prog,5,aprog,msg,sizeof msg)==1);assert(F(L(pad_inst),0x60,signed char)==-2);
 assert(seq_audition_knob(80)==1);assert(F(L(pad_inst),0x60,signed char)==5);/* G over C: +5 */
 seq_audition_knob(0);assert(F(L(pad_inst),0x60,signed char)==0);/* "--": no compensation */
 set(aud_inst,0x3000,0,40000);seq_audition_tick(prog,aprog,1);assert(F(L(pad_inst),0x60,signed char)==0);
 seq_audition_off("t");assert(F(L(pad_inst),0x60,signed char)==0&&F(L(pad_inst),0x4a8,uintptr_t)==0x1000);
 set(aud_inst,0x2000,0,3000);assert(seq_audition_toggle(prog,5,aprog,msg,sizeof msg)==1);
 seq_audition_save_begin();assert(F(L(pad_inst),0x60,signed char)==0);seq_audition_save_end();
 seq_audition_tick(prog,aprog,1);assert(F(L(pad_inst),0x60,signed char)==-2);
 {char info[64];seq_audition_info(info,sizeof info);assert(strstr(info,"file D")&&strstr(info,"pad C")&&strstr(info,"-2 st"));}
 seq_audition_off("t");assert(F(L(pad_inst),0x60,signed char)==0);
 /* Preview and swap are coordinated; the latest loaded file resumes it. */
 set(pad_inst,0x1000,100,9000);set(aud_inst,0x2000,0,3000);
 assert(seq_audition_toggle(prog,5,aprog,msg,sizeof msg)==1);
 seq_audition_preview(0);assert(seq_audition_state()==4&&F(L(pad_inst),0x4a8,uintptr_t)==0x1000&&F(L(pad_inst),0x60,signed char)==0);
 set(aud_inst,0x3000,0,40000);seq_audition_tick(prog,aprog,1);assert(F(L(pad_inst),0x4a8,uintptr_t)==0x1000);
 seq_audition_preview(1);seq_audition_tick(prog,aprog,1);assert(F(L(pad_inst),0x4a8,uintptr_t)==0x3000);
 struct seq_audition_view v;assert(seq_audition_view(&v)&&v.state==2&&v.pad==5);
 seq_audition_save_begin();seq_audition_save_begin();seq_audition_save_end();seq_audition_tick(prog,aprog,1);assert(seq_audition_state()==3);
 seq_audition_save_end();seq_audition_tick(prog,aprog,1);assert(seq_audition_state()==2);
 seq_audition_off("load selected file");assert(F(L(pad_inst),0x4a8,uintptr_t)==0x1000);
 set(pad_inst,0x3000,0,40000);seq_audition_tick(prog,aprog,1);assert(F(L(pad_inst),0x4a8,uintptr_t)==0x3000&&!seq_audition_active());
 /* More than eight retired candidates must never be evicted early. */
 now+=31000;seq_audition_tick(prog,aprog,1);set(pad_inst,0x1000,100,9000);set(aud_inst,0x2000,0,3000);
 assert(seq_audition_toggle(prog,5,aprog,msg,sizeof msg)==1);
 for(unsigned n=0;n<40;++n){set(aud_inst,n%2?0x2000:0x3000,0,n%2?3000:40000);seq_audition_tick(prog,aprog,1);}
 assert(released[2]+released[3]<-30);seq_audition_off("rapid scroll");now+=31000;seq_audition_tick(prog,aprog,1);
 {int held=0;for(unsigned n=0;n<8;++n){assert(released[n]<=0);held-=released[n];}assert(held>=1&&held<=4);}/* at most four newest remain */
 puts("PASS rapid 40-file scrolling retains all old candidates until grace expires; Preview Off restores, fresh load resumes, nested save and permanent load preserve source/tune");
 /* refusals */
 assert(seq_audition_toggle(prog,6,aprog,msg,sizeof msg)==-1&&!strcmp(msg,"Pad not ready"));
 F(L(pad_inst),0x740+0x188+0x50,unsigned)=1;assert(seq_audition_toggle(prog,5,aprog,msg,sizeof msg)==-1&&strstr(msg,"Looped"));
 free(L(pad_inst));free(pad_inst);free(L(aud_inst));free(aud_inst);puts("seq_audition ok");return 0;
}
