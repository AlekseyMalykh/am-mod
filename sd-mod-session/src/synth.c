#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <time.h>
#include "synth.h"
#include "music_settings.h"
extern unsigned input_fx_mode(void) __attribute__((weak));
extern void input_fx_set_mode(unsigned) __attribute__((weak));
#include <math.h>
#include <stdatomic.h>
#include <string.h>
#define VOICES 8
#define RATE 44100.0f
enum { ENV_OFF, ENV_ATTACK, ENV_DECAY, ENV_SUSTAIN, ENV_RELEASE };
struct voice { unsigned note,env;float phase,inc,level,velocity,lp;unsigned long age; };
static struct voice voices[VOICES];
static unsigned long clock_age;
static _Atomic unsigned mic_on,fader_mode,direct_out=1;static _Atomic int used; /* used: a note was played; the MPC microphone is then muted unless Mic is On */
static _Atomic unsigned wave,cutoff=100,attack=2,decay=40,sustain=90,release_cc=40,gain=100,midi_channel=15;
static _Atomic unsigned *const params[AM_SYNTH_PARAMS]={&wave,&cutoff,&attack,&decay,&sustain,&release_cc,&gain,&midi_channel,&mic_on,&fader_mode,&direct_out};
static _Atomic int active_voices;
static _Atomic float st_peak,st_env_level,st_bend,st_mod;static _Atomic unsigned st_env_stage,st_voices,st_notes[4]; /* for the Mixer illustration */
static float bend_semitones,mod_depth,lfo_phase; /* audio thread only: pitch wheel +-2 semitones, mod wheel -> vibrato */
/* MIDI thread pushes, audio thread pops: single producer, single consumer. */
#define QUEUE 64u
static _Atomic unsigned event_queue[QUEUE];static _Atomic unsigned event_head,event_tail;
static void push(unsigned value){unsigned h=atomic_load_explicit(&event_head,memory_order_relaxed);
 if(h-atomic_load_explicit(&event_tail,memory_order_acquire)>=QUEUE)return;
 atomic_store_explicit(&event_queue[h%QUEUE],value,memory_order_relaxed);atomic_store_explicit(&event_head,h+1,memory_order_release);}
static unsigned long long now_ms(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (unsigned long long)t.tv_sec*1000ull+(unsigned long long)t.tv_nsec/1000000ull;}
static _Atomic unsigned long long last_note_ms;
void am_synth_note(unsigned note,unsigned velocity,int on){if(note>127)return;if(on&&velocity){if(input_fx_set_mode&&input_fx_mode&&input_fx_mode()!=2)input_fx_set_mode(2); /* playing the synth selects it as the Input track input */atomic_store(&used,1);atomic_store(&last_note_ms,now_ms());}push((on&&velocity?1u:0u)<<16|(velocity&127)<<8|note);}
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
 {float pk=0.0f,el=0.0f;unsigned es=0,vc=0;unsigned long newest=0;uint32_t nb[4]={0,0,0,0};
  for(unsigned f=0;f<frames;++f){float x=left[f]<0?-left[f]:left[f];if(x>pk)pk=x;}
  for(unsigned i=0;i<VOICES;++i){struct voice *v=&voices[i];if(v->env==ENV_OFF)continue;++vc;if(v->env!=ENV_RELEASE)nb[v->note>>5]|=1u<<(v->note&31);if(v->age>=newest){newest=v->age;el=v->level;es=v->env;}}
  float old=atomic_load(&st_peak);atomic_store(&st_peak,pk>old*0.9f?pk:old*0.9f);
  atomic_store(&st_env_level,el);atomic_store(&st_env_stage,es);atomic_store(&st_voices,vc);
  atomic_store(&st_bend,bend_semitones);atomic_store(&st_mod,mod_depth);
  for(unsigned i=0;i<4;++i)atomic_store(&st_notes[i],nb[i]);}
 lfo_phase+=(float)frames*5.5f/RATE;if(lfo_phase>=1.0f)lfo_phase-=(float)(int)lfo_phase;
 atomic_store_explicit(&active_voices,alive,memory_order_release);
}
unsigned am_synth_param_get(unsigned index){return index<AM_SYNTH_PARAMS?atomic_load(params[index]):0;}
void am_synth_param_set(unsigned index,unsigned value){if(index<AM_SYNTH_PARAMS)atomic_store(params[index],index==0?value%4:index==7?value%16:index==8?value%2:index==9?value%3:index==10?value%2:value>127?127:value);}
int am_synth_direct_out(void){return atomic_load(&direct_out)!=0;}

/* The microphone is muted only while the synth is being played (10 s after the last note), so the mic works again by itself. */
int am_synth_mic_muted(void){return atomic_load_explicit(&used,memory_order_acquire)&&!atomic_load_explicit(&mic_on,memory_order_acquire)&&(am_synth_active()||now_ms()-atomic_load(&last_note_ms)<10000ull);}

extern void music_get_key(int*,int*) __attribute__((weak));
extern uint64_t music_light_snapshot(const struct music_sample*,int) __attribute__((weak));
extern int music_layout_pad_pitch(uint64_t,unsigned,int,int*) __attribute__((weak));
extern uint64_t music_pitch_light(uint64_t,int,int*) __attribute__((weak));
extern uint32_t music_light_colour_rgb(unsigned) __attribute__((weak));
/* 16 Levels of the synth = the project's own engine (music_settings.c): same colours, roles, layouts.
 * Key (tonic) and scale stay; Transpose moves the pads along the notes (Degrees: scale steps, others: semitones)
 * and the lights follow the notes. layout 0 Mono 1 Piano 2 Scale 3 Degrees; scale 0 Project, 1..8 = project scales. */
