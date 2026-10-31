#define main unused_fx_main
#include "check_fx_native.c"
#undef main
#include "src/resample_stock.h"
#define F(p,o,T) (*(T*)((char*)(p)+(o)))
static unsigned char context[0xa50],settings[0x900],thread_owner[0x200],audio_info[0xb0],engine[0x800],track[0xc00],sample_track[0xc00],note[56],target[24],things[0x148],program[0x2900];
static uintptr_t executor[5];
static unsigned trigger_calls,accepted,last_source,renders;
static void patch(uintptr_t at,void*fn){uint32_t code[]={0x58000050,0xd61f0200};memcpy((void*)(base+at),code,8);memcpy((void*)(base+at+8),&fn,8);__builtin___clear_cache((char*)(base+at),(char*)(base+at+16));}
static void nothing(void*p){(void)p;}
static void toggle(void*p,int on){assert(p==thread_owner+0x30&&!on);}
static void*no_tls(unsigned key){assert(!key);return NULL;}
static unsigned clock_ms(void){return 0;}
static void zero(float*p,unsigned n){memset(p,0,n*sizeof(float));}
static void trigger(void*e,int64_t tick,void*n,void*t,unsigned source,unsigned mask,unsigned on){
 assert(e==engine&&!tick&&n==note&&t==(source==3?track:sample_track)&&mask==0x7ffffff8&&on==1);
 ++trigger_calls;last_source=source;
 assert(F(n,8,unsigned)==3&&F(n,48,unsigned char)==60);
 /* This is the actual native TrackInputFilter policy, not a fake voice success. */
 if(FN(0x282fe30,int(*)(void*,unsigned,unsigned))((char*)t+0x750,source,0))++accepted;
}
static void render(void*c,void*s,void*out,void*gain,void*buf,void*time,unsigned stamp){
 (void)gain;(void)buf;(void)time;assert(c==context&&s==settings&&out==target&&!stamp);++renders;
}
static void run(void){FN(0x18b9a50,void(*)(void*,void*,void*,void*,void*))(context,settings,target,note,track);}
int main(int argc,char**argv){
 assert(argc==2);struct sigaction sa={.sa_sigaction=fault,.sa_flags=SA_SIGINFO};sigaction(SIGSEGV,&sa,NULL);sigaction(SIGILL,&sa,NULL);load(argv[1]);phase="single-note native trigger";
 patch(0xb53508,no_tls);patch(0x2142bd0,nothing);patch(0xdd20b0,zero);patch(0xf2f240,clock_ms);patch(0x211aa30,trigger);patch(0x18b97c0,render);patch(0x18b4500,nothing);patch(0x2227150,nothing);
 executor[4]=(uintptr_t)toggle;F(thread_owner,0x30,void*)=executor;
 F(context,0x70,void*)=thread_owner;F(context,0x88,void*)=audio_info;F(audio_info,0xa8,unsigned)=128;
 F(settings,0x588,void*)=audio_info;F(settings,0x5c0,void*)=engine;F(note,8,unsigned)=3;F(note,48,unsigned char)=60;
 F(engine,0x3c0,void*)=things;F(things,0xb8,void*)=program;F(program,0x14a8+60*4,int)=18;F(program,0x12a8+18*4,int)=60;
 F(things,0x140,void*)=sample_track;F(sample_track,0xbb8,void*)=program;
 F(track,0x750+40,unsigned)=1;run();assert(trigger_calls==1&&last_source==3&&!accepted&&renders==1);
 void*blocked=mmap((void*)(base+0x7600000),0x1b00000,PROT_NONE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);assert(blocked==(void*)(base+0x7600000));
 void*below=mmap((void*)(base-0x100000),4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);assert(below==(void*)(base-0x100000));F(below,0,unsigned)=0x1234;
 assert(resample_stock_install(base));assert(F(below,0,unsigned)==0x1234);run();assert(trigger_calls==2&&last_source==0&&accepted==1&&renders==2);
 for(unsigned policy=0;policy<4;++policy){F(sample_track,0x750+40,unsigned)=policy;run();assert(last_source==0&&accepted==policy+2);}
 F(program,0x12a8+18*4,int)=61;unsigned previous=trigger_calls;run();assert(trigger_calls==previous);F(program,0x12a8+18*4,int)=60;
 F(things,0xb8,void*)=NULL;run();assert(trigger_calls==previous);F(things,0xb8,void*)=program;
 assert(F((void*)base,0x18b9f84,uint32_t)==0x52800082); /* Seq's separate render path stays byte-exact. */
 puts("PASS actual native single-note renderer and near BL relay: dedicated call targets validated AC50 Sample track, mapped MIDI60 and export mask unchanged, wrong/missing program rejected, Seq path untouched; receiver and output writer are fixtures, source policy uses actual native TrackInputFilter");return 0;
}
