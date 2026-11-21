#include <stdint.h>
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
static _Atomic unsigned mic_on,fader_mode;static _Atomic int used; /* used: a note was played; the MPC microphone is then muted unless Mic is On */
static _Atomic unsigned wave,cutoff=100,attack=2,decay=40,sustain=90,release_cc=40,gain=100,midi_channel=15;
static _Atomic unsigned *const params[AM_SYNTH_PARAMS]={&wave,&cutoff,&attack,&decay,&sustain,&release_cc,&gain,&midi_channel,&mic_on,&fader_mode};
static _Atomic int active_voices;
static float bend_semitones,mod_depth,lfo_phase; /* audio thread only: pitch wheel +-2 semitones, mod wheel -> vibrato */
/* MIDI thread pushes, audio thread pops: single producer, single consumer. */
#define QUEUE 64u
static _Atomic unsigned event_queue[QUEUE];static _Atomic unsigned event_head,event_tail;
static void push(unsigned value){unsigned h=atomic_load_explicit(&event_head,memory_order_relaxed);
 if(h-atomic_load_explicit(&event_tail,memory_order_acquire)>=QUEUE)return;
 atomic_store_explicit(&event_queue[h%QUEUE],value,memory_order_relaxed);atomic_store_explicit(&event_head,h+1,memory_order_release);}
void am_synth_note(unsigned note,unsigned velocity,int on){if(note>127)return;if(on&&velocity)atomic_store(&used,1);push((on&&velocity?1u:0u)<<16|(velocity&127)<<8|note);}
void am_synth_cc(unsigned cc,unsigned value){push(2u<<16|(value&127)<<8|(cc&127));}
void am_synth_midi(const unsigned char *d,unsigned n){
 if(!d||n<2||(d[0]&0x0f)!=atomic_load(&midi_channel))return;
 switch(d[0]&0xf0){
  case 0x90:am_synth_note(d[1],n>2?d[2]:0,n>2&&d[2]);break;
  case 0x80:am_synth_note(d[1],0,0);break;
  case 0xb0:if(n>2)am_synth_cc(d[1],d[2]);break;
  case 0xe0:if(n>2)push(3u<<16|(d[2]&127u)<<8|(d[1]&127u));break;
 }
}
void am_synth_key(const unsigned char *d,unsigned n){
 if(!d||n<3)return;
 switch(d[0]&0xf0){case 0x90:am_synth_note(d[1],d[2],d[2]!=0);break;case 0x80:am_synth_note(d[1],0,0);break;
  case 0xb0:if(d[1]==1)am_synth_cc(1,d[2]);break;
  case 0xe0:push(3u<<16|(d[2]&127u)<<8|(d[1]&127u));break;}
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
  else if(kind==3)bend_semitones=((float)((b<<7)|a)-8192.0f)/8192.0f*2.0f;
  else switch(a){case 70:atomic_store(&wave,b%4);break;case 71:atomic_store(&cutoff,b);break;case 72:atomic_store(&release_cc,b);break;
   case 73:atomic_store(&attack,b);break;case 75:atomic_store(&decay,b);break;case 76:atomic_store(&sustain,b);break;case 7:atomic_store(&gain,b);break;
   case 1:mod_depth=(float)b/127.0f;break;
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
 int alive=0;float bend=powf(2.0f,bend_semitones/12.0f);
 for(unsigned i=0;i<VOICES;++i){struct voice *v=&voices[i];if(v->env==ENV_OFF)continue;alive=1;
  for(unsigned f=0;f<frames;++f){
   switch(v->env){
    case ENV_ATTACK:v->level+=att;if(v->level>=1.0f){v->level=1.0f;v->env=ENV_DECAY;}break;
    case ENV_DECAY:v->level-=dec;if(v->level<=sus){v->level=sus;v->env=ENV_SUSTAIN;}break;
    case ENV_RELEASE:v->level-=rel;if(v->level<=0.0f){v->level=0.0f;v->env=ENV_OFF;}break;
    default:break;
   }
   if(v->env==ENV_OFF)break;
   float vib=mod_depth>0.0f?mod_depth*0.03f*sinf(6.2831853f*(lfo_phase+(float)f*5.5f/RATE)):0.0f;
   v->phase+=v->inc*bend*(1.0f+vib);if(v->phase>=1.0f)v->phase-=1.0f;
   float s=w==0?sinf(6.2831853f*v->phase):w==1?2.0f*v->phase-1.0f:w==2?(v->phase<0.5f?1.0f:-1.0f):(v->phase<0.5f?4.0f*v->phase-1.0f:3.0f-4.0f*v->phase);
   v->lp+=coefficient*(s-v->lp);
   float x=v->lp*v->level*v->velocity*out;left[f]+=x;if(right)right[f]+=x;
  }
 }
 lfo_phase+=(float)frames*5.5f/RATE;if(lfo_phase>=1.0f)lfo_phase-=(float)(int)lfo_phase;
 atomic_store_explicit(&active_voices,alive,memory_order_release);
}
unsigned am_synth_param_get(unsigned index){return index<AM_SYNTH_PARAMS?atomic_load(params[index]):0;}
void am_synth_param_set(unsigned index,unsigned value){if(index<AM_SYNTH_PARAMS)atomic_store(params[index],index==0?value%4:index==7?value%16:index==8?value%2:index==9?value%3:value>127?127:value);}

int am_synth_mic_muted(void){return atomic_load_explicit(&used,memory_order_acquire)&&!atomic_load_explicit(&mic_on,memory_order_acquire);}

static const uint8_t key_scales[7][12]={{0,2,4,5,7,9,11},{0,2,3,5,7,8,10},{0,2,3,5,7,9,10},{0,2,4,5,7,9,10},{0,2,4,7,9},{0,3,5,7,10},{0,3,5,6,7,10}};
static const uint8_t key_scale_len[7]={7,7,7,7,5,5,6};
unsigned am_synth_key_note(unsigned layout,unsigned root,unsigned scale,unsigned octave,unsigned pad){
 unsigned base=12u*(octave+1)+root;int n;
 if(layout==1){unsigned len=key_scale_len[scale%7];n=(int)(base+12*(pad/len)+key_scales[scale%7][pad%len]);}
 else n=(int)(base+pad);
 return n>127?127u:(unsigned)n;
}
/* Physical fader on the Mixer synth page: Off / Mod wheel / Pitch wheel (param "Fader"). */
void am_synth_fader(unsigned value){
 unsigned mode=atomic_load(&fader_mode);value&=127u;
 if(mode==1)am_synth_cc(1,value);
 else if(mode==2){unsigned v14=value*16383u/127u;push(3u<<16|((v14>>7)&127u)<<8|(v14&127u));}
}
