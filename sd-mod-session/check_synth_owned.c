/* Real Synth queue/DSP; inspect audio-owned voices to verify exact ownership. */
#define MPC_PAD_MIDI_RECORDING_TRIAL 1
#include "src/synth.c"
#include <assert.h>
#include <stdio.h>
static unsigned live_owned(struct am_synth_handle h){
 unsigned n=0;for(unsigned i=0;i<VOICES;++i)n+=voices[i].owner.id==h.id&&voices[i].owner.epoch==h.epoch&&voices[i].env!=ENV_OFF&&voices[i].env!=ENV_RELEASE;return n;
}
static unsigned live_legacy(unsigned note){
 unsigned n=0;for(unsigned i=0;i<VOICES;++i)n+=!voices[i].owner.id&&voices[i].note==note&&voices[i].env!=ENV_OFF&&voices[i].env!=ENV_RELEASE;return n;
}
static float render(void){
 float l[256]={0},r[256]={0},energy=0;am_synth_render_add(l,r,256);
 for(unsigned i=0;i<256;++i){assert(isfinite(l[i])&&isfinite(r[i]));energy+=fabsf(l[i])+fabsf(r[i]);}return energy;
}
int main(void){
 struct am_synth_handle a={999,99},b,stolen;
 assert(!am_synth_owned_on(128,100,&a)&&a.id==999&&a.epoch==99);
 assert(!am_synth_owned_on(60,0,&a)&&a.id==999);
 assert(!am_synth_owned_off((struct am_synth_handle){0,0}));
 assert(am_synth_owned_on(60,30,&a)&&am_synth_owned_on(60,110,&b));
 assert(a.id!=b.id&&a.epoch==b.epoch);am_synth_note(60,80,1);
 assert(render()>0&&live_owned(a)==1&&live_owned(b)==1&&live_legacy(60)==1);
 assert(am_synth_owned_off(a));render();assert(!live_owned(a)&&live_owned(b)==1&&live_legacy(60)==1);
 am_synth_note(60,0,0);render();assert(live_owned(b)==1&&!live_legacy(60));
 am_synth_note(60,80,1);render();assert(live_legacy(60)==1);
 for(unsigned i=0;i<QUEUE;++i)am_synth_cc(71,100);
 am_synth_note(60,0,0);render();assert(live_owned(b)==1&&!live_legacy(60)); /* overflow panic is legacy-only */
 assert(am_synth_owned_off(a));render();assert(live_owned(b)==1); /* duplicate Off */
 am_synth_cc(123,0);render();assert(!live_owned(b)); /* explicit all-notes-off is global */
 assert(am_synth_owned_reset());render();assert(!live_owned(b));
 /* New project/Stop revokes an On which has not reached audio yet. */
 assert(am_synth_owned_on(62,100,&a));assert(am_synth_owned_reset());render();assert(!live_owned(a));
 assert(am_synth_owned_on(62,100,&b));assert(am_synth_owned_off(a));render();assert(live_owned(b)==1);
 /* Old Off after voice stealing must not release its replacement. */
 assert(am_synth_owned_reset());render();
 for(unsigned i=0;i<8;++i){assert(am_synth_owned_on(64,100,&a));render();}
 stolen=a;
 for(unsigned i=0;i<8;++i){assert(am_synth_owned_on(64,100,&b));render();}
 assert(!live_owned(stolen)&&live_owned(b)==1);assert(am_synth_owned_off(stolen));render();assert(live_owned(b)==1);
 /* Rejected Off keeps the handle usable for retry after consumer progress. */
 for(unsigned i=0;i<QUEUE;++i)am_synth_cc(71,100);
 assert(!am_synth_owned_off(b));render();assert(live_owned(b)==1);
 assert(am_synth_owned_off(b));render();assert(!live_owned(b));
 /* Reset does not require a queue slot and never kills a live legacy key. */
 am_synth_note(67,80,1);render();assert(live_legacy(67)==1);
 assert(am_synth_owned_on(67,100,&b));render();assert(live_owned(b)==1);
 for(unsigned i=0;i<QUEUE;++i)am_synth_cc(71,100);
 assert(am_synth_owned_reset());render();assert(!live_owned(b)&&live_legacy(67)==1);
 /* An unpublished producer cannot resurrect a revoked On after reset. */
 unsigned h=atomic_load(&event_head);assert(h==atomic_load(&event_tail));
 atomic_store(&event_head,h+1);assert(am_synth_owned_on(69,100,&a));
 assert(am_synth_owned_reset());render();assert(!live_owned(a));
 event_queue[h%QUEUE].command=(struct synth_command){2u<<16|71u,0,0};
 atomic_store(&event_queue[h%QUEUE].sequence,h+1);render();assert(!live_owned(a));
 /* Transactional rejection at On capacity; no borrowed handle or ID reuse. */
 for(unsigned i=0;i<QUEUE-16;++i)am_synth_cc(71,100);
 a=(struct am_synth_handle){777,77};assert(!am_synth_owned_on(60,100,&a)&&a.id==777&&a.epoch==77);render();
 am_synth_note(67,0,0);assert(am_synth_owned_reset());
 for(unsigned i=0;i<1000;++i)render();
 assert(!am_synth_active());
 atomic_store(&owned_serial,UINT64_MAX);assert(!am_synth_owned_on(60,100,&a));
 atomic_store(&owned_epoch,UINT32_MAX);assert(!am_synth_owned_reset());
 puts("PASS owned Synth: independent unisons/velocity, stale Off, stealing, queue retry, reset barrier, legacy isolation, finite PCM");
}
