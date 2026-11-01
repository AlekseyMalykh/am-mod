/* Compare actual pinned Duck DSP and the production Opto master adapter.
 * The preset service/playhead are offline fixtures; the stock DSP is real. */
#define modern_fx_probe unused_fx_gain_probe
#include "check_modern_fx.cc"
#undef modern_fx_probe
#include "src/compressor.h"
extern "C" int modern_fx_probe(uintptr_t base,const char**phase){
 native_base=base;*phase="native compressor gain comparison";
 reinterpret_cast<void(*)()>(base+0xc034d0)();
 reinterpret_cast<void(*)(float)>(base+0xd8d320)(48000.f);
 alignas(8) unsigned char format[24]{},description[112]{},presets[256]{};
 uintptr_t preset_vtable[3]={0,0,reinterpret_cast<uintptr_t>(no_preset_directory)};
 *reinterpret_cast<uintptr_t**>(presets)=preset_vtable;
 new(presets+8)std::map<int,int>();new(presets+0x68)std::map<int,int>();void*empty=presets;
 reinterpret_cast<void(*)(void*,void*)>(base+0x2530ee0)(format,&empty);
 *reinterpret_cast<uint32_t*>(description+72)=0x43434d50;
 NativePtr instance;using Callback=std::function<void(NativePtr,const NativeText&)>;
 Callback callback=[&](NativePtr p,const NativeText&){instance=std::move(p);};
 reinterpret_cast<void(*)(void*,const void*,double,int,Callback)>(base+0x2531110)(format,description,48000.,256,std::move(callback));assert(instance);
 auto*v=*reinterpret_cast<uintptr_t**>(instance.get());
 assert(reinterpret_cast<int(*)(void*)>(v[24])(instance.get())==6);
 auto set=[&](int p,float x){reinterpret_cast<void(*)(void*,int,float)>(v[40])(instance.get(),p,x);};
 set(2,0);set(3,0);set(4,0);set(5,0); /* Amount=0, Color off, Bypass off. */
 reinterpret_cast<void(*)(void*,double,int)>(v[4])(instance.get(),48000.,256);
 OfflinePlayHead playhead;*reinterpret_cast<void**>(reinterpret_cast<char*>(instance.get())+16)=&playhead;
 float l[256],r[256],*channels[]={l,r};alignas(8) unsigned char audio[320]{},midi[16]{};
 *reinterpret_cast<int*>(audio)=2;*reinterpret_cast<int*>(audio+4)=256;*reinterpret_cast<float***>(audio+16)=channels;
 auto measure=[&](float input_db){
  double dry=0,wet=0;float amplitude=std::pow(10.f,input_db/20);
  auto table=*reinterpret_cast<uintptr_t**>(instance.get());
  for(unsigned block=0;block<375;++block){
   double energy=0;for(unsigned i=0;i<256;++i){l[i]=r[i]=amplitude*std::sin(2*3.141592653589793*1000*(block*256+i)/48000);energy+=double(l[i])*l[i];}
   reinterpret_cast<void(*)(void*,void*,void*)>(table[7])(instance.get(),audio,midi);playhead.frame+=256;
   for(unsigned i=0;i<256;++i){assert(std::isfinite(l[i])&&l[i]==r[i]);assert(std::fabs(l[i])<=amplitude*2.00001f);}
   if(block>=281){dry+=energy;for(float sample:l)wet+=double(sample)*sample;}
  }
  return 10*std::log10(wet/dry);
 };
 for(float db:{-60.f,-30.f,-12.f,-6.f})assert(std::fabs(measure(db))<.001);
 set(3,1);double native_gain[4];unsigned at=0;
 for(float db:{-60.f,-30.f,-12.f,-6.f}){
  double gain=measure(db);native_gain[at++]=gain;assert(std::fabs(gain-6.0206)<.01);
  printf("GAIN_COMPARE Duck InBoost=100%% Amount=0 Color=off input_peak_db=%.0f gain_db=%.5f\n",db,gain);
 }
 assert(compressor_native_bind(base,instance.get()));assert(compressor_select(COMP_OPTO));
 compressor_knob(0,127);compressor_knob(1,0);compressor_knob(2,127);
 comp_config c;compressor_config(&c);assert(c.gain==6&&c.ratio==1&&c.threshold==0);
 at=0;
 for(float db:{-60.f,-30.f,-12.f,-6.f}){
  double gain=measure(db);assert(std::fabs(gain-6)<.01);
  assert(gain<=native_gain[at]&&native_gain[at]-gain<.05);++at;
  printf("GAIN_COMPARE Opto Gain=+6dB Ratio=1 input_peak_db=%.0f gain_db=%.5f\n",db,gain);
 }
 compressor_bypass();assert(std::fabs(measure(-30))<.01);
 assert(reinterpret_cast<float(*)(void*,int)>(v[27])(instance.get(),3)==1);
 puts("PASS Opto maximum gain matches native In Boost within 0.05dB, never exceeds it; actual master adapter, stereo, bypass and parameter isolation");return 0;
}