static _Atomic unsigned cfg_root,cfg_transpose=24,cfg_layout,cfg_scale,cfg_octave=3;
static unsigned char cfg_cols[6]={5,1,10,4,8,3}; /* project colour slots: tonic, scale, other, original, V, IV */
uint64_t am_synth_pad_info(unsigned pad,unsigned*note_out){
 if(pad>=16||!music_light_snapshot)return 0;
 unsigned lay=atomic_load(&cfg_layout)%4,rr=atomic_load(&cfg_root)%13,sc=atomic_load(&cfg_scale)%9,tr=atomic_load(&cfg_transpose);
 int pt=0,ps=0;if(music_get_key)music_get_key(&pt,&ps);
 if(pt<0||pt>11)pt=0;
 if(ps<0||ps>7)ps=0;
 int root=rr?(int)rr-1:pt;
 struct music_sample m;memset(&m,0,sizeof m);
 m.root=root;m.mode=lay==0?MUSIC_MONO:lay==1?MUSIC_PIANO:lay==2?MUSIC_SCALE:MUSIC_DEGREES;
 m.transpose=(int)(tr>48?24:tr)-24;m.tonic=root;m.scale=sc?(int)sc-1:ps;
 for(unsigned i=0;i<3;++i)m.colour[i]=cfg_cols[i];
 m.original_colour=cfg_cols[3];m.fifth_colour=cfg_cols[4];m.fourth_colour=cfg_cols[5];
 uint64_t v=music_light_snapshot(&m,0);
 int pitch=(int)pad;
 if(v&&music_layout_pad_pitch)music_layout_pad_pitch(v,pad,0,&pitch);
 int n=12*(int)(atomic_load(&cfg_octave)+1)+root+pitch;n=n<0?0:n>127?127:n;
 if(note_out)*note_out=(unsigned)n;
 if(!v)return 0;
 int rep=0;uint64_t l=lay&&music_pitch_light?music_pitch_light(v,pitch,&rep):0;
 if(!rep&&music_light_colour_rgb){ /* Monochrome: only the original pad is marked */
  float b=pitch==0?.75f:.10f;uint32_t bits;memcpy(&bits,&b,sizeof bits);
  l=(uint64_t)(music_light_colour_rgb(cfg_cols[3])&0xffffffu)|((uint64_t)bits<<32);
 }
 return l;
}
/* Physical fader on the Mixer synth page: Off / Mod wheel / Pitch wheel (param "Fader"). */
void am_synth_fader(unsigned value){
 unsigned mode=atomic_load(&fader_mode);value&=127u;
 if(mode==1)am_synth_cc(1,value);
 else if(mode==2){unsigned v14=value*16383u/127u;push(3u<<16|((v14>>7)&127u)<<8|(v14&127u));}
}

int am_synth_peek(struct am_synth_state*out){
 if(!out)return 0;
 memset(out,0,sizeof *out);
 if(!am_synth_active())return 0;
 out->peak=atomic_load(&st_peak);out->env_level=atomic_load(&st_env_level);out->bend=atomic_load(&st_bend);out->mod=atomic_load(&st_mod);
 out->env_stage=atomic_load(&st_env_stage);out->voices=atomic_load(&st_voices);
 for(unsigned i=0;i<4;++i)out->notes[i]=atomic_load(&st_notes[i]);
 return 1;
}

/* Pad keyboard outside the Mixer (Sample Record screen): 16 Levels layout for the synth. */
static _Atomic int keys_on;static _Atomic unsigned pad_mask;static unsigned char pad_note[16];
void am_synth_keys_get_cfg(unsigned*l,unsigned*r,unsigned*sc,unsigned*o,unsigned*t,unsigned char*cols){if(l)*l=atomic_load(&cfg_layout);if(r)*r=atomic_load(&cfg_root);if(sc)*sc=atomic_load(&cfg_scale);if(o)*o=atomic_load(&cfg_octave);if(t)*t=atomic_load(&cfg_transpose);if(cols)memcpy(cols,cfg_cols,6);}
void am_synth_keys_cfg(unsigned layout,unsigned root,unsigned scale,unsigned octave,unsigned transpose,const unsigned char*cols){if(cols)memcpy(cfg_cols,cols,6);atomic_store(&cfg_transpose,transpose);atomic_store(&cfg_layout,layout);atomic_store(&cfg_root,root);atomic_store(&cfg_scale,scale);atomic_store(&cfg_octave,octave);}
int am_synth_keys_get(void){return atomic_load(&keys_on);}
void am_synth_pad(unsigned pad,int down){
 if(pad>=16)return;
 unsigned bit=1u<<pad;
 if(down){
  if(atomic_load(&pad_mask)&bit)am_synth_note(pad_note[pad],0,0);
  unsigned n=60;am_synth_pad_info(pad,&n);
  pad_note[pad]=(unsigned char)n;
  atomic_fetch_or(&pad_mask,bit);
  am_synth_note(n,100,1);
 }else if(atomic_load(&pad_mask)&bit){
  atomic_fetch_and(&pad_mask,~bit);
  am_synth_note(pad_note[pad],0,0);
 }
}
void am_synth_keys_set(int on){
 atomic_store(&keys_on,on?1:0);
 if(on&&input_fx_set_mode&&input_fx_mode&&input_fx_mode()!=2)input_fx_set_mode(2); /* pad keyboard = play the synth through the Input track */
 if(!on){
  for(unsigned i=0;i<16;++i){if(atomic_load(&pad_mask)&(1u<<i))am_synth_pad(i,0);}
 }
}
