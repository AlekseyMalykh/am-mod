#include "chords.h"
#include <pthread.h>
#include <stdatomic.h>
#include <string.h>
#include "pad_palette.h"
static pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;
static struct chord_config state;
/* Physical ownership survives close/reopen until the matching key-up. */
static unsigned owned_params;
static _Atomic unsigned active;
static _Atomic uint64_t generation;
static const unsigned masks[8]={0xab5,0x5ad,0x6ad,0x5ab,0xad5,0x6b5,0x56b,0xfff};
static int clamp(int x,int lo,int hi){return x<lo?lo:x>hi?hi:x;}
static void changed(void){atomic_fetch_add(&generation,1);}
void chords_open(void){pthread_mutex_lock(&mutex);state.enabled=1;state.latch=1;state.types=1;
 /* Every entry starts with the arpeggiator Off; when turned on it runs at
  * 1/4 over one octave. */
 state.arp=ARP_OFF;state.arp_rate=0;state.arp_range=1;state.repeat=0;
 state.extensions=state.held_params=state.held_roots=0;state.octave=state.inversion=0;state.last_root=0;
 atomic_store(&active,1);changed();pthread_mutex_unlock(&mutex);}
void chords_close(void){pthread_mutex_lock(&mutex);if(state.enabled){state.enabled=0;atomic_store(&active,0);changed();}pthread_mutex_unlock(&mutex);}
int chords_active(void){return atomic_load(&active)!=0;}
uint64_t chords_generation(void){return atomic_load(&generation);}
void chords_get(struct chord_config*out){pthread_mutex_lock(&mutex);*out=state;pthread_mutex_unlock(&mutex);}
void chords_context(int root,int tonic,int scale,int semi,int valid){
 pthread_mutex_lock(&mutex);
 valid=!!valid&&root>=0&&root<12&&tonic>=0&&tonic<12&&scale>=0&&scale<8&&semi>=-96&&semi<=96;
 if(state.root!=root||state.tonic!=tonic||state.scale!=scale||state.semi!=semi||state.valid!=valid){
  state.root=root;state.tonic=tonic;state.scale=scale;state.semi=semi;state.valid=valid;changed();}
 pthread_mutex_unlock(&mutex);
}
void chords_set(int field,int value){pthread_mutex_lock(&mutex);
 if(field==CHORD_OCTAVE)state.octave=clamp(value,-2,2);
 else if(field==CHORD_INVERSION)state.inversion=clamp(value,-3,3);
 else if(field==CHORD_ARP)state.arp=clamp(value,0,ARP_PATTERNS-1);
 else if(field==CHORD_ARP_RATE)state.arp_rate=clamp(value,0,ARP_RATES-1);
 else if(field==CHORD_ARP_RANGE)state.arp_range=clamp(value,1,2);
 else if(field==CHORD_REPEAT)state.repeat=!!value;
 else if(field==CHORD_REPEAT_RATE)state.repeat_rate=clamp(value,0,ARP_RATES-1);
 else if(field==CHORD_LATCH){state.latch=!!value;if(!state.latch){state.types=(state.held_params>>12)&15;state.extensions=(state.held_params>>8)&15;}}
 changed();pthread_mutex_unlock(&mutex);
}
void chords_reset(void){pthread_mutex_lock(&mutex);state.types=1;state.extensions=0;state.octave=state.inversion=0;changed();pthread_mutex_unlock(&mutex);}
int chords_parameter(unsigned pad,int down){
 if(pad<8||pad>=16)return 0;
 unsigned bit=1u<<pad;pthread_mutex_lock(&mutex);
 if(!state.enabled&&!(owned_params&bit)){pthread_mutex_unlock(&mutex);return 0;}
 if(down){
  if(!(owned_params&bit)){
   if(pad>=12){unsigned held=(state.held_params>>12)&15;state.types=held|(1u<<(pad-12));}
   else if(state.latch)state.extensions^=1u<<(pad-8);
   state.held_params|=bit;
   owned_params|=bit;
  }
 }else {state.held_params&=~bit;owned_params&=~bit;}
 if(!state.latch){state.types=(state.held_params>>12)&15;state.extensions=(state.held_params>>8)&15;}
 changed();pthread_mutex_unlock(&mutex);return 1;
}
void chords_roots(unsigned held,int last){pthread_mutex_lock(&mutex);state.held_roots=held&255;if(last>=0&&last<8)state.last_root=last;changed();pthread_mutex_unlock(&mutex);}
int chords_degree(int scale,unsigned degree){
 unsigned mask=masks[scale>=0&&scale<8?scale:0],count=0;
 for(unsigned i=0;i<12;++i)count+=(mask>>i)&1;
 unsigned octave=degree/count,target=degree%count;
 for(int i=0;i<12;++i)if(mask&(1u<<i)){if(!target--)return (int)octave*12+i;}
 return 0;
}
int chords_quality(unsigned types){if((types&3)==3)return 4;for(int i=3;i>=0;--i)if(types&(1u<<i))return i;return 5;}
const char*chords_quality_name(unsigned types){static const char*names[]={"Maj","Min","Dim","Aug","Sus4","Single"};return names[chords_quality(types)];}
static void sort(int *p,int n){for(int i=1;i<n;++i){int v=p[i],j=i;while(j&&p[j-1]>v){p[j]=p[j-1];--j;}p[j]=v;}}
int chords_make(const struct chord_config*c,unsigned root,int out[CHORD_NOTES]){
 if(!c||!c->valid||root>=8)return 0;
 int quality=chords_quality(c->types),interval[CHORD_NOTES]={0},n=1;
 if(quality==5&&c->extensions)quality=0;
 if(quality!=5){static const int thirds[]={4,3,3,4,5},fifths[]={7,7,6,8,7};interval[n++]=thirds[quality];interval[n++]=fifths[quality];}
 static const int extensions[]={9,10,11,14};for(int i=0;i<4;++i)if(c->extensions&(1u<<i))interval[n++]=extensions[i];
 sort(interval,n);
 for(int i=0;i<c->inversion;++i){interval[0]+=12;sort(interval,n);}
 for(int i=0;i>c->inversion;--i){interval[n-1]-=12;sort(interval,n);}
 int offset=(c->tonic-c->root-c->semi)%12;if(offset<0)offset+=12;
 offset+=12*c->octave+chords_degree(c->scale,root);
 int count=0;
 for(int i=0;i<n;++i){int note=offset+interval[i];if(note < -120||note>120)return 0;
  if(!count||out[count-1]!=note)out[count++]=note;}
 return count;
}
int chords_transition(struct chord_voices*v,const struct chord_config*c,unsigned root,int down,int out[CHORD_NOTES]){
 if(root>=8)return 0;
 unsigned bit=1u<<root;int count=0;
 if(down){
  if(v->held&bit)return 0;
  int notes[CHORD_NOTES],n=chords_make(c,root,notes);if(!n)return 0;
  v->held|=bit;v->count[root]=(unsigned char)n;
  for(int i=0;i<n;++i){int note=notes[i];v->notes[root][i]=(signed char)note;if(!v->references[note+120]++)out[count++]=note;}
 }else if(v->held&bit){
  v->held&=~bit;
  for(unsigned i=0;i<v->count[root];++i){int note=v->notes[root][i];if(v->references[note+120]&&!--v->references[note+120])out[count++]=note;}
  v->count[root]=0;
 }
 return count;
}
int chords_light(unsigned pad,uint64_t*out){
 if(pad>=16||!chords_active())return 0;
 struct chord_config c;chords_get(&c);
 uint32_t colour=pad<8?0x00e060:pad<12?0xffb830:PAD_SELECT;
 unsigned selected=pad<8?c.held_roots&(1u<<pad):pad<12?c.extensions&(1u<<(pad-8)):c.types&(1u<<(pad-12));
 float strength=selected?.85f:pad<8?.28f:.17f;if(!c.valid&&pad<8)strength=.06f;
 uint32_t bits;memcpy(&bits,&strength,4);*out=((uint64_t)bits<<32)|colour;return 1;
}
const char *const chords_arp_names[ARP_PATTERNS]={"Off","Up","Down","Up-Down","Up-Down+","Random","As Played","Chord"};
const char *const chords_arp_rate_names[ARP_RATES]={"1/4","1/8","1/8T","1/16","1/16T","1/32"};
const unsigned chords_arp_rate_ticks[ARP_RATES]={960,480,320,240,160,120};
int chords_arp_step(int pattern,int range,const int*notes,int n,unsigned*step,unsigned*seed,int out[CHORD_NOTES*2]){
 if(!notes||n<=0||n>CHORD_NOTES*2||pattern<=ARP_OFF||pattern>=ARP_PATTERNS||!step)return 0;
 range=range==2?2:1;
 if(pattern==ARP_CHORD){for(int i=0;i<n;++i)out[i]=notes[i];++*step;return n;}
 int pool[CHORD_NOTES*4],count=0;
 for(int o=0;o<range;++o)for(int i=0;i<n;++i)pool[count++]=notes[i]+12*o;
 if(pattern!=ARP_AS_PLAYED){
  for(int i=1;i<count;++i){int v=pool[i],j=i;while(j&&pool[j-1]>v){pool[j]=pool[j-1];--j;}pool[j]=v;}
 }
 unsigned s=*step,index;
 switch(pattern){
  case ARP_DOWN:index=(unsigned)count-1-s%(unsigned)count;break;
  case ARP_UP_DOWN:{unsigned period=count>1?2u*(unsigned)count-2u:1u,p=s%period;index=p<(unsigned)count?p:period-p;break;}
  case ARP_UP_DOWN_ENDS:{unsigned period=2u*(unsigned)count,p=s%period;index=p<(unsigned)count?p:period-1u-p;break;}
  case ARP_RANDOM:{unsigned x=seed&&*seed?*seed:0x9e3779b9u;x^=x<<13;x^=x>>17;x^=x<<5;if(seed)*seed=x;index=x%(unsigned)count;break;}
  default:index=s%(unsigned)count;break; /* Up, As Played */
 }
 out[0]=pool[index];++*step;return 1;
}
