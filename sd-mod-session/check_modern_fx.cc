#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <memory>
#include <utility>
#include <map>
#include <new>
#include <cmath>
#include <cassert>
#include <unordered_map>
#include <typeindex>
#include <mutex>
#include <cstddef>
#include "src/fx_native.h"
#include "src/fx_catalog.h"
struct NativeObject{};
struct NativeDelete {void operator()(NativeObject*p)const{if(p){auto*v=*reinterpret_cast<uintptr_t**>(p);reinterpret_cast<void(*)(void*)>(v[1])(p);}}};
using NativePtr=std::unique_ptr<NativeObject,NativeDelete>;
struct NativeText{const char*data;};
extern "C" uint32_t requested_fx;
static uintptr_t native_base;
struct EmptyNativeString {
 const char*data;
 EmptyNativeString(){reinterpret_cast<void(*)(void*)>(native_base+0xf1c620)(this);}
 ~EmptyNativeString(){reinterpret_cast<void(*)(void*)>(native_base+0xf0a290)(this);}
};
static EmptyNativeString no_preset_directory(void*,const void*){return {};}
struct OfflinePlayHead {
 uint64_t frame=0;
 virtual ~OfflinePlayHead(){}
 virtual bool position(void*p){std::memset(p,0,88);*reinterpret_cast<double*>(p)=120;*reinterpret_cast<int*>(static_cast<char*>(p)+8)=4;*reinterpret_cast<int*>(static_cast<char*>(p)+12)=4;*reinterpret_cast<double*>(static_cast<char*>(p)+40)=frame/48000.*2;static_cast<char*>(p)[60]=1;return true;}
};
extern "C" int modern_fx_probe(uintptr_t base,const char**phase){
 native_base=base;
 *phase="native sample-rate lookup tables";reinterpret_cast<void(*)(float)>(base+0xd8d320)(48000.f);
 alignas(8) unsigned char format[24]{},description[112]{},mpc_format[624]{};
 *phase="native AIR format constructor";
 // Isolated fixture: empty preset-directory service. The FX factory and DSP
 // stay native. Production must borrow the live PluginManager dependency.
 alignas(8) unsigned char presets[256]{};
 uintptr_t preset_vtable[3]={0,0,reinterpret_cast<uintptr_t>(no_preset_directory)};
 *reinterpret_cast<uintptr_t**>(presets)=preset_vtable;
 new(presets+0x8)std::map<int,int>();new(presets+0x68)std::map<int,int>();
 void*empty_presets=presets;
 reinterpret_cast<void(*)(void*,void*)>(base+0x2530ee0)(format,&empty_presets);
 *reinterpret_cast<uintptr_t*>(mpc_format)=base+0x6eacf78;*reinterpret_cast<void**>(mpc_format+32)=&empty_presets;
 *reinterpret_cast<uint32_t*>(description+72)=requested_fx; // actual descriptor ID
 NativePtr instance;bool called=false;
 using Callback=std::function<void(NativePtr,const NativeText&)>;
 Callback callback=[&](NativePtr p,const NativeText&error){called=true;std::printf("AIR callback instance=%d error=%s\n",p!=nullptr,error.data?error.data:"");instance=std::move(p);};
 *phase="native AIR Tube Drive factory";
 reinterpret_cast<void(*)(void*,const void*,double,int,Callback)>(base+0x2531110)(format,description,48000.,256,std::move(callback));
 if(!instance){
  Callback mpc_callback=[&](NativePtr p,const NativeText&error){std::printf("MPC callback instance=%d error=%s\n",p!=nullptr,error.data?error.data:"");instance=std::move(p);};
  *phase="native MPC FX factory";reinterpret_cast<void(*)(void*,const void*,double,int,Callback)>(base+0x2537560)(mpc_format,description,48000.,256,std::move(mpc_callback));
 }
 std::printf("AIR factory returned callback=%d instance=%d\n",called,instance!=nullptr);
 bool created=instance!=nullptr;
 if(instance){auto*v=*reinterpret_cast<uintptr_t**>(instance.get());std::printf("AIR instance vptr=%llx\n",(unsigned long long)(reinterpret_cast<uintptr_t>(v)-base));
  unsigned n=reinterpret_cast<int(*)(void*)>(v[24])(instance.get());std::printf("AIR id=%08x params=%u\n",requested_fx,n);assert(n&&n<=32);
  for(unsigned i=0;i<n;++i){auto name=reinterpret_cast<EmptyNativeString(*)(void*,int)>(v[25])(instance.get(),i);auto display=reinterpret_cast<EmptyNativeString(*)(void*,int)>(v[33])(instance.get(),i);std::printf("p%u=%g %s [%s]\n",i,(double)reinterpret_cast<float(*)(void*,int)>(v[27])(instance.get(),i),name.data,display.data);}
  *phase="native AIR prepare";reinterpret_cast<void(*)(void*,double,int)>(v[4])(instance.get(),48000.,256);
  OfflinePlayHead playhead;*reinterpret_cast<void**>(reinterpret_cast<char*>(instance.get())+16)=&playhead;
  float left[256]{},right[256]{};float*channels[]={left,right};
  alignas(8) unsigned char audio[320]{};*reinterpret_cast<int*>(audio)=2;*reinterpret_cast<int*>(audio+4)=256;*reinterpret_cast<float***>(audio+16)=channels;
  alignas(8) unsigned char midi[16]{};double energy=0,error=0;*phase="native AIR process";
  for(unsigned block=0;block<32;++block){for(unsigned i=0;i<256;++i)left[i]=right[i]=.5f*std::sin(2*3.141592653589793*440*(block*256+i)/48000);
   reinterpret_cast<void(*)(void*,void*,void*)>(v[7])(instance.get(),audio,midi);
   playhead.frame+=256;
   for(unsigned i=0;i<256;++i){assert(std::isfinite(left[i])&&std::isfinite(right[i]));energy+=left[i]*left[i]+right[i]*right[i];double dry=.5*std::sin(2*3.141592653589793*440*(block*256+i)/48000);error+=(left[i]-dry)*(left[i]-dry);}}
  std::printf("Native FX real audio energy=%.8f difference=%.8f\n",energy,error);assert(energy>1);if(requested_fx==0x61465444||requested_fx==0x61465075)assert(error>.01);
  *phase="production native FX backend";
  /* Exercise the application-owned capture path. The old registry lookup
   * could pass with invented registrations absent on the actual device.
   * Owners below are bounded fixtures; the playhead getter/factories/DSP
   * execute native code. No constructors or audio device are simulated. */
  struct Registry {std::unordered_map<std::type_index,void*> services;std::mutex mutex;};
  static_assert(offsetof(Registry,mutex)==0x38,"Native registry mutex ABI");
  Registry registry;
  alignas(8) unsigned char manager[0x248]{},device_playhead[0x100]{},application[0x100]{},audio_owner[0x198]{},core[0x7b8]{};void*formats[]={format,mpc_format};
  *reinterpret_cast<uintptr_t*>(manager)=base+0x6da1f90;*reinterpret_cast<void***>(manager+0x70)=formats;*reinterpret_cast<int*>(manager+0x7c)=2;
  *reinterpret_cast<uintptr_t*>(device_playhead)=base+0x6d8a418;*reinterpret_cast<double*>(device_playhead+8)=123;*reinterpret_cast<unsigned*>(device_playhead+16)=7;*reinterpret_cast<unsigned*>(device_playhead+20)=8;
  auto&application_slot=*reinterpret_cast<void**>(base+0x71e3dc8);void*prior_application=application_slot;
  *reinterpret_cast<uintptr_t*>(application)=base+0x6e07288;*reinterpret_cast<void**>(application+0xf8)=&registry;*reinterpret_cast<void**>(application+0xd8)=audio_owner;
  *reinterpret_cast<uintptr_t*>(audio_owner)=base+0x6e0c430;*reinterpret_cast<void**>(audio_owner+0x148)=core;
  *reinterpret_cast<uintptr_t*>(core)=base+0x6e0cf38;*reinterpret_cast<void**>(core+0x4d0)=manager;*reinterpret_cast<void**>(core+0x490)=device_playhead;
  application_slot=application;
  fx_native_registry_bind(&registry);fx_native_context environment{};
  assert(fx_native_context_capture(base,&environment));assert(registry.services.empty());
  assert(environment.air_format==format&&environment.mpc_format==mpc_format&&environment.bpm==123&&environment.numerator==7&&environment.denominator==8);
  *reinterpret_cast<uintptr_t*>(manager)=base+0x6e78360;assert(!fx_native_context_capture(base,&environment));
  *reinterpret_cast<uintptr_t*>(manager)=base+0x6da1f90;*reinterpret_cast<double*>(device_playhead+8)=0;assert(!fx_native_context_capture(base,&environment));
  *reinterpret_cast<double*>(device_playhead+8)=123;
  *reinterpret_cast<void**>(application+0xf8)=nullptr;assert(!fx_native_context_capture(base,&environment));*reinterpret_cast<void**>(application+0xf8)=&registry;
  *reinterpret_cast<uintptr_t*>(audio_owner)=0;assert(!fx_native_context_capture(base,&environment));*reinterpret_cast<uintptr_t*>(audio_owner)=base+0x6e0c430;
  *reinterpret_cast<void**>(audio_owner+0x148)=nullptr;assert(!fx_native_context_capture(base,&environment));*reinterpret_cast<void**>(audio_owner+0x148)=core;
  *reinterpret_cast<uintptr_t*>(core)=0;assert(!fx_native_context_capture(base,&environment));*reinterpret_cast<uintptr_t*>(core)=base+0x6e0cf38;
  *reinterpret_cast<uintptr_t*>(device_playhead)=0;assert(!fx_native_context_capture(base,&environment));*reinterpret_cast<uintptr_t*>(device_playhead)=base+0x6d8a418;
  application_slot=nullptr;assert(!fx_native_context_capture(base,&environment));application_slot=application;
  assert(fx_native_context_capture(base,&environment)&&registry.services.empty());application_slot=prior_application;fx_native_registry_bind(nullptr);
  std::puts("PASS FX context capture: application-owned manager/playhead with empty service registry, native position getter; rejects missing/mismatched owners, destructor base and invalid tempo; owner objects are fixtures");
  fx_backend backend{};fx_native_backend(&environment,&backend);
  const auto*entry=fx_catalog_find(requested_fx);assert(entry&&entry->count==n);
  void*prepared=backend.create(backend.user,requested_fx,48000,entry->defaults);assert(prepared);
  float interleaved[512]{};interleaved[0]=interleaved[1]=.5f;
  for(unsigned i=0;i<16;++i){backend.process(backend.user,prepared,interleaved,256);for(float sample:interleaved)assert(std::isfinite(sample));std::memset(interleaved,0,sizeof(interleaved));}
  backend.destroy(backend.user,prepared);std::puts("PASS production native FX backend creates, configures, processes and destroys");
  if(requested_fx==0x61465075){
   prepared=backend.create(backend.user,requested_fx,48000,entry->defaults);assert(prepared);double modulation=0;
   for(unsigned b=0;b<100;++b){for(unsigned i=0;i<512;++i)interleaved[i]=.25f;backend.process(backend.user,prepared,interleaved,256);for(float sample:interleaved){assert(std::isfinite(sample));modulation+=(sample-.25f)*(sample-.25f);}}
   backend.destroy(backend.user,prepared);assert(modulation>.01);std::printf("PASS offline transport activates Pumper modulation=%g\n",modulation);
  }
  if(requested_fx==0x61465444){
   *phase="five real FX stages and move";
   fx_chain_config chain{};const uint32_t ids[]={0,0x61465444,0x6146446c,0x61465276,0x61464c6d};
   for(uint32_t id:ids){unsigned i=0;while(fx_catalog_at(i)&&fx_catalog_at(i)->id!=id)++i;fx_stage stage{};assert(fx_catalog_stage(i,48000,&stage));assert(fx_chain_add(&chain,&stage,48000)==FX_OK);}
   chain.stage[0].eq.band[1].gain_db=12;chain.stage[1].parameter[0]=.6f;chain.stage[2].parameter[1]=0;chain.stage[2].parameter[0]=.1f;chain.stage[2].parameter[9]=.7f;
   fx_program*program=fx_program_prepare(&chain,48000,&backend);assert(program);
   float first[32768*2]{};first[0]=first[1]=.25f;fx_program_process(program,first,32768);fx_program_destroy(program);
   assert(fx_chain_move(&chain,0,4)==FX_OK);program=fx_program_prepare(&chain,48000,&backend);assert(program);
   float moved[32768*2]{};moved[0]=moved[1]=.25f;fx_program_process(program,moved,32768);fx_program_destroy(program);
   double difference=0,tail=0;for(unsigned i=0;i<32768*2;++i){assert(std::isfinite(first[i])&&std::isfinite(moved[i]));difference+=(first[i]-moved[i])*(first[i]-moved[i]);if(i>512)tail+=first[i]*first[i];}
   assert(tail>1e-8&&difference>1e-8);std::printf("PASS five real stages EQ/TubeDrive/Delay/Reverb/Limiter; tail=%g reordered_difference=%g\n",tail,difference);
  }
 }
 *phase="native AIR instance destruction";instance.reset();
 reinterpret_cast<void(*)(void*)>(base+0x2530e30)(format);
 return called&&created?0:1;
}
