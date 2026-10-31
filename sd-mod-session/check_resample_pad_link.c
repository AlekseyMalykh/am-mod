/* Execute the pinned native DrumProgram noteOn and Instrument Pad Link
 * getters. Voice allocation/DSP is replaced by a receiver, not simulated
 * Pad Link expansion. No SD files or user project are opened. */
#define main unused_fx_main
#include "check_fx_native.c"
#undef main
#define F(p,o,T) (*(T*)((char*)(p)+(o)))
static unsigned char program[0x3000],instrument[6][0x2400],data[44];
static void*instruments[128];static uint64_t activity[2],pending[2];
static unsigned seen[6],calls;
static void patch(uintptr_t at,void*fn){uint32_t code[]={0x58000050,0xd61f0200};memcpy((void*)(base+at),code,8);memcpy((void*)(base+at+8),&fn,8);__builtin___clear_cache((char*)(base+at),(char*)(base+at+16));}
static void control(void*p,void*i,void*t,unsigned on){assert(p==program&&i&&t==data&&!on);}
static void voices(void*p,void*time,void*mix,void*i,void*event,unsigned mode,float a,float b,float c){
 (void)time;(void)mix;(void)event;(void)mode;(void)a;(void)b;(void)c;
 assert(p==program);unsigned n=(unsigned)(((unsigned char*)i-instrument[0])/sizeof(instrument[0]));assert(n<6);++seen[n];++calls;
}
static void ignored(void*p,void*i,void*t,unsigned on){(void)p;(void)i;(void)t;(void)on;}
static void run(void){memset(seen,0,sizeof(seen));calls=0;FN(0x286f890,void(*)(void*,void*,void*,void*,unsigned,float,float,float))(program,NULL,data,data,0,0,1,0);}
int main(int argc,char**argv){
 assert(argc==2);load(argv[1]);phase="native Pad Link expansion";
 (void)control;patch(0x286d5b0,ignored);patch(0x286d230,voices);
 F(program,0x2808,void*)=instruments;F(program,0x2810,void*)=instruments+128;
 F(program,0x2850,void*)=activity;F(program,0x2878,void*)=pending;
 F(program,0x14a8+60*4,int)=18;data[0]=60;
 for(unsigned n=0;n<6;++n){instruments[18+n]=instrument[n];F(instrument[n],0x2310,void*)=program;}
 F(instrument[0],0x234d,unsigned char)=1;
 for(unsigned k=0;k<4;++k){unsigned target=20+k;F(instrument[0],0xea8+0x58*k,int)=(int)target;float got=FN(0x2806330,float(*)(void*,unsigned))(instrument[0],0x404+k);assert(got*128==target);}
 /* Target also has a link: native launches only the source's direct list. */
 F(instrument[1],0x234d,unsigned char)=1;F(instrument[1],0xea8,int)=24;
 run();assert(calls==5&&seen[0]==1&&seen[1]==1&&seen[2]==1&&seen[3]==1&&seen[4]==1&&!seen[5]);
 F(instrument[2],0x2e0,unsigned char)=1;run();assert(calls==4&&!seen[2]);
 F(instrument[0],0x234d,unsigned char)=0;run();assert(calls==1&&seen[0]==1);
 puts("PASS real native Pad Link getters and voice-launch expansion: root plus four direct targets, muted target skipped, no recursive link expansion; DSP receiver is a fixture.");return 0;
}
