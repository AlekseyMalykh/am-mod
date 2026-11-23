/* Native callback order supplied as values; no claimed clock/binding hooks. */
#define MPC_PAD_MIDI_RECORDING_TRIAL 1
#include "src/synth.c"
#include "src/synth_sequence.c"
#include <assert.h>
#include <stdio.h>
static struct am_synth_sequence player;
static struct am_synth_sequence_voice a,b;
static void render(void){float l[128]={0},r[128]={0};am_synth_render_add(l,r,128);for(unsigned i=0;i<128;++i)assert(isfinite(l[i])&&isfinite(r[i]));}
static unsigned held(void){unsigned n=0;for(unsigned i=0;i<VOICES;++i)n+=voices[i].owner.id&&voices[i].env!=ENV_OFF&&voices[i].env!=ENV_RELEASE;return n;}
int main(void){
 struct am_midi_event e={.tick=7600,.duration=960,.tune=0,.source_note=51,.velocity=73,.probability=25,.repeats=4};
 assert(am_synth_sequence_configure(&player,1)==1);
 assert(am_synth_sequence_on(&player,&a,1,1,1,&e,0)==1);
 e.velocity=101;assert(am_synth_sequence_on(&player,&b,1,1,2,&e,0)==1);render();assert(held()==2);
 /* Identical notes have separate tokens; native probability/repeats metadata
  * must not cause a second scheduling decision in this sink. */
 assert(am_synth_sequence_on(&player,&b,1,1,2,&e,0)==0);render();assert(held()==2);
 assert(am_synth_sequence_on(&player,&b,1,1,3,&e,0)==-1);
 for(unsigned i=0;i<100;++i)render();
 assert(held()==2); /* No local duration timer. */
 assert(am_synth_sequence_off(&player,&b,1,1,2)==1);render();assert(held()==1);
 assert(am_synth_sequence_off(&player,&b,1,1,2)==0);render();assert(held()==1);
 assert(am_synth_sequence_off(&player,&a,1,1,1)==1);render();assert(!held());
 e.tick=0;assert(am_synth_sequence_on(&player,&a,1,1,3,&e,12)==1);render();assert(held()==1);
 assert(am_synth_sequence_off(&player,&a,1,1,1)==0);render();assert(held()==1); /* reused native slot */
 e.slice=1;assert(am_synth_sequence_on(&player,&b,1,1,4,&e,0)==-2);e.slice=0;
 e.muted=1;assert(am_synth_sequence_on(&player,&b,1,1,4,&e,0)==0);e.muted=0;
 e.tune=100;assert(am_synth_sequence_on(&player,&b,1,1,4,&e,0)==-2);e.tune=0;
 assert(am_synth_sequence_on(&player,&b,1,1,4,&e,INT32_MAX)==-2);
 for(unsigned i=0;i<QUEUE;++i)am_synth_cc(71,100);
 assert(am_synth_sequence_off(&player,&a,1,1,3)==-1);assert(am_synth_sequence_retry(&player,&a)==-1);
 render();assert(am_synth_sequence_retry(&player,&a)==1);render();assert(!held());
 assert(am_synth_sequence_on(&player,&a,1,1,4,&e,0)==1);render();assert(held()==1);
 for(unsigned i=0;i<QUEUE;++i)am_synth_cc(71,100);
 assert(am_synth_sequence_stop(&player)==1);render();assert(!held());
 assert(am_synth_sequence_on(&player,&a,1,1,4,&e,0)==0);
 assert(am_synth_sequence_on(&player,&a,1,1,999,&e,0)==0);
 assert(am_synth_sequence_on(&player,&a,1,2,5,&e,0)==1); /* reuse after Stop */
 assert(am_synth_sequence_off(&player,&a,1,1,4)==0);render();assert(held()==1);
 assert(am_synth_sequence_configure(&player,2)==1);render();assert(!held());
 assert(am_synth_sequence_on(&player,&a,1,1,99,&e,0)==0);
 assert(am_synth_sequence_off(&player,&a,1,1,1)==0);
 assert(am_synth_sequence_on(&player,&a,2,1,1,&e,0)==1);render();assert(held()==1);
 assert(am_synth_sequence_off(&player,&a,1,1,1)==0);render();assert(held()==1);
 assert(am_synth_sequence_stop(&player)==1);render();assert(!held());
 assert(am_synth_sequence_retry(&player,&a)==0&&!a.occurrence);
 for(unsigned i=0;i<1500;++i)render();
 assert(!am_synth_active());
 puts("PASS Synth sequence sink: overlapping notes/loops, native-only decisions, exact Off, retry/Stop, source epochs, reused native token, finite PCM");
}


