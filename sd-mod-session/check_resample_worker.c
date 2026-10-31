#include "src/resample_worker.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdatomic.h>
#include <sched.h>
struct input {uint64_t next;};
struct environment {atomic_uint opens,closes,applies,comparisons,restore_fail,run,async_delay;unsigned source,destination,native_wait,native_operation,native_rendered,native_target;const float*borrowed,*pending_pcm;};
static struct environment env;static struct resample_worker*worker;static struct resample_flow flow;static uint64_t after;
static int source_read(void*user,uint64_t first,size_t count,float*out){
 struct input*i=user;assert(first==i->next&&count<=256);i->next+=count;
 for(size_t n=0;n<count;++n){out[n*2]=.1f*sinf((float)(first+n)*.031f);out[n*2+1]=.1f*cosf((float)(first+n)*.037f);}return 1;
}
static void source_close(void*user){++env.closes;free(user);}
static int source_open(void*user,unsigned source,struct pad_sample_selection*out){
 assert(user==&env&&source<128);struct input*i=calloc(1,sizeof(*i));assert(i);i->next=10;
 *out=(struct pad_sample_selection){.stream={i,6010,48000,source_read,source_close},.first=10,.end=6010,.pad=source};++env.opens;env.source=source;return 1;
}
static int compare(void*user,unsigned source,const float*pcm,uint64_t frames,double rate,int rendered){
 assert(user==&env&&source==env.source&&rate==48000);++env.comparisons;
 if(!rendered&&atomic_load(&env.restore_fail))return 0;
 if(atomic_load(&env.async_delay)){assert(!env.native_operation);env.native_operation=rendered?1:2;env.native_rendered=(unsigned)rendered;env.pending_pcm=pcm;env.native_wait=env.async_delay;return 2;}
 if(rendered){assert(pcm&&frames>=6000&&isfinite(pcm[2]));env.borrowed=pcm;}else env.borrowed=NULL;return 1;
}
static int apply(void*user,const struct resample_spec*spec,const float*pcm,uint64_t frames,double rate){
 assert(user==&env&&spec->source==env.source&&!env.borrowed&&pcm&&frames>=6000&&rate==48000);
 if(atomic_load(&env.async_delay)){assert(!env.native_operation);env.native_operation=3;env.pending_pcm=pcm;env.native_target=spec->destination;env.native_wait=env.async_delay;return 2;}
 env.destination=spec->destination;++env.applies;return 1;
}
static int native_poll(void*user,unsigned op){
 assert(user==&env&&env.native_operation&&((op==3)==(env.native_operation==3)));
 if(env.native_wait){--env.native_wait;return 2;}
 if(env.native_operation==3){assert(env.pending_pcm&&isfinite(env.pending_pcm[2]));env.destination=env.native_target;++env.applies;}
 else if(env.native_rendered){assert(env.pending_pcm&&isfinite(env.pending_pcm[2]));env.borrowed=env.pending_pcm;}
 else env.borrowed=NULL;
 env.pending_pcm=NULL;env.native_operation=0;return 1;
}
static void drain_ui(void){struct resample_worker_event e;if(resample_worker_poll(worker,&after,&e))resample_flow_update(&flow,&e.update);}
static void button(unsigned k){assert(resample_flow_button(&flow,0x6b+k,1));assert(resample_flow_button(&flow,0x6b+k,0));}
static void tick(void){resample_worker_step(worker);drain_ui();resample_flow_retry_cancel(&flow);}
static void until(enum resample_page page){unsigned limit=0;while(flow.page!=page){assert(++limit<1000);tick();}}
static void sample(void){uint64_t occupied[2]={0,0};assert(resample_flow_open(&flow,65,"Native source","Main",1,1,occupied));button(2);until(RS_SAMPLE);}
static void setup(int with_apply){
 struct resample_worker_backend b={.user=&env,.open=source_open,.compare=compare,.apply=with_apply?apply:NULL,.poll=native_poll};worker=resample_worker_create(&b);assert(worker);
 struct resample_backend ui={worker,resample_worker_send};resample_flow_init(&flow,&ui);after=0;
}
static void*thread(void*unused){(void)unused;while(atomic_load(&env.run)){resample_worker_step(worker);sched_yield();}return NULL;}
int main(void){
 setup(1);sample();unsigned initial=env.applies;
 struct fx_chain_config chain={.count=1,.stage={{.kind=FX_STAGE_EQ}}};fx_eq_default(&chain.stage[0].eq);chain.stage[0].eq.band[1].gain_db=6;assert(resample_flow_set_fx(&flow,&chain));
 button(2);tick();assert(flow.page==RS_RENDERING&&flow.captured==4096&&!flow.done&&!env.closes);
 until(RS_READY);assert(env.closes==1&&env.applies==initial&&!env.borrowed);
 button(1);assert(flow.page==RS_SWITCHING&&!flow.compare_rendered);until(RS_READY);assert(env.borrowed&&flow.compare_rendered);
 atomic_store(&env.restore_fail,1);resample_flow_cancel(&flow);tick();assert(flow.page==RS_CANCELLING&&env.borrowed&&env.applies==initial);
 assert(!resample_worker_destroy(worker)&&env.borrowed); /* Result must stay alive. */
 atomic_store(&env.restore_fail,0);until(RS_CLOSED);assert(!env.borrowed&&env.applies==initial&&env.opens==env.closes);
 sample();button(2);tick();assert(flow.page==RS_RENDERING);resample_flow_cancel(&flow);until(RS_CLOSED);assert(env.opens==env.closes);
 sample();button(2);until(RS_READY);resample_flow_knob(&flow,0,1);resample_flow_knob(&flow,1,1);resample_flow_knob(&flow,2,2);
 assert(resample_flow_destination(&flow)==83);button(2);assert(flow.page==RS_APPLYING&&env.applies==initial);until(RS_CLOSED);
 assert(env.applies==initial+1&&env.destination==83&&env.opens==env.closes);
 sample();flow.spec.velocity=64;button(2);until(RS_ERROR);assert(strstr(flow.message,"velocity")&&env.applies==initial+1);button(0);until(RS_CLOSED);
 sample();button(2);until(RS_READY);atomic_store(&env.async_delay,2);button(1);tick();assert(flow.page==RS_SWITCHING&&env.native_operation&&!flow.compare_rendered);
 resample_flow_cancel(&flow);tick();assert(flow.page==RS_CANCELLING&&env.native_operation&&!resample_worker_destroy(worker));
 until(RS_CLOSED);assert(!env.borrowed&&!env.pending_pcm&&!env.native_operation);
 sample();button(2);until(RS_READY);resample_flow_knob(&flow,0,1);resample_flow_knob(&flow,2,1);button(2);tick();
 assert(flow.page==RS_APPLYING&&env.applies==initial+1&&env.pending_pcm&&!resample_worker_destroy(worker));
 resample_flow_cancel(&flow);assert(flow.page==RS_CANCELLING);
 until(RS_CLOSED);assert(flow.applied&&env.applies==initial+2&&!env.pending_pcm&&!env.native_operation);atomic_store(&env.async_delay,0);
 struct resample_command c={.action=RS_ACT_CANCEL,.token=999,.spec={.source=1,.destination=1}};
 for(unsigned i=0;i<16;++i)assert(resample_worker_send(worker,&c));
 assert(!resample_worker_send(worker,&c));
 for(unsigned i=0;i<16;++i)resample_worker_step(worker);
 assert(resample_worker_destroy(worker));
 setup(1);sample();struct fx_chain_config unavailable={.count=1,.stage={{.kind=FX_STAGE_NATIVE,.native_id=1}}};
 assert(resample_flow_set_fx(&flow,&unavailable));unsigned closed_before=env.closes,applied_before=env.applies;
 button(2);until(RS_ERROR);assert(!strcmp(flow.message,"Native FX unavailable")&&env.closes==closed_before&&env.applies==applied_before);
 button(0);until(RS_CLOSED);assert(env.opens==env.closes&&resample_worker_destroy(worker));
 puts("PASS unavailable native FX is reported separately from render memory; source retained until Cancel, no Apply or leak");
 puts("PASS UI controller -> copied bounded worker queue -> native-source interface -> real bounded PCM/EQ renderer; capture/render progress, compare/apply acknowledgements, cancellation midway, failed Original restore retains borrowed result, bank target, no implicit model write, velocity unsupported fails closed,16-command overflow and full reader cleanup; native mix/Undo callbacks are adapters");
 setup(0);sample();button(2);until(RS_READY);button(2);assert(flow.page==RS_CONFIRM);button(2);until(RS_ERROR);assert(strstr(flow.message,"Apply/Undo")&&env.applies==initial+2);button(0);until(RS_CLOSED);assert(resample_worker_destroy(worker));
 puts("PASS missing native Apply/Undo adapter never reports successful Apply and releases staged source/result on Cancel");
 puts("PASS pending native compare/cancel/restore/Apply keeps PCM alive, polls without blocking, waits for final transaction acknowledgement and forbids destruction while a native task can borrow output");
 setup(1);atomic_store(&env.run,1);pthread_t owner;assert(!pthread_create(&owner,NULL,thread,NULL));
 for(unsigned trial=0;trial<50;++trial){
  assert(resample_flow_open(&flow,65,"Test","Main",1,1,NULL));button(2);
  unsigned spins=0;while(flow.page!=RS_SAMPLE){drain_ui();sched_yield();assert(++spins<1000000);}
  button(2);spins=0;while(flow.page!=RS_READY){drain_ui();sched_yield();assert(++spins<1000000);}
  button(0);spins=0;while(flow.page!=RS_SAMPLE){drain_ui();sched_yield();assert(++spins<1000000);}
  resample_flow_cancel(&flow);spins=0;while(flow.page!=RS_CLOSED){drain_ui();sched_yield();assert(++spins<1000000);}
 }
 atomic_store(&env.run,0);assert(!pthread_join(owner,NULL));assert(env.opens==env.closes&&resample_worker_destroy(worker));
 puts("PASS50 concurrent UI/worker prepare/render/back/reprepare/cancel sessions, coalesced snapshot polling, copied error lifetime and balanced source ownership; no device queue or audio thread invoked");
}
