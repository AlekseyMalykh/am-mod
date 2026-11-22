#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <time.h>
#include "synth.h"
#include "music_settings.h"
extern unsigned input_fx_mode(void) __attribute__((weak));
extern void input_fx_set_mode(unsigned) __attribute__((weak));
extern int rear_native_use_synth(void) __attribute__((weak));
#include <math.h>
#include <stdatomic.h>
#include <string.h>
#define VOICES 8
#define RATE 44100.0f
enum { ENV_OFF, ENV_ATTACK, ENV_DECAY, ENV_SUSTAIN, ENV_RELEASE };
struct voice { unsigned note,env,fst;float phase[4],phase2[4],subph,fl,inc,ginc,level,velocity,lp[2],svf_low[2],svf_band[2];unsigned long age; };
static unsigned noise_state=22222u;
static float last_inc;
static struct voice voices[VOICES];
static unsigned long clock_age;
static _Atomic unsigned mic_on,fader_mode,direct_out=1;static _Atomic int used; /* used: a note was played; the MPC microphone is then muted unless Mic is On */
static _Atomic unsigned wave,cutoff=100,attack=2,decay=40,sustain=90,release_cc=40,gain=100,midi_channel=15;
static _Atomic unsigned filt_type,reso,env_amt,lfo_rate=40,lfo_depth,lfo_target,unison,detune=30,spread=64,glide,velsens=127; /* Filter/LFO and Voices pages */
static _Atomic unsigned osc2_wave,osc2_mix,osc2_semi=64,sub_level,noise_level,drive,osc2_fine=64,bend_range=21,f_att=2,f_dec=40,f_sus=90,f_rel=40,keytrack,lfo_shape; /* params 22..35 */
static _Atomic unsigned *const params[AM_SYNTH_PARAMS]={&wave,&cutoff,&attack,&decay,&sustain,&release_cc,&gain,&midi_channel,&mic_on,&fader_mode,&direct_out,&filt_type,&reso,&env_amt,&lfo_rate,&lfo_depth,&lfo_target,&unison,&detune,&spread,&glide,&velsens,&osc2_wave,&osc2_mix,&osc2_semi,&sub_level,&noise_level,&drive,&osc2_fine,&bend_range,&f_att,&f_dec,&f_sus,&f_rel,&keytrack,&lfo_shape};
static _Atomic int active_voices;
static _Atomic float st_peak,st_env_level,st_bend,st_mod;static _Atomic unsigned st_env_stage,st_voices,st_notes[4]; /* for the Mixer illustration */
static float lfo2_phase;static float bend_semitones,mod_depth,lfo_phase; /* audio thread only: pitch wheel +-2 semitones, mod wheel -> vibrato */
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
 memset(pick,0,sizeof *pick);pick->note=note;pick->env=ENV_ATTACK;pick->inc=freq(note)/RATE;pick->ginc=(atomic_load(&glide)&&last_inc>0.0f)?last_inc:pick->inc;last_inc=pick->inc;
 for(unsigned k=0;k<4;++k){pick->phase[k]=(float)k*0.173f;pick->phase2[k]=(float)k*0.173f+0.31f;} /* spread the unison phases */
