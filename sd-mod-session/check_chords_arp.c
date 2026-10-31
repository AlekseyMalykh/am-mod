#include "src/chords.h"
#include <assert.h>
#include <stdio.h>
static void seq(int pattern,int range,const int*n,int c,int steps,const int*expect){
 unsigned s=0,seed=1;int out[16];for(int i=0;i<steps;++i){int k=chords_arp_step(pattern,range,n,c,&s,&seed,out);assert(k==1);
 if(expect&&out[0]!=expect[i]){printf("pattern %d step %d got %d want %d\n",pattern,i,out[0],expect[i]);assert(0);}}
}
int main(void){
 int ceg[3]={7,0,4}; /* pressed order G C E */
 int up[6]={0,4,7,0,4,7},down[6]={7,4,0,7,4,0},ud[6]={0,4,7,4,0,4},ude[7]={0,4,7,7,4,0,0},asp[4]={7,0,4,7};
 seq(ARP_UP,1,ceg,3,6,up);seq(ARP_DOWN,1,ceg,3,6,down);seq(ARP_UP_DOWN,1,ceg,3,6,ud);
 seq(ARP_UP_DOWN_ENDS,1,ceg,3,7,ude);seq(ARP_AS_PLAYED,1,ceg,3,4,asp);
 int up2[7]={0,4,7,12,16,19,0};seq(ARP_UP,2,ceg,3,7,up2);
 unsigned s=0,seed=5;int out[16];assert(chords_arp_step(ARP_CHORD,1,ceg,3,&s,&seed,out)==3&&s==1);
 for(int i=0;i<50;++i){int k=chords_arp_step(ARP_RANDOM,2,ceg,3,&s,&seed,out);assert(k==1);assert(out[0]==0||out[0]==4||out[0]==7||out[0]==12||out[0]==16||out[0]==19);}
 assert(!chords_arp_step(ARP_OFF,1,ceg,3,&s,&seed,out));
 int one[1]={5};seq(ARP_UP_DOWN,1,one,1,3,(int[]){5,5,5});
 puts("PASS arpeggiator patterns, range, chord repeat, random bounds, single note");
}
