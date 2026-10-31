#include "src/chords.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void expected(struct chord_config*c,unsigned root,const int*notes,int n){
 int out[CHORD_NOTES];assert(chords_make(c,root,out)==n);assert(!memcmp(out,notes,(size_t)n*sizeof(int)));
}
int main(void){
 struct chord_config c={.valid=1,.root=0,.tonic=0,.types=1};
 int maj[]={0,4,7},minor[]={0,3,7},dim[]={0,3,6},aug[]={0,4,8},sus[]={0,5,7};
 expected(&c,0,maj,3);c.types=2;expected(&c,0,minor,3);c.types=4;expected(&c,0,dim,3);
 c.types=8;expected(&c,0,aug,3);c.types=3;expected(&c,0,sus,3);
 c.types=1;c.extensions=15;int extended[]={0,4,7,9,10,11,14};expected(&c,0,extended,7);
 c.extensions=0;c.inversion=1;int first[]={4,7,12};expected(&c,0,first,3);
 c.inversion=-1;int below[]={-5,0,4};expected(&c,0,below,3);c.inversion=0;
 for(int scale=0;scale<8;++scale){int previous=-1;for(unsigned i=0;i<8;++i){int d=chords_degree(scale,i);assert(d>previous);previous=d;}}
 c.root=6;c.semi=2;c.tonic=0;int aligned[]={4,8,11};expected(&c,0,aligned,3);
 c.root=c.semi=c.tonic=0;
 struct chord_voices voices={0};int out[CHORD_NOTES];
 assert(chords_transition(&voices,&c,0,1,out)==3);
 assert(!chords_transition(&voices,&c,0,1,out)); /* Pressure cannot retrigger. */
 assert(chords_transition(&voices,&c,4,1,out)==2); /* G shared between C and G. */
 c.types=2;c.octave=2; /* Press-time pitches survive UI changes. */
 assert(chords_transition(&voices,&c,0,0,out)==2&&out[0]==0&&out[1]==4);
 assert(chords_transition(&voices,&c,4,0,out)==3&&out[0]==7&&out[1]==11&&out[2]==14);
 assert(!voices.held);for(unsigned i=0;i<241;++i)assert(!voices.references[i]);
 assert(!chords_transition(&voices,&c,4,0,out));
 chords_open();chords_context(0,0,0,0,1);
 chords_parameter(13,1);chords_parameter(12,1);chords_get(&c);assert(c.types==3);
 chords_parameter(13,0);chords_parameter(12,0);chords_get(&c);assert(c.types==3);
 chords_parameter(8,1);chords_parameter(8,1);chords_get(&c);assert(c.extensions==1);
 chords_parameter(8,0);chords_parameter(8,1);chords_get(&c);assert(!c.extensions);
 chords_set(CHORD_LATCH,0);chords_parameter(8,0);chords_parameter(14,1);chords_get(&c);assert(c.types==4);
 chords_parameter(14,0);chords_get(&c);assert(!c.types);
 uint64_t light;assert(chords_light(0,&light)&&(uint32_t)light==0x00e060);
 assert(chords_light(9,&light)&&(uint32_t)light==0xffb830);
 assert(chords_light(15,&light)&&(uint32_t)light==0xffffff);
 chords_close();assert(!chords_light(0,&light));chords_parameter(8,0);
 chords_open();assert(chords_parameter(12,1));chords_close();chords_open();chords_close();
 assert(chords_parameter(12,0)); /* Key-up remains owned across reopen. */
 assert(!chords_parameter(12,0)&&!chords_parameter(13,1));
 puts("PASS chords, extensions, inversions, scale roots, tuning alignment, shared ownership, release snapshots and palette");return 0;
}
