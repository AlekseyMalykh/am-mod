/* Actual native Single wrapper, settings construction and submission copy.
 * Stop at18b81f0: application task queue and audio driver are not started. */
#define STOCK_PROOF_MAIN unused_stock_proof
#include "check_resample_stock.c"
static unsigned char services[0x800],task_runner[0x200];
static unsigned submitted;
static void no_initialize(void*p){assert(!p);}
static void queue_boundary(void*q,void*service,unsigned sequence_index,void*s){
 assert(q==task_runner&&service==services&&service!=engine&&sequence_index==7);++submitted;
 assert(F(s,0x218,unsigned)==48000&&F(s,0x2c8,unsigned)==4&&F(s,0x690,unsigned)==18);
 assert(F(s,0x6e8,uintptr_t)==F(path,0,uintptr_t));
 assert(F(service,0x6e8,uintptr_t)==0x70&&F(service,0x218,unsigned)==0xdeadbeef);
}
int main(int argc,char**argv){
 assert(argc==2);struct sigaction sa={.sa_sigaction=fault,.sa_flags=SA_SIGINFO};sigaction(SIGSEGV,&sa,NULL);sigaction(SIGILL,&sa,NULL);sigaction(SIGABRT,&sa,NULL);load(argv[1]);phase="actual settings versus render service context";
 patch(0x18b81f0,queue_boundary);patch(0x1c07070,no_initialize);
 F(things,0x1c8,void*)=sequence;F(sequence,0x200,int)=7;F(sequence,0x480,int)=7;F(things,0x500,unsigned)=18;
 F(runner,0xb0,void*)=services;F(runner,0xb8,void*)=task_runner;F(services,0x5c0,void*)=engine;F(services,0x6e8,uintptr_t)=0x70;F(services,0x218,unsigned)=0xdeadbeef;
 FN(0xf535c0,void(*)(void*,const char*))(path,"/tmp/never-rendered-service-proof.wav");stock_callback_init(callback);
 FN(0x18c63b0,void(*)(void*,void*,void*,unsigned,unsigned,unsigned,void*,unsigned,unsigned))(runner,path,things,0,48000,24,callback,1,1);
 assert(submitted==1);stock_callback_destroy(callback);FN(0xf0a290,void(*)(void*))(path);
 puts("PASS actual18c63b0 ->18c5a00 ->18c54d0 ->18b81f0: submission settings +6e8 is filename/+218 rate; runner+b0 is a separate service context whose same offsets hold0x70/deadbeef; native queue receives the exact service pointer. Background task/audio remain unexecuted.");return 0;
}