pick->fst=ENV_ATTACK;
pick->velocity=(float)velocity/127.0f;pick->age=++clock_age;
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
static float lfo_wave(unsigned shape,float x){x-=floorf(x);return shape==0?sinf(6.2831853f*x):shape==1?(x<0.5f?4.0f*x-1.0f:3.0f-4.0f*x):shape==2?(x<0.5f?1.0f:-1.0f):1.0f-2.0f*x;}
static float osc_wave(unsigned w,float ph){return w==0?sinf(6.2831853f*ph):w==1?2.0f*ph-1.0f:w==2?(ph<0.5f?1.0f:-1.0f):(ph<0.5f?4.0f*ph-1.0f:3.0f-4.0f*ph);}
static float seconds(unsigned cc,float low,float high){float x=(float)cc/127.0f;return low*powf(high/low,x);}
void am_synth_render_add(float *left,float *right,unsigned frames){
 drain();
 unsigned w=atomic_load(&wave);
 float att=1.0f/(RATE*seconds(atomic_load(&attack),0.002f,2.0f)),dec=1.0f/(RATE*seconds(atomic_load(&decay),0.01f,3.0f));
 float rel=1.0f/(RATE*seconds(atomic_load(&release_cc),0.01f,4.0f)),sus=(float)atomic_load(&sustain)/127.0f;
 float cut=atomic_load(&cutoff)/127.0f,out=(float)atomic_load(&gain)/127.0f*0.25f;
 unsigned ftype=atomic_load(&filt_type)%3;float res=(float)atomic_load(&reso)/127.0f,eamt=(float)atomic_load(&env_amt)/127.0f;
 float lrate=0.1f*powf(200.0f,(float)atomic_load(&lfo_rate)/127.0f),ldepth=(float)atomic_load(&lfo_depth)/127.0f;unsigned ltarget=atomic_load(&lfo_target)%3;
 unsigned nuni=atomic_load(&unison)%4+1;float det=(float)atomic_load(&detune)/127.0f*50.0f,spr=(float)atomic_load(&spread)/127.0f;
 unsigned gl=atomic_load(&glide);float gcoef=gl?1.0f/(RATE*seconds(gl,0.005f,1.0f)*0.35f):1.0f;float vs=(float)atomic_load(&velsens)/127.0f;
 unsigned w2=atomic_load(&osc2_wave)%4;float mix2=(float)atomic_load(&osc2_mix)/127.0f,subl=(float)atomic_load(&sub_level)/127.0f*0.8f,noisel=(float)atomic_load(&noise_level)/127.0f*0.5f;
 float r2=powf(2.0f,((float)(((int)atomic_load(&osc2_semi)*24+63)/127-12)+((float)atomic_load(&osc2_fine)-64.0f)*50.0f/64.0f/100.0f)/12.0f);
 float dr=(float)atomic_load(&drive)/127.0f,gdr=1.0f+dr*8.0f,invg=1.0f/sqrtf(gdr);
 float brange=(float)((atomic_load(&bend_range)*12u+63u)/127u);
 float fatt=1.0f/(RATE*seconds(atomic_load(&f_att),0.002f,2.0f)),fdec=1.0f/(RATE*seconds(atomic_load(&f_dec),0.01f,3.0f)),frel=1.0f/(RATE*seconds(atomic_load(&f_rel),0.01f,4.0f)),fsus=(float)atomic_load(&f_sus)/127.0f;
 float kt=(float)atomic_load(&keytrack)/127.0f;unsigned lshape=atomic_load(&lfo_shape)%4;
 float unorm=1.0f/sqrtf((float)nuni);float uo[4],gL[4],gR[4];
 for(unsigned k=0;k<nuni;++k){float t=nuni>1?((float)k/(float)(nuni-1))*2.0f-1.0f:0.0f;uo[k]=powf(2.0f,t*det/1200.0f);
  float pan=t*spr;float ang=(pan+1.0f)*0.78539816f;gL[k]=cosf(ang)*1.41421356f;gR[k]=sinf(ang)*1.41421356f;}
 float lfo_ph=lfo2_phase;
 int alive=0;float bend=powf(2.0f,bend_semitones*0.5f*brange/12.0f);
 for(unsigned i=0;i<VOICES;++i){struct voice *v=&voices[i];if(v->env==ENV_OFF)continue;alive=1;
  for(unsigned f=0;f<frames;++f){
   switch(v->env){
    case ENV_ATTACK:v->level+=att;if(v->level>=1.0f){v->level=1.0f;v->env=ENV_DECAY;}break;
    case ENV_DECAY:v->level-=dec;if(v->level<=sus){v->level=sus;v->env=ENV_SUSTAIN;}break;
    case ENV_RELEASE:v->level-=rel;if(v->level<=0.0f){v->level=0.0f;v->env=ENV_OFF;}break;
    default:break;
   }
   if(v->env==ENV_OFF)break;
   if(v->env==ENV_RELEASE)v->fst=ENV_RELEASE;
   switch(v->fst){
    case ENV_ATTACK:v->fl+=fatt;if(v->fl>=1.0f){v->fl=1.0f;v->fst=ENV_DECAY;}break;
    case ENV_DECAY:v->fl-=fdec;if(v->fl<=fsus){v->fl=fsus;v->fst=ENV_SUSTAIN;}break;
    case ENV_RELEASE:v->fl-=frel;if(v->fl<0.0f)v->fl=0.0f;break;
    default:break;
   }
   float vib=mod_depth>0.0f?mod_depth*0.03f*sinf(6.2831853f*(lfo_phase+(float)f*5.5f/RATE)):0.0f;
   float lv=ldepth>0.0f?lfo_wave(lshape,lfo_ph+(float)f*lrate/RATE):0.0f;
   v->ginc+=(v->inc-v->ginc)*gcoef;
   float pm=ltarget==0&&ldepth>0.0f?powf(2.0f,lv*ldepth*2.0f/12.0f):1.0f; /* LFO > pitch: up to +-2 semitones */
   float s=0.0f,sl=0.0f,sr=0.0f;
   for(unsigned k=0;k<nuni;++k){
    v->phase[k]+=v->ginc*bend*pm*uo[k]*(1.0f+vib);if(v->phase[k]>=1.0f)v->phase[k]-=1.0f;
    float ph=v->phase[k];
    float o=osc_wave(w,ph);
    if(mix2>0.0f){v->phase2[k]+=v->ginc*bend*pm*uo[k]*r2*(1.0f+vib);if(v->phase2[k]>=1.0f)v->phase2[k]-=(float)(int)v->phase2[k];o=(o+mix2*osc_wave(w2,v->phase2[k]))/(1.0f+mix2);}
    s+=o*unorm;sl+=o*unorm*gL[k];sr+=o*unorm*gR[k];}
   if(subl>0.0f||noisel>0.0f){
    float sb=0.0f,nz=0.0f;
    if(subl>0.0f){v->subph+=v->ginc*bend*pm*0.5f*(1.0f+vib);if(v->subph>=1.0f)v->subph-=1.0f;sb=sinf(6.2831853f*v->subph)*subl;}
    if(noisel>0.0f){noise_state=noise_state*1664525u+1013904223u;nz=((float)(int)noise_state/2147483648.0f)*noisel;}
    sl+=sb+nz;sr+=sb+nz;}
   float cm=cut+eamt*v->fl*(1.0f-cut)+kt*((float)v->note-60.0f)/60.0f*0.5f;if(ltarget==1)cm+=lv*ldepth*0.5f;if(cm<0.0f)cm=0.0f;if(cm>1.0f)cm=1.0f;
   float in2[2]={sl,sr},o2[2];
   float coefficient=0.02f+0.98f*cm*cm,fc=40.0f*powf(250.0f,cm);if(fc>RATE*0.2f)fc=RATE*0.2f;
   float g=2.0f*sinf(3.14159265f*fc/RATE),q=2.0f-1.6f*res,rcomp=1.0f/(1.0f+1.2f*res); /* resonance is capped and level-compensated */
   for(unsigned ch=0;ch<2;++ch){
    if(ftype==0&&res<=0.0f){v->lp[ch]+=coefficient*(in2[ch]-v->lp[ch]);o2[ch]=v->lp[ch];}
    else{
     v->svf_low[ch]+=g*v->svf_band[ch];float high=in2[ch]-v->svf_low[ch]-q*v->svf_band[ch];v->svf_band[ch]+=g*high;
     if(!(fabsf(v->svf_low[ch])<8.0f&&fabsf(v->svf_band[ch])<8.0f))v->svf_low[ch]=v->svf_band[ch]=0.0f;
     o2[ch]=rcomp*(ftype==0?v->svf_low[ch]:ftype==1?high:v->svf_band[ch]*(0.5f+res));}
   }
   float fl=o2[0],fr=o2[1];(void)s;
   if(dr>0.0f){fl=tanhf(fl*gdr)*invg;fr=tanhf(fr*gdr)*invg;}
   float amp=ltarget==2&&ldepth>0.0f?1.0f-ldepth*(0.5f+0.5f*lv):1.0f;
   float vel=1.0f-vs+vs*v->velocity;
   float gain_v=v->level*vel*out*amp;
   if(right){left[f]+=fl*gain_v;right[f]+=fr*gain_v;}else left[f]+=0.5f*(fl+fr)*gain_v;
  }
 }
 {float pk=0.0f,el=0.0f;unsigned es=0,vc=0;unsigned long newest=0;uint32_t nb[4]={0,0,0,0};
  for(unsigned f=0;f<frames;++f){float x=left[f]<0?-left[f]:left[f];if(x>pk)pk=x;}
  for(unsigned i=0;i<VOICES;++i){struct voice *v=&voices[i];if(v->env==ENV_OFF)continue;++vc;if(v->env!=ENV_RELEASE)nb[v->note>>5]|=1u<<(v->note&31);if(v->age>=newest){newest=v->age;el=v->level;es=v->env;}}
  float old=atomic_load(&st_peak);atomic_store(&st_peak,pk>old*0.9f?pk:old*0.9f);
  atomic_store(&st_env_level,el);atomic_store(&st_env_stage,es);atomic_store(&st_voices,vc);
  atomic_store(&st_bend,bend_semitones);atomic_store(&st_mod,mod_depth);
  for(unsigned i=0;i<4;++i)atomic_store(&st_notes[i],nb[i]);}
 lfo2_phase+=(float)frames*lrate/RATE;lfo2_phase-=(float)(int)lfo2_phase;
 lfo_phase+=(float)frames*5.5f/RATE;if(lfo_phase>=1.0f)lfo_phase-=(float)(int)lfo_phase;
 atomic_store_explicit(&active_voices,alive,memory_order_release);
}
unsigned am_synth_param_get(unsigned index){return index<AM_SYNTH_PARAMS?atomic_load(params[index]):0;}
void am_synth_param_set(unsigned index,unsigned value){if(index<AM_SYNTH_PARAMS)atomic_store(params[index],index==0?value%4:index==7?value%16:index==8?value%2:index==9?value%3:index==10?value%2:index==11||index==16?value%3:index==17?value%4:index==22||index==35?value%4:value>127?127:value);}
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
static _Atomic unsigned cfg_root,cfg_transpose=24,cfg_layout=2,cfg_scale,cfg_octave=3;
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
 if(on&&input_fx_mode&&input_fx_mode()!=2){ /* pad keyboard = play the synth through the Input track */
  if(rear_native_use_synth)rear_native_use_synth();else if(input_fx_set_mode)input_fx_set_mode(2);}
 if(!on){
  for(unsigned i=0;i<16;++i){if(atomic_load(&pad_mask)&(1u<<i))am_synth_pad(i,0);}
 }
}
