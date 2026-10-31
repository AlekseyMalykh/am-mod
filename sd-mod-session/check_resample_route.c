/* Real native export/input/normalizer/dispatch paths. The final track
 * processing and renderer remain fixtures; this is not generated audio. */
#define main unused_fx_main
#include "check_fx_native.c"
#undef main
#include "src/resample_stock.h"
#define F(p,o,T) (*(T*)((char*)(p)+(o)))
static unsigned char engine[0x800],things[0x700],track[0x2200],wrong_track[0x2200],program[0x2900],policy[0x210],routes[0x90],clock_data[0xd0],note[56],queued[8][96];
static unsigned char context[0xa50],settings[0x900],thread_owner[0x200],audio_info[0xb0],target[24];
static uintptr_t executor[5];
static uintptr_t property_vtable[3];
static unsigned received;
static void patch(uintptr_t at,void*fn){uint32_t code[]={0x58000050,0xd61f0200};memcpy((void*)(base+at),code,8);memcpy((void*)(base+at+8),&fn,8);__builtin___clear_cache((char*)(base+at),(char*)(base+at+16));}
static int disabled(void*p){(void)p;return 0;}
static void ignored(void*p){(void)p;}
static void transformed(void*t,int64_t tick,void*e){assert(t==track&&!tick&&e&&F(e,48,unsigned char)==60&&F(e,16,float)==1.f);++received;}
static void out_event(void*p,int n,int count,void*a,void*b,void*c){(void)p;(void)n;(void)count;(void)a;(void)b;(void)c;}
static void reset_queue(void){memset(queued,0,sizeof(queued));F(engine,0x6e8,void*)=queued;F(engine,0x6f0,void*)=queued;F(engine,0x6f8,void*)=queued+8;}
static void toggle(void*p,int on){assert(p==thread_owner+0x30&&!on);}
static void*no_tls(unsigned key){assert(!key);return NULL;}
static unsigned clock_ms(void){return 0;}
static void zero(float*p,unsigned n){memset(p,0,n*sizeof(float));}
static void render(void*c,void*s,void*out,void*gain,void*buf,void*time,unsigned stamp){(void)gain;(void)buf;(void)time;assert(c==context&&s==settings&&out==target&&!stamp);}
int main(int argc,char**argv){
 assert(argc==2);struct sigaction sa={.sa_sigaction=fault,.sa_flags=SA_SIGINFO};sigaction(SIGSEGV,&sa,NULL);sigaction(SIGILL,&sa,NULL);sigaction(SIGABRT,&sa,NULL);load(argv[1]);phase="native export input routing";
 patch(0x21152d0,ignored);patch(0x2133800,disabled);patch(0x29cb250,transformed);patch(0x212cba0,out_event);patch(0x3b89890,disabled);
 F(engine,0x3c0,void*)=things;F(engine,0x5d8,void*)=policy;F(engine,0x7a8,void*)=routes;F(engine,0x788,void*)=clock_data;
 F(things,0xb8,void*)=program;F(things,0x140,void*)=track;F(things,0x368,void*)=wrong_track;F(track,0xbb8,void*)=program;
 property_vtable[2]=(uintptr_t)disabled;F(track,0x890,void*)=property_vtable;
 F(program,0x12a8+18*4,int)=60;F(program,0x14a8+60*4,int)=18;
 /* Native factory supplies all default probability/modifier fields. */
 typedef struct {uint64_t q[7];} Event;
 *(Event*)note=FN(0x27e54d0,Event(*)(unsigned,int64_t,uint64_t,unsigned,unsigned,float))(60,0,4,100,0,1.f);
 assert(F(note,8,unsigned)==3&&F(note,48,unsigned char)==60);
 reset_queue();unsigned char raw[56];memcpy(raw,note,56);raw[48]=18;
 FN(0x211b090,void(*)(void*,int64_t,void*,unsigned))(engine,0,raw,0x7ffffffa);
 assert(!received&&F(engine,0x6f0,void*)==queued);
 F(track,0x750+40,unsigned)=1;
 FN(0x211aa30,void(*)(void*,int64_t,void*,void*,unsigned,unsigned,unsigned))(engine,0,note,track,3,0x7ffffff8,1);
 assert(!received&&F(engine,0x6f0,void*)==queued);
 FN(0x211aa30,void(*)(void*,int64_t,void*,void*,unsigned,unsigned,unsigned))(engine,0,note,track,0,0x7ffffff8,1);
 assert(received==1);
 patch(0xb53508,no_tls);patch(0x2142bd0,ignored);patch(0xdd20b0,zero);patch(0xf2f240,clock_ms);patch(0x18b97c0,render);patch(0x18b4500,ignored);patch(0x2227150,ignored);
 executor[4]=(uintptr_t)toggle;F(thread_owner,0x30,void*)=executor;F(context,0x70,void*)=thread_owner;F(context,0x88,void*)=audio_info;F(audio_info,0xa8,unsigned)=128;F(settings,0x588,void*)=audio_info;F(settings,0x5c0,void*)=engine;
 assert(resample_stock_install(base));
 FN(0x18b9a50,void(*)(void*,void*,void*,void*,void*))(context,settings,target,note,wrong_track);assert(received==2);
 F(things,0x140,void*)=NULL;FN(0x18b9a50,void(*)(void*,void*,void*,void*,void*))(context,settings,target,note,wrong_track);assert(received==2);F(things,0x140,void*)=track;
 F(track,0xbb8,void*)=NULL;FN(0x18b9a50,void(*)(void*,void*,void*,void*,void*))(context,settings,target,note,wrong_track);assert(received==2);
 puts("PASS actual native factory, dedicated single-note export, receiver, normalizer, TrackInputFilter and dispatch: empty live routes drop ordinary input; source3 rejected by policy1; production adapter explicitly reaches the validated AC50 track with MIDI60/velocity1; missing/wrong track rejected. Recording/outbound-event/final track processing and renderer are fixtures; generated audio remains unproven.");return 0;
}
