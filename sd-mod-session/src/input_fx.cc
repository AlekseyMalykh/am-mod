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
namespace {
/* One process-lifetime service: project changes swap programs through the
 * existing SPSC exchange, never freeing objects borrowed by the input owner. */
std::atomic<fx_exchange*> exchange{nullptr};
std::atomic<unsigned> active{0},seen_rate{0},state{0},publishing{0};
std::atomic<uint64_t> published{0},consumed{0},flush{0};
std::atomic_flag processing=ATOMIC_FLAG_INIT;
fx_native_context context{};
fx_backend backend{};
fx_chain_config configured{};
bool configured_valid=false;
uint64_t generation=0,flushed=0;
int source=-99;
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
 auto*x=exchange.load(std::memory_order_acquire);if(x)fx_exchange_collect(x);
 if(!image||(music_fx_mode_active&&!music_fx_mode_active())){active.store(0,std::memory_order_release);state.store(0);return;}
 unsigned rate=seen_rate.load(std::memory_order_acquire);
 if(!rate){state.store(1);return;}
 mixer_project model;uint64_t revision;unsigned archive_mode;
 if(!mixer_archive_snapshot(&model,&revision,&archive_mode)||archive_mode==MIXER_ARCHIVE_OPAQUE||mixer_archive_loading()){
  active.store(0,std::memory_order_release);state.store(1);return;
 }
 (void)revision;
 if(!x){x=fx_exchange_create();if(!x){state.store(3);return;}exchange.store(x,std::memory_order_release);}
 fx_chain_config next{};if(!model.input.fx_bypass)next=model.input.fx;
 uint64_t next_generation=mixer_archive_generation(),next_flush=flush.load(std::memory_order_acquire);
 int next_source=rear_native_source?rear_native_source():0;
 if(configured_valid&&generation==next_generation&&source==next_source&&flushed==next_flush&&
    !std::memcmp(&configured,&next,sizeof next)){active.store(1,std::memory_order_release);return;}
 if(consumed.load(std::memory_order_acquire)!=published.load(std::memory_order_relaxed))return;
 mixer_native_binding binding{};
 if(next.count){
  if(!mixer_live_native_binding(&binding)||!binding.clock_valid||!fx_native_application_formats(image,&context)){active.store(0,std::memory_order_release);state.store(1);return;}
  context.bpm=0;std::memcpy(&context.bpm,binding.position,8);
  std::memcpy(&context.numerator,binding.position+8,4);std::memcpy(&context.denominator,binding.position+12,4);
  std::memcpy(context.initial_position,binding.position,88);context.position=clock;context.position_user=nullptr;
  fx_native_backend(&context,&backend);
 }
 fx_program*program=fx_program_prepare(&next,rate,next.count?&backend:nullptr);
 if(!program){active.store(0,std::memory_order_release);state.store(3);return;}
 publishing.store(1,std::memory_order_release);
 uint64_t version=published.fetch_add(1,std::memory_order_acq_rel)+1;
 if(fx_exchange_publish(x,program)!=FX_OK){published.store(version-1,std::memory_order_release);publishing.store(0,std::memory_order_release);fx_program_destroy(program);return;}
 publishing.store(0,std::memory_order_release);
 configured=next;configured_valid=true;
 generation=next_generation;source=next_source;flushed=next_flush;active.store(1,std::memory_order_release);state.store(2);
}
extern "C" int input_fx_audio(void*buffer,unsigned rate){
 if(!buffer||rate<8000||rate>192000)return 0;
 int channels,frames;std::memcpy(&channels,buffer,4);std::memcpy(&frames,(char*)buffer+4,4);
 float**data;std::memcpy(&data,(char*)buffer+16,sizeof data);
 if(channels<1||channels>64||frames<1||frames>8192||!data||!data[0]||(channels>1&&!data[1]))return 0;
 seen_rate.store(rate,std::memory_order_release);
 auto*x=exchange.load(std::memory_order_acquire);
 if(!x||!active.load(std::memory_order_acquire)||(music_fx_mode_active&&!music_fx_mode_active()))return 0;
 if(processing.test_and_set(std::memory_order_acquire))return 0;
 bool clear=*((unsigned char*)buffer+0x128)&1;
 for(int n=0;n<frames;++n)for(unsigned ch=0;ch<2;++ch){
  float sample=clear?0.f:data[channels==1?0:ch][n];if(!std::isfinite(sample))sample=0;
  original[n*2+ch]=pcm[n*2+ch]=sample;
 }
 uint64_t version=published.load(std::memory_order_acquire);
 ++input_block;
 unsigned slot=monitor_slot.load(std::memory_order_acquire);
 if(slot<4){std::memset(&monitor_work,0,sizeof monitor_work);monitor_work.channel=9;monitor_work.slot=slot;
  fx_exchange_process_tapped(x,pcm,(unsigned)frames,slot,tap,nullptr);
  unsigned write=monitor_write.load(std::memory_order_relaxed),read=monitor_read.load(std::memory_order_acquire);
  if(write-read<2){monitor_slots[write%2]=monitor_work;monitor_write.store(write+1,std::memory_order_release);}
 }else fx_exchange_process(x,pcm,(unsigned)frames);
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
 if(!publishing.load(std::memory_order_acquire)&&!fx_exchange_transitioning_audio(x)&&fx_exchange_ready_audio(x))consumed.store(version,std::memory_order_release);
 processing.clear(std::memory_order_release);return 1;
}
