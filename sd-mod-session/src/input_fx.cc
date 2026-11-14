#include "input_fx.h"
#include "mixer_archive.h"
#include "mixer_live.h"
#include "fx_native.h"
#include <atomic>
#include <cmath>
#include <cstring>
#include <algorithm>
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
std::atomic<float> gain_track[2]{{1.f},{1.f}},gain_master[2]{{1.f},{1.f}};
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
 fx_program*program=fx_program_prepare(&next,rate,next.count?&L.backend:nullptr);
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
extern "C" int input_fx_monitor(unsigned slot,mixer_fx_monitor*out){
 monitor_slot.store(slot<4?slot:~0u,std::memory_order_release);
 if(!out)return 0;
 unsigned read=monitor_read.load(std::memory_order_relaxed),end=monitor_write.load(std::memory_order_acquire);int found=0;
 while(read!=end){const auto&value=monitor_slots[read%2];if(value.slot==slot){*out=value;found=1;}++read;}
 monitor_read.store(read,std::memory_order_release);return found;
}
extern "C" void input_fx_flush(void){flush.fetch_add(1,std::memory_order_release);}
extern "C" const char*input_fx_status(void){
 static const char*labels[]={"Input FX off","Input FX waiting for audio","Mic / USB / Rear: Input FX ready","Input FX unavailable: dry input"};
 unsigned s=state.load(std::memory_order_acquire);return labels[s<4?s:3];
}
extern "C" void input_fx_ui_tick(uintptr_t image){
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
}
extern "C" int input_fx_audio(void*buffer,unsigned rate){
 if(!buffer||rate<8000||rate>192000)return 0;
 int channels,frames;std::memcpy(&channels,buffer,4);std::memcpy(&frames,(char*)buffer+4,4);
 float**data;std::memcpy(&data,(char*)buffer+16,sizeof data);
 if(channels<1||channels>64||frames<1||frames>8192||!data||!data[0]||(channels>1&&!data[1]))return 0;
 seen_rate.store(rate,std::memory_order_release);
 auto*x=lanes[0].x.load(std::memory_order_acquire);
 if(!x||!lanes[0].active.load(std::memory_order_acquire)||(music_fx_mode_active&&!music_fx_mode_active()))return 0;
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
 processing.clear(std::memory_order_release);return 1;
}
