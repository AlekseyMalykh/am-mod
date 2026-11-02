/* Compare actual pinned Duck DSP and the production Opto master adapter.
 * The preset service/playhead are offline fixtures; the stock DSP is real. */
#define modern_fx_probe unused_fx_gain_probe
#include "check_modern_fx.cc"
#undef modern_fx_probe
#include "src/compressor.h"
static void*candidate;static uintptr_t image_base;static unsigned attempts;
static void(*specific)(void*,void*,void*);
static void during_process(void*p,void*b,void*m){
 ++attempts;assert(!compressor_native_bind(image_base,candidate));specific(p,b,m);
}
extern "C" int modern_fx_probe(uintptr_t base,const char**phase){
 native_base=base;*phase="native master replacement";
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
 auto old_table=*reinterpret_cast<uintptr_t**>(instance.get());
 reinterpret_cast<void(*)(void*,double,int)>(old_table[4])(instance.get(),48000.,256);
 OfflinePlayHead playhead;*reinterpret_cast<void**>(reinterpret_cast<char*>(instance.get())+16)=&playhead;
 assert(compressor_native_bind(base,instance.get()));
 auto hooked=*reinterpret_cast<uintptr_t**>(instance.get());
 NativePtr replacement;Callback next=[&](NativePtr p,const NativeText&){replacement=std::move(p);};
 reinterpret_cast<void(*)(void*,const void*,double,int,Callback)>(base+0x2531110)(format,description,48000.,256,std::move(next));assert(replacement);
 auto vt=*reinterpret_cast<uintptr_t**>(replacement.get());
 reinterpret_cast<void(*)(void*,double,int)>(vt[4])(replacement.get(),48000.,256);
 *reinterpret_cast<void**>(reinterpret_cast<char*>(replacement.get())+16)=&playhead;
 candidate=replacement.get();image_base=base;
 specific=reinterpret_cast<void(*)(void*,void*,void*)>(hooked[114]);hooked[114]=reinterpret_cast<uintptr_t>(during_process);
 float l[256]{},r[256]{},*channels[]={l,r};alignas(8) unsigned char audio[320]{},midi[16]{};
 *reinterpret_cast<int*>(audio)=2;*reinterpret_cast<int*>(audio+4)=256;*reinterpret_cast<float***>(audio+16)=channels;
 reinterpret_cast<void(*)(void*,void*,void*)>(hooked[7])(instance.get(),audio,midi);
 assert(attempts==1&&!compressor_ready());
 assert(compressor_native_bind(base,replacement.get()));
 assert(compressor_ready()&&compressor_select(COMP_OPTO));
 instance.reset();assert(compressor_ready()&&compressor_type()==COMP_OPTO);
 replacement.reset();assert(!compressor_ready()&&compressor_type()==COMP_OPTO);
 puts("PASS in-flight old audio block rejects master handoff without waiting; UI retry binds new master, late old destructor cannot clear it");
 return 0;
}
