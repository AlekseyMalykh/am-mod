#include <cstdio>
#include "input_fx.h"
#include "mixer_archive.h"
#include "mixer_live.h"
#include "fx_native.h"
#include <atomic>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <chrono>
extern "C" unsigned music_fx_mode_active(void) __attribute__((weak));
extern "C" int rear_native_source(void) __attribute__((weak));
/* Recording route (Mod Settings > Quick): 0 point (Input/Track/Master), 1 track A-H, 2 wet. */
extern "C" unsigned music_rec_get(unsigned field) __attribute__((weak));
namespace {
/* Recording lanes. Lane 0 is the Input FX chain (always). Lane 1 is the chosen Track
 * and lane 2 the Master chain. Each lane is one process-lifetime service: project changes
 * swap programs through the existing SPSC exchange, never freeing objects borrowed by the
 * input owner. */
struct Lane {
    std::atomic<fx_exchange*> x{nullptr};
    std::atomic<unsigned> active{0},publishing{0};
    std::atomic<uint64_t> published{0},consumed{0};
    fx_native_context context{};
    fx_backend backend{};
    fx_chain_config configured{};
    bool valid=false;
    uint64_t generation=0,flushed=0;
    int source=-99;
};
Lane lanes[3];
std::atomic<unsigned> seen_rate{0},state{0};
std::atomic<uint64_t> flush{0};
std::atomic_flag processing=ATOMIC_FLAG_INIT;
/* Effective route for the audio owner: point 0 Input, 1 Track, 2 Master (wet only). */
std::atomic<unsigned> route_point{0};
std::atomic<float> input_peak[2]{{0.f},{0.f}};
/* Tuner: the audio owner only writes a mono ring; the UI tick runs the pitch detector. */
float tune_ring[8192];
std::atomic<unsigned> tune_write{0};
std::atomic<long long> tune_asked_ms{0};
std::atomic<float> tune_hz{0.f};
long long steady_ms(){return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
/* YIN: cumulative mean normalised difference, first dip under the threshold. */
bool yin_pitch(const float*x,int n,int rate,float*out){
 static float d[2048],c[2048];
 int tmax=std::min(rate/40,2000),tmin=std::max(rate/1000,2),w=n-tmax;
 if(w<512)return false;
 double energy=0;for(int i=0;i<w;++i)energy+=(double)x[i]*x[i];
 if(energy/w<1e-6)return false;
 d[0]=0;
 for(int tau=1;tau<=tmax;++tau){double sum=0;for(int i=0;i<w;++i){double t=(double)x[i]-x[i+tau];sum+=t*t;}d[tau]=(float)sum;}
 double run=0;c[0]=1;
 for(int tau=1;tau<=tmax;++tau){run+=d[tau];c[tau]=run>0?(float)(d[tau]*tau/run):1.f;}
 int best=-1;
 for(int tau=tmin;tau<tmax;++tau)if(c[tau]<.15f){while(tau+1<tmax&&c[tau+1]<c[tau])++tau;best=tau;break;}
 if(best<0)return false;
 float t=(float)best;
 if(best>1&&best<tmax-1){float a=c[best-1],b=c[best],e=c[best+1],den=a-2*b+e;if(den!=0)t+=.5f*(a-e)/den;}
 *out=(float)rate/t;return true;
}
void tuner_update(){
 static long long last;static unsigned hold;static float buf[3072];
 long long now=steady_ms();
 if(now-tune_asked_ms.load()>500||now-last<80)return;
 last=now;
 unsigned rate=seen_rate.load(std::memory_order_acquire),w=tune_write.load(std::memory_order_acquire);
 if(!rate)return;
 for(unsigned i=0;i<3072;++i)buf[i]=tune_ring[(w-3072+i)&8191];
 float hz=0;
 if(yin_pitch(buf,3072,(int)rate,&hz)){tune_hz.store(hz);hold=0;}
 else if(++hold>4)tune_hz.store(0.f);
}
std::atomic<float> gain_track[2]{{1.f},{1.f}},gain_master[2]{{1.f},{1.f}},gain_input[2]{{1.f},{1.f}};
float pcm[8192*2],original[8192*2];
thread_local uint64_t input_block;
std::atomic<unsigned> monitor_slot{~0u},monitor_write{0},monitor_read{0};
mixer_fx_monitor monitor_work{},monitor_slots[2]{};
void tap(void*,const float*data,size_t frames,int after,float gain){
    float peak=0;float*out=after?monitor_work.pcm:monitor_work.input_pcm;
    size_t count=std::min<size_t>(frames,MIXER_FX_MONITOR_FRAMES);
    for(size_t n=0;n<count;++n){float l=data[n*2],r=data[n*2+1];if(!std::isfinite(l))l=0;if(!std::isfinite(r))r=0;out[n]=(l+r)*.5f;peak=std::max(peak,std::max(std::fabs(l),std::fabs(r)));}
    if(after){monitor_work.output_peak=peak;monitor_work.gain_reduction_db=gain>0?-20.f*std::log10(gain):120.f;}
    else monitor_work.input_peak=peak;
}
int clock(void*,void*out,uint64_t*block){*block=input_block;return mixer_live_native_input_position(out);}
/* Fader/Pan of a Mixer channel, the same law as the playback buses. */
void channel_gain(const mixer_mix&mix,float out[2]){
    float gain=!mix.mute&&std::isfinite(mix.level_db)?std::pow(10.f,mix.level_db/20.f):0.f;
    out[0]=gain*(mix.pan>0?1-mix.pan:1);out[1]=gain*(mix.pan<0?1+mix.pan:1);
}
/* 0 waiting (unchanged), 1 unavailable (inactive), 2 ready, 3 error. */
int lane_update(Lane&L,const fx_chain_config&next,unsigned rate,uintptr_t image,uint64_t gen,uint64_t fl,int src){
 auto*x=L.x.load(std::memory_order_acquire);
 if(!x){x=fx_exchange_create();if(!x)return 3;L.x.store(x,std::memory_order_release);}
 if(L.valid&&L.generation==gen&&L.source==src&&L.flushed==fl&&!std::memcmp(&L.configured,&next,sizeof next)){L.active.store(1,std::memory_order_release);return 2;}
 if(L.consumed.load(std::memory_order_acquire)!=L.published.load(std::memory_order_relaxed))return 0;
 mixer_native_binding binding{};
 if(next.count){
  if(!mixer_live_native_binding(&binding)||!binding.clock_valid||!fx_native_application_formats(image,&L.context)){L.active.store(0,std::memory_order_release);return 1;}
  L.context.bpm=0;std::memcpy(&L.context.bpm,binding.position,8);
  std::memcpy(&L.context.numerator,binding.position+8,4);std::memcpy(&L.context.denominator,binding.position+12,4);
  std::memcpy(L.context.initial_position,binding.position,88);L.context.position=clock;L.context.position_user=nullptr;
  fx_native_backend(&L.context,&L.backend);
 }
 fx_program*program=fx_program_prepare_delta(&next,rate,next.count?&L.backend:nullptr,
     L.valid&&L.generation==gen&&L.source==src&&L.flushed==fl?&L.configured:nullptr);
 if(!program){L.active.store(0,std::memory_order_release);return 3;}
 L.publishing.store(1,std::memory_order_release);
 uint64_t version=L.published.fetch_add(1,std::memory_order_acq_rel)+1;
 if(fx_exchange_publish(x,program)!=FX_OK){L.published.store(version-1,std::memory_order_release);L.publishing.store(0,std::memory_order_release);fx_program_destroy(program);return 0;}
 L.publishing.store(0,std::memory_order_release);
 L.configured=next;L.valid=true;L.generation=gen;L.source=src;L.flushed=fl;L.active.store(1,std::memory_order_release);
 return 2;
}
bool lane_ready(const Lane&L){return L.x.load(std::memory_order_acquire)&&L.active.load(std::memory_order_acquire);}
void lane_done(Lane&L,uint64_t version){
 auto*x=L.x.load(std::memory_order_acquire);
 if(x&&!L.publishing.load(std::memory_order_acquire)&&!fx_exchange_transitioning_audio(x)&&fx_exchange_ready_audio(x))L.consumed.store(version,std::memory_order_release);
}
}
namespace {
/* A published program is adopted only by the audio owner. Lanes that the current route does not use
 * must still adopt it (on silence), otherwise lane_update() waits for them forever and the route
 * never becomes ready: the Track/Master chain would then never reach the recording. */
float idle_silence[8192*2];
void consume_idle(unsigned frames,bool used0,bool used1,bool used2){
 const bool used[3]={used0,used1,used2};
 for(unsigned i=0;i<3;++i){
  if(used[i])continue;
  Lane&L=lanes[i];auto*x=L.x.load(std::memory_order_acquire);
  if(!x||L.publishing.load(std::memory_order_acquire))continue;
  uint64_t pending=L.published.load(std::memory_order_acquire);
  if(L.consumed.load(std::memory_order_acquire)==pending)continue;
  std::memset(idle_silence,0,(size_t)frames*2*sizeof(float));
  fx_exchange_process(x,idle_silence,frames);
  lane_done(L,pending);
 }
}
}
extern "C" int input_fx_monitor(unsigned slot,mixer_fx_monitor*out){
 monitor_slot.store(slot<4?slot:~0u,std::memory_order_release);
 if(!out)return 0;
 unsigned read=monitor_read.load(std::memory_order_relaxed),end=monitor_write.load(std::memory_order_acquire);int found=0;
 while(read!=end){const auto&value=monitor_slots[read%2];if(value.slot==slot){*out=value;found=1;}++read;}
 monitor_read.store(read,std::memory_order_release);return found;
}
extern "C" int input_fx_pitch(float*hz){tune_asked_ms.store(steady_ms());float v=tune_hz.load();if(hz)*hz=v;return v>0.f;}
extern "C" void input_fx_peak(float*l,float*r){if(l)*l=input_peak[0].load(std::memory_order_relaxed);if(r)*r=input_peak[1].load(std::memory_order_relaxed);}
extern "C" void input_fx_flush(void){flush.fetch_add(1,std::memory_order_release);}
extern "C" const char*input_fx_status(void){
 static const char*labels[]={"Input FX off","Input FX waiting for audio","Mic / USB / Rear: Input FX ready","Input FX unavailable: dry input"};
 unsigned s=state.load(std::memory_order_acquire);return labels[s<4?s:3];
}
extern "C" void input_fx_ui_tick(uintptr_t image){
 tuner_update();
 for(auto&L:lanes){auto*x=L.x.load(std::memory_order_acquire);if(x)fx_exchange_collect(x);}
 if(!image||(music_fx_mode_active&&!music_fx_mode_active())){for(auto&L:lanes){L.active.store(0,std::memory_order_release);}route_point.store(0);state.store(0);return;}
 unsigned rate=seen_rate.load(std::memory_order_acquire);
 if(!rate){state.store(1);return;}
 mixer_project model;uint64_t revision;unsigned archive_mode;
 if(!mixer_archive_snapshot(&model,&revision,&archive_mode)||archive_mode==MIXER_ARCHIVE_OPAQUE||mixer_archive_loading()){
  for(auto&L:lanes){L.active.store(0,std::memory_order_release);}
  route_point.store(0);state.store(1);return;
 }
 (void)revision;
 /* Recording route: point only counts while "wet" (effects recorded) is on. */
 unsigned point=music_rec_get?music_rec_get(0):0,track=music_rec_get?music_rec_get(1):0,wet=music_rec_get?music_rec_get(2):1;
 if(point>2)point=2;
 if(track>=MIXER_TRACKS)track=0;
 unsigned effective=wet?point:0;
 fx_chain_config next[3]{};
 if(wet&&!model.input.fx_bypass)next[0]=model.input.fx;
 if(effective>=1&&!model.track[track].fx_bypass)next[1]=model.track[track].fx;
 if(effective>=2&&!model.master.fx_bypass)next[2]=model.master.fx;
 float g[2];
 channel_gain(model.input.mix,g);gain_input[0].store(g[0]);gain_input[1].store(g[1]);
 channel_gain(model.track[track].mix,g);gain_track[0].store(g[0]);gain_track[1].store(g[1]);
 channel_gain(model.master.mix,g);gain_master[0].store(g[0]);gain_master[1].store(g[1]);
 uint64_t gen=mixer_archive_generation(),fl=flush.load(std::memory_order_acquire);
 int src=rear_native_source?rear_native_source():0;
 int result[3]={0,2,2};
 for(unsigned i=0;i<3;++i){
  if(i&&effective<i){lanes[i].active.store(0,std::memory_order_release);continue;}
  result[i]=lane_update(lanes[i],next[i],rate,image,gen,fl,src);
 }
 /* Lane 0 decides the public status exactly as before. Switch the route only when
  * every lane it needs is ready, so audio never records half a chain. */
 if(result[0]==3||result[1]==3||result[2]==3)state.store(3);
 else if(result[0]==1)state.store(1);
 else if(result[0]==2)state.store(2);
 bool ready=result[0]==2&&(effective<1||result[1]==2)&&(effective<2||result[2]==2);
 route_point.store(ready?effective:0,std::memory_order_release);
 {   /* Diagnostic (console log): one line whenever the recording route changes, at most 40 lines. */
  static unsigned long long last;static unsigned lines;
  unsigned long long sig=((unsigned long long)point<<0)|((unsigned long long)track<<4)|((unsigned long long)wet<<8)|((unsigned long long)effective<<12)|
   ((unsigned long long)ready<<16)|((unsigned long long)result[0]<<20)|((unsigned long long)result[1]<<24)|((unsigned long long)result[2]<<28)|
   ((unsigned long long)next[0].count<<32)|((unsigned long long)next[1].count<<40)|((unsigned long long)next[2].count<<48)|((unsigned long long)state.load()<<56);
  if(sig!=last&&lines<40){last=sig;++lines;
   std::fprintf(stderr,"INPUT_FX route point=%u track=%u wet=%u effective=%u ready=%d result=%d/%d/%d fx=%u/%u/%u bypass=%d/%d/%d state=%u gain_track=%.3f/%.3f src=%d tracks=%u,%u,%u,%u,%u,%u,%u,%u\n",
    point,track,wet,effective,(int)ready,result[0],result[1],result[2],(unsigned)next[0].count,(unsigned)next[1].count,(unsigned)next[2].count,
    (int)model.input.fx_bypass,(int)model.track[track].fx_bypass,(int)model.master.fx_bypass,state.load(),gain_track[0].load(),gain_track[1].load(),src,
    (unsigned)model.track[0].fx.count,(unsigned)model.track[1].fx.count,(unsigned)model.track[2].fx.count,(unsigned)model.track[3].fx.count,
    (unsigned)model.track[4].fx.count,(unsigned)model.track[5].fx.count,(unsigned)model.track[6].fx.count,(unsigned)model.track[7].fx.count);}
 }
}
extern "C" int input_fx_audio(void*buffer,unsigned rate){
 if(!buffer||rate<8000||rate>192000)return 0;
 int channels,frames;std::memcpy(&channels,buffer,4);std::memcpy(&frames,(char*)buffer+4,4);
 float**data;std::memcpy(&data,(char*)buffer+16,sizeof data);
 if(channels<1||channels>64||frames<1||frames>8192||!data||!data[0]||(channels>1&&!data[1]))return 0;
 seen_rate.store(rate,std::memory_order_release);
 {static unsigned calls;if(calls<3&&++calls)std::fprintf(stderr,"INPUT_FX audio call=%u channels=%d frames=%d lane0=%d mode=%d\n",calls,channels,frames,(int)lanes[0].active.load(),music_fx_mode_active?(int)music_fx_mode_active():-1);}
 {   /* Input level for the Mixer strip: raw peak of the selected input, slow release. */
  bool silent_in=*((unsigned char*)buffer+0x128)&1;
  float pl=0,pr=0;
  if(!silent_in)for(int n=0;n<frames;++n){
   float l=data[0][n],r=data[channels>1?1:0][n];
   if(std::isfinite(l))pl=std::max(pl,std::fabs(l));
   if(std::isfinite(r))pr=std::max(pr,std::fabs(r));
  }
  input_peak[0].store(std::max(pl,input_peak[0].load()*.97f),std::memory_order_relaxed);
  input_peak[1].store(std::max(pr,input_peak[1].load()*.97f),std::memory_order_relaxed);
 }
 {   /* Tuner input: mono mix of the selected input. */
  bool silent_in=*((unsigned char*)buffer+0x128)&1;
  unsigned w=tune_write.load(std::memory_order_relaxed);
  for(int n=0;n<frames;++n){
   float l=data[0][n],r=data[channels>1?1:0][n];
   tune_ring[(w+(unsigned)n)&8191]=silent_in||!std::isfinite(l)||!std::isfinite(r)?0.f:(l+r)*.5f;
  }
  tune_write.store(w+(unsigned)frames,std::memory_order_release);
 }
 auto*x=lanes[0].x.load(std::memory_order_acquire);
 if(music_fx_mode_active&&!music_fx_mode_active())return 0;
 if(!x||!lanes[0].active.load(std::memory_order_acquire)){
  if(!processing.test_and_set(std::memory_order_acquire)){consume_idle((unsigned)frames,false,false,false);processing.clear(std::memory_order_release);}
  return 0;
 }
 if(processing.test_and_set(std::memory_order_acquire))return 0;
 bool clear=*((unsigned char*)buffer+0x128)&1;
 for(int n=0;n<frames;++n)for(unsigned ch=0;ch<2;++ch){
  float sample=clear?0.f:data[channels==1?0:ch][n];if(!std::isfinite(sample))sample=0;
  original[n*2+ch]=pcm[n*2+ch]=sample;
 }
 uint64_t versions[3]={lanes[0].published.load(std::memory_order_acquire),lanes[1].published.load(std::memory_order_acquire),lanes[2].published.load(std::memory_order_acquire)};
 unsigned point=route_point.load(std::memory_order_acquire);
 ++input_block;
 unsigned slot=monitor_slot.load(std::memory_order_acquire);
 if(slot<4){std::memset(&monitor_work,0,sizeof monitor_work);monitor_work.channel=9;monitor_work.slot=slot;
  fx_exchange_process_tapped(x,pcm,(unsigned)frames,slot,tap,nullptr);
  unsigned write=monitor_write.load(std::memory_order_relaxed),read=monitor_read.load(std::memory_order_acquire);
  if(write-read<2){monitor_slots[write%2]=monitor_work;monitor_write.store(write+1,std::memory_order_release);}
 }else fx_exchange_process(x,pcm,(unsigned)frames);
 {   /* Input channel fader/pan/mute (Mixer > Input > MIX), after the Input FX chain. */
  float gl=gain_input[0].load(),gr=gain_input[1].load();
  if(gl!=1.f||gr!=1.f)for(int n=0;n<frames;++n){pcm[n*2]*=gl;pcm[n*2+1]*=gr;}
 }
 /* Record through the chosen Track, then optionally through the Master. */
 bool track_used=point>=1&&lane_ready(lanes[1]);
 bool master_used=point>=2&&lane_ready(lanes[2]);
 if(track_used){
  fx_exchange_process(lanes[1].x.load(std::memory_order_acquire),pcm,(unsigned)frames);
  float gl=gain_track[0].load(),gr=gain_track[1].load();
  for(int n=0;n<frames;++n){pcm[n*2]*=gl;pcm[n*2+1]*=gr;}
 }
 if(master_used){
  fx_exchange_process(lanes[2].x.load(std::memory_order_acquire),pcm,(unsigned)frames);
  float gl=gain_master[0].load(),gr=gain_master[1].load();
  for(int n=0;n<frames;++n){pcm[n*2]*=gl;pcm[n*2+1]*=gr;}
 }
 bool valid=true;for(int n=0;n<frames*2;++n)if(!std::isfinite(pcm[n])){valid=false;break;}
 if(!valid){std::memcpy(pcm,original,(size_t)frames*2*sizeof(float));state.store(3,std::memory_order_release);}
 if(clear)for(int ch=2;ch<channels;++ch)if(data[ch])std::memset(data[ch],0,(size_t)frames*sizeof(float));
 bool silent=true;
 for(int n=0;n<frames;++n){
  if(channels==1){data[0][n]=(pcm[n*2]+pcm[n*2+1])*.5f;silent&=data[0][n]==0;}
  else for(unsigned ch=0;ch<2;++ch){data[ch][n]=pcm[n*2+ch];silent&=data[ch][n]==0;}
 }
 /* Additional native channel pairs are left intact. Do not mark a buffer
  * silent when another pair carries data. */
 for(int ch=2;ch<channels&&silent;++ch)if(data[ch])for(int n=0;n<frames&&silent;++n)silent&=data[ch][n]==0;
 *((unsigned char*)buffer+0x128)=silent?1:0;
 lane_done(lanes[0],versions[0]);
 if(track_used)lane_done(lanes[1],versions[1]);
 if(master_used)lane_done(lanes[2],versions[2]);
 consume_idle((unsigned)frames,true,track_used,master_used);
 processing.clear(std::memory_order_release);return 1;
}
