#define STOCK_PROOF_MAIN export_proof_main
#include "check_resample_stock.c"
#include "src/resample_stock_backend.h"
extern void stock_queue_pump(void),stock_test_layer(void),stock_callback_init_action(void*,void(*)(void));
static unsigned char copy_context[0xe0],mode_registry[0x180],mode_node[0x40],program[0x2900],instruments[2][0x2468],samples[2][0x180];
static void*pad_map[128];static uintptr_t pad_samples[128];
void stock_test_layer_c(void*instrument,int layer,uintptr_t*out){
 out[0]=out[1]=0;if(layer)return;
 if(instrument==instruments[0])out[0]=pad_samples[18];else {assert(instrument==instruments[1]);out[0]=pad_samples[19];}
}
static void assign_rendered(void){pad_samples[requested_target]=(uintptr_t)samples[1];}
static struct resample_stock_reply reply;
#ifdef MPC_PAD_EQ
static void*eq_program;static unsigned eq_binds;
void pad_eq_native_program(void*p){eq_program=p;++eq_binds;}
void pad_eq_native_clear(void){eq_program=NULL;}
#endif
static void poll_reply(struct resample_stock_backend*b,unsigned status){assert(resample_stock_backend_poll(b,&reply)&&reply.status==status);}
#ifndef STOCK_BACKEND_MAIN
#define STOCK_BACKEND_MAIN main
#endif
int STOCK_BACKEND_MAIN(int argc,char**argv){
 assert(!export_proof_main(argc,argv));phase="production stock backend";
 stock_callback_init_action(callback,assign_rendered);FN(0xf535c0,void(*)(void*,const char*))(path,"/run/fixture-not-written.wav");
 patch(0x2803b90,stock_test_layer);F(program,0x2808,void*)=pad_map;F(program,0x2810,void*)=pad_map+128;
 pad_map[18]=instruments[0];pad_map[19]=instruments[1];F(instruments[0],0x2310,void*)=program;F(instruments[1],0x2310,void*)=program;
 pad_samples[18]=(uintptr_t)samples[0];F(samples[0],0xa8,const char*)="Stock source";F(samples[1],0xa8,const char*)="Stock result";
 F(things,0xb8,void*)=F(properties,0xb8,void*)=program;F(things,0x5f8,unsigned)=F(properties,0x500,unsigned)=18;
 F(copy_context,0,uintptr_t)=base+0x6e0dfd8;F(copy_context,0x48,void*)=dummy_program;F(copy_context,0x78,void*)=things;F(copy_context,0xc8,void*)=properties;F(copy_context,0x98,void*)=mode_registry;
 F(mode_registry,0x168,void*)=mode_node;F(mode_node,0x28,void*)=mode;
 struct resample_stock_backend*b=resample_stock_backend_create(base,copy_context);assert(b);
#ifdef MPC_PAD_EQ
 F(copy_context,0x98,void*)=NULL;
 assert(resample_stock_backend_bind_eq(b));assert(!eq_binds);stock_queue_pump();assert(eq_binds==1&&eq_program==program&&!resample_stock_backend_poll(b,&reply));
 assert(resample_stock_backend_capture(b));stock_queue_pump();poll_reply(b,RSB_ERROR);assert(eq_binds==2&&eq_program==program);
 F(copy_context,0x98,void*)=mode_registry;
 puts("PASS EQ binds on model owner without resample mode; unavailable resample capture cannot disable EQ or fabricate capture reply");
#endif
 assert(resample_stock_backend_capture(b));stock_queue_pump();poll_reply(b,RSB_CAPTURED);
 assert(reply.snapshot.source==18&&reply.snapshot.has_sample&&reply.snapshot.has_sequence&&!strcmp(reply.snapshot.sample,"Stock source")&&reply.snapshot.occupied[0]==(UINT64_C(1)<<18));
 struct resample_destination_request r={101,reply.snapshot.epoch,RESAMPLE_SOUND,18,19,0};requested_target=19;unsigned before=starts;
 assert(resample_stock_backend_start(b,&r)&&!resample_stock_backend_start(b,&r));stock_queue_pump();poll_reply(b,RSB_SUBMITTED);assert(starts==before+1&&resample_stock_backend_busy(b)&&!pad_samples[19]);
 assert(!resample_stock_backend_poll(b,&reply));stock_queue_pump();assert(!pad_samples[19]);
 stock_callback_release(1);assert(pad_samples[19]==(uintptr_t)samples[1]);assert(!resample_stock_backend_poll(b,&reply));stock_queue_pump();poll_reply(b,RSB_APPLIED);assert(reply.token==101&&!resample_stock_backend_busy(b));
 r.token=102;assert(resample_stock_backend_start(b,&r));stock_queue_pump();poll_reply(b,RSB_ERROR);assert(starts==before+1&&!resample_stock_backend_busy(b)); /* Occupied now. */
 r.token=103;r.overwrite_confirmed=1;assert(resample_stock_backend_start(b,&r)&&resample_stock_backend_cancel(b,r.token));stock_queue_pump();poll_reply(b,RSB_CANCELLED);assert(starts==before+1);
 r.token=104;assert(resample_stock_backend_start(b,&r));resample_stock_backend_changed(b);stock_queue_pump();poll_reply(b,RSB_ERROR);assert(starts==before+1);
 assert(resample_stock_backend_capture(b));stock_queue_pump();poll_reply(b,RSB_CAPTURED);r.source_epoch=reply.snapshot.epoch;r.token=105;
 assert(resample_stock_backend_start(b,&r));stock_queue_pump();poll_reply(b,RSB_SUBMITTED);stock_callback_release(0);assert(!resample_stock_backend_poll(b,&reply));stock_queue_pump();poll_reply(b,RSB_ERROR);assert(!resample_stock_backend_busy(b));
 resample_stock_backend_stop(b);assert(!resample_stock_backend_capture(b)&&!resample_stock_backend_start(b,&r));stock_callback_destroy(callback);FN(0xf0a290,void(*)(void*))(path);
 puts("PASS production stock backend captures source/all-layer occupancy on queued owner, protects destination and epoch, serializes jobs, distinguishes submission from assigned result, handles callback disposal/cancel/stop; queue/render/Sample getter boundaries are fixtures");return 0;
}
