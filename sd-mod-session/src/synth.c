#include "synth.h"
#include <math.h>
#include <stdatomic.h>
#include <string.h>
#define VOICES 8
#define RATE 44100.0f
enum { ENV_OFF, ENV_ATTACK, ENV_DECAY, ENV_SUSTAIN, ENV_RELEASE };
struct voice { unsigned note,env;float phase,inc,level,velocity,lp;unsigned long age; };
static struct voice voices[VOICES];
static unsigned long clock_age;
static _Atomic unsigned wave,cutoff=100,attack=2,decay=40,sustain=90,release_cc=40,gain=100,midi_channel=15;
static _Atomic unsigned *const params[AM_SYNTH_PARAMS]={&wave,&cutoff,&attack,&decay,&sustain,&release_cc,&gain,&midi_channel};
static _Atomic int active_voices;
/* MIDI thread pushes, audio thread pops: single producer, single consumer. */
#define QUEUE 64u
static _Atomic unsigned event_queue[QUEUE];static _Atomic unsigned event_head,event_tail;
static void push(unsigned value){unsigned h=atomic_load_explicit(&event_head,memory_order_relaxed);
 if(h-atomic_load_explicit(&event_tail,memory_order_acquire)>=QUEUE)return;
 atomic_store_explicit(&event_queue[h%QUEUE],value,memory_order_relaxed);atomic_store_explicit(&event_head,h+1,memory_order_release);}
void am_synth_note(unsigned note,unsigned velocity,int on){if(note>127)return;push((on&&velocity?1u:0u)<<16|(velocity&127)<<8|note);}
void am_synth_cc(unsigned cc,unsigned value){push(2u<<16|(value&127)<<8|(cc&127));}
void am_synth_midi(const unsigned char *d,unsigned n){
 if(!d||n<2||(d[0]&0x0f)!=atomic_load(&midi_channel))return;
 switch(d[0]&0xf0){
  case 0x90:am_synth_note(d[1],n>2?d[2]:0,n>2&&d[2]);break;
  case 0x80:am_synth_note(d[1],0,0);break;
  case 0xb0:if(n>2)am_synth_cc(d[1],d[2]);break;
 }
}
int am_synth_active(void){return atomic_load_explicit(&active_voices,memory_order_acquire)>0||atomic_load_explicit(&event_head,memory_order_acquire)!=atomic_load_explicit(&event_tail,memory_order_acquire);}
static float freq(unsigned note){return 440.0f*powf(2.0f,((float)note-69.0f)/12.0f);}
static void start(unsigned note,unsigned velocity){
 struct voice *pick=&voices[0];
 for(unsigned i=0;i<VOICES;++i){struct voice *v=&voices[i];if(v->env==ENV_OFF){pick=v;break;}if(v->age<pick->age)pick=v;}
 memset(pick,0,sizeof *pick);pick->note=note;pick->env=ENV_ATTACK;pick->inc=freq(note)/RATE;pick->velocity=(float)velocity/127.0f;pick->age=++clock_age;
}
static void stop(unsigned note){for(unsigned i=0;i<VOICES;++i)if(voices[i].env!=ENV_OFF&&voices[i].env!=ENV_RELEASE&&voices[i].note==note)voices[i].env=ENV_RELEASE;}
static void drain(void){
 unsigned t=atomic_load_explicit(&event_tail,memory_order_relaxed),h=atomic_load_explicit(&event_head,memory_order_acquire);
 for(;t!=h;++t){unsigned e=atomic_load_explicit(&event_queue[t%QUEUE],memory_order_relaxed),kind=e>>16,b=(e>>8)&127,a=e&127;
  if(kind==1)start(a,b);else if(kind==0)stop(a);
  else switch(a){case 70:atomic_store(&wave,b%4);break;case 71:atomic_store(&cutoff,b);break;case 72:atomic_store(&release_cc,b);break;
   case 73:atomic_store(&attack,b);break;case 75:atomic_store(&decay,b);break;case 76:atomic_store(&sustain,b);break;case 7:atomic_store(&gain,b);break;
   case 123:case 120:for(unsigned i=0;i<VOICES;++i)if(voices[i].env!=ENV_OFF)voices[i].env=ENV_RELEASE;break;}
 }
 atomic_store_explicit(&event_tail,t,memory_order_release);
}
static float seconds(unsigned cc,float low,float high){float x=(float)cc/127.0f;return low*powf(high/low,x);}
void am_synth_render_add(float *left,float *right,unsigned frames){
 drain();
 unsigned w=atomic_load(&wave);
 float att=1.0f/(RATE*seconds(atomic_load(&attack),0.002f,2.0f)),dec=1.0f/(RATE*seconds(atomic_load(&decay),0.01f,3.0f));
 float rel=1.0f/(RATE*seconds(atomic_load(&release_cc),0.01f,4.0f)),sus=(float)atomic_load(&sustain)/127.0f;
 float cut=atomic_load(&cutoff)/127.0f,coefficient=0.02f+0.98f*cut*cut,out=(float)atomic_load(&gain)/127.0f*0.25f;
 int alive=0;
 for(unsigned i=0;i<VOICES;++i){struct voice *v=&voices[i];if(v->env==ENV_OFF)continue;alive=1;
  for(unsigned f=0;f<frames;++f){
   switch(v->env){
    case ENV_ATTACK:v->level+=att;if(v->level>=1.0f){v->level=1.0f;v->env=ENV_DECAY;}break;
    case ENV_DECAY:v->level-=dec;if(v->level<=sus){v->level=sus;v->env=ENV_SUSTAIN;}break;
    case ENV_RELEASE:v->level-=rel;if(v->level<=0.0f){v->level=0.0f;v->env=ENV_OFF;}break;
    default:break;
   }
   if(v->env==ENV_OFF)break;
   v->phase+=v->inc;if(v->phase>=1.0f)v->phase-=1.0f;
   float s=w==0?sinf(6.2831853f*v->phase):w==1?2.0f*v->phase-1.0f:w==2?(v->phase<0.5f?1.0f:-1.0f):(v->phase<0.5f?4.0f*v->phase-1.0f:3.0f-4.0f*v->phase);
   v->lp+=coefficient*(s-v->lp);
   float x=v->lp*v->level*v->velocity*out;left[f]+=x;if(right)right[f]+=x;
  }
 }
 atomic_store_explicit(&active_voices,alive,memory_order_release);
}
unsigned am_synth_param_get(unsigned index){return index<AM_SYNTH_PARAMS?atomic_load(params[index]):0;}
void am_synth_param_set(unsigned index,unsigned value){if(index<AM_SYNTH_PARAMS)atomic_store(params[index],index==0?value%4:index==7?value%16:value>127?127:value);}
