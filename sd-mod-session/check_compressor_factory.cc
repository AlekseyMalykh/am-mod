#define modern_fx_probe compressor_unused_parent_probe
#include "check_modern_fx.cc"
#undef modern_fx_probe
#include "src/compressor.h"
static bool showing=true;
static unsigned wakes,repaints,type_labels,indicators,native_calls;
static void(*native_specific)(void*,void*,void*);
extern "C" void compressor_phone_wake(void){++wakes;}
static int visible(void*){return showing;}
static int modal(void*){return 0;}
static void repaint(void*,uint64_t,uint64_t){++repaints;}
static void colour(void*,uint32_t){}
static void rectangle(void*,int x,int y,int w,int h){assert(x>=0&&y>=0&&w>=0&&h>=0&&x+w<=320&&y+h<=240);if(w==5&&y==25){assert(h==1||h==3);++indicators;}}
static void font_create(uint64_t*f,int,float){*f=0;}
static void font_destroy(uint64_t*){}
static void font_set(void*,const uint64_t*){}
static void text(void*,const uint64_t*s,int x,int y,int w,int h,int,int,float){assert(x>=0&&y>=0&&x+w<=320&&y+h<=240);if(!strcmp(reinterpret_cast<const char*>(*s),"Type"))++type_labels;}
static void redirect(uintptr_t base,uintptr_t offset,void*fn){uint32_t code[]={0x58000050,0xd61f0200};memcpy(reinterpret_cast<void*>(base+offset),code,8);memcpy(reinterpret_cast<void*>(base+offset+8),&fn,8);__builtin___clear_cache(reinterpret_cast<char*>(base+offset),reinterpret_cast<char*>(base+offset+16));}
static void counted(void*p,void*b,void*m){++native_calls;native_specific(p,b,m);}
struct ImageRef {
 void*data;
 ImageRef():data(nullptr){}
 ImageRef(const ImageRef&)=delete;
 ImageRef(ImageRef&&r):data(r.data){r.data=nullptr;}
 ~ImageRef(){if(data&&__atomic_sub_fetch(reinterpret_cast<unsigned*>(static_cast<char*>(data)+8),1,__ATOMIC_ACQ_REL)==0)reinterpret_cast<void(*)(void*)>((*reinterpret_cast<uintptr_t**>(data))[1])(data);}
};
struct ContextRef {
 void*data;
 ContextRef():data(nullptr){}
 ContextRef(const ContextRef&)=delete;
 ContextRef(ContextRef&&r):data(r.data){r.data=nullptr;}
 ~ContextRef(){if(data)reinterpret_cast<void(*)(void*)>((*reinterpret_cast<uintptr_t**>(data))[1])(data);}
};
static void check_image(uintptr_t base){
 uint32_t*source=compressor_image_pixels(base);assert(source);
 for(unsigned i=0;i<320*164;++i)source[i]=0xff000000u|((i*17)&255)*0x010101u;
 uintptr_t type=base+0x6d794c8;
 auto create=reinterpret_cast<ImageRef(*)(void*,int,int,int,bool)>(base+0x100dd30);
 ImageRef destination=create(&type,2,320,240,true);assert(destination.data);
 auto vt=*reinterpret_cast<uintptr_t**>(destination.data);
 ContextRef context=reinterpret_cast<ContextRef(*)(void*)>(vt[2])(destination.data);
 uintptr_t graphics[]={0,reinterpret_cast<uintptr_t>(context.data),0};assert(compressor_image_draw(graphics));
 auto out=*reinterpret_cast<uint32_t**>(static_cast<char*>(destination.data)+56);
 assert(!memcmp(out+320*34,source,320*164*4));
 assert(out[0]==0&&out[320*239]==0);puts("PASS real native software Image creation and single blit; pixels match at y34, header/footer untouched");
}
static int button(unsigned code,unsigned down,uint64_t ticket){unsigned char e[16]{};e[8]=static_cast<unsigned char>(code);e[9]=static_cast<unsigned char>(down);return compressor_ui_button(e,ticket);}
extern "C" int modern_fx_probe(uintptr_t base,const char**phase){
 native_base=base;
 *phase="OptoCat native software Image ABI";check_image(base);
 *phase="native Colour parameter descriptors";
 reinterpret_cast<void(*)()>(base+0xc034d0)();
 reinterpret_cast<void(*)(float)>(base+0xd8d320)(48000.f);
 alignas(8) unsigned char format[24]{},description[112]{},presets[256]{};
 uintptr_t preset_vtable[3]={0,0,reinterpret_cast<uintptr_t>(no_preset_directory)};
 *reinterpret_cast<uintptr_t**>(presets)=preset_vtable;
 new(presets+8)std::map<int,int>();new(presets+0x68)std::map<int,int>();void*empty=presets;
 reinterpret_cast<void(*)(void*,void*)>(base+0x2530ee0)(format,&empty);
 *reinterpret_cast<uint32_t*>(description+72)=0x43434d50;
 NativePtr instance;
 using Callback=std::function<void(NativePtr,const NativeText&)>;
 Callback callback=[&](NativePtr p,const NativeText&e){printf("Colour error=%s\n",e.data?e.data:"");instance=std::move(p);};
 *phase="native Colour factory";
 reinterpret_cast<void(*)(void*,const void*,double,int,Callback)>(base+0x2531110)(format,description,48000.,256,std::move(callback));
 assert(instance);auto*v=*reinterpret_cast<uintptr_t**>(instance.get());
 printf("Colour host vptr=%lx\n",reinterpret_cast<uintptr_t>(v)-base);
 unsigned n=reinterpret_cast<int(*)(void*)>(v[24])(instance.get());
 for(unsigned i=0;i<n;++i){auto name=reinterpret_cast<EmptyNativeString(*)(void*,int)>(v[25])(instance.get(),i);auto display=reinterpret_cast<EmptyNativeString(*)(void*,int)>(v[33])(instance.get(),i);printf("Colour p%u=%g %s [%s]\n",i,double(reinterpret_cast<float(*)(void*,int)>(v[27])(instance.get(),i)),name.data,display.data);}
 assert(n==6&&!compressor_select(COMP_OPTO));
 float native_parameters[6];for(unsigned i=0;i<6;++i)native_parameters[i]=reinterpret_cast<float(*)(void*,int)>(v[27])(instance.get(),i);
 *phase="native Colour prepare";reinterpret_cast<void(*)(void*,double,int)>(v[4])(instance.get(),48000.,256);
 OfflinePlayHead playhead;*reinterpret_cast<void**>(reinterpret_cast<char*>(instance.get())+16)=&playhead;
 printf("Engine rate=%g\n",double(*reinterpret_cast<float*>(base+0x7173b80)));
 redirect(base,0x112fd50,reinterpret_cast<void*>(visible));redirect(base,0x112f6b0,reinterpret_cast<void*>(modal));redirect(base,0x1149880,reinterpret_cast<void*>(repaint));
 redirect(base,0x102d5e0,reinterpret_cast<void*>(colour));redirect(base,0x102e000,reinterpret_cast<void*>(rectangle));redirect(base,0x1082810,reinterpret_cast<void*>(font_create));
 redirect(base,0x1070d60,reinterpret_cast<void*>(font_destroy));redirect(base,0x102dca0,reinterpret_cast<void*>(font_set));redirect(base,0x10817b0,reinterpret_cast<void*>(text));
 alignas(8) unsigned char screen[0x2700]{},meter[0x88]{};
 *reinterpret_cast<uintptr_t*>(screen)=base+0x6e1bf30;
 *reinterpret_cast<void**>(screen+0x2698)=meter;
 *reinterpret_cast<uintptr_t*>(meter)=base+0x6d8fac0;*reinterpret_cast<void**>(meter+0x80)=instance.get();
 compressor_ui_init(base);assert(compressor_ui_install());
 auto ui=reinterpret_cast<uintptr_t*>(base+0x6e1bf30);
 *phase="native Compressor Type binding";reinterpret_cast<void(*)(void*,void*)>(ui[27])(screen,nullptr);
 assert(compressor_ready()&&wakes&&type_labels==1&&indicators==2);
 auto vt=*reinterpret_cast<uintptr_t**>(instance.get());native_specific=reinterpret_cast<void(*)(void*,void*,void*)>(vt[114]);vt[114]=reinterpret_cast<uintptr_t>(counted);
 float l[256],r[256],*channels[]={l,r};alignas(8) unsigned char audio[320]{},midi[16]{};
 *reinterpret_cast<int*>(audio)=2;*reinterpret_cast<int*>(audio+4)=256;*reinterpret_cast<float***>(audio+16)=channels;
 auto process=reinterpret_cast<void(*)(void*,void*,void*)>(vt[7]);
 auto audio_block=[&](unsigned block){for(unsigned i=0;i<256;++i)l[i]=r[i]=.7f*sinf(float(block*256+i)*.057f);process(instance.get(),audio,midi);playhead.frame+=256;for(unsigned i=0;i<256;++i)assert(std::isfinite(l[i])&&l[i]==r[i]);};
 *phase="native Duck pass-through";audio_block(0);assert(native_calls==1&&compressor_type()==COMP_DUCK);
 uint64_t ticket=compressor_ui_ticket();assert(button(0x0d,1,ticket)&&compressor_type()==COMP_OPTO);
 assert(button(0x0d,1,ticket)&&compressor_type()==COMP_OPTO);assert(button(0x0d,0,ticket));
 *phase="Opto in actual native AudioPlugin callback";
 for(unsigned i=0;i<300;++i)audio_block(i);
 unsigned settled=native_calls;for(unsigned i=300;i<320;++i)audio_block(i);
 assert(settled==native_calls&&compressor_reduction()>5);
 assert(compressor_audio_blocks()>=320&&compressor_input_level()>-12&&compressor_hits()>0);
 reinterpret_cast<void(*)(void*,void*)>(ui[27])(screen,nullptr);assert(type_labels==2&&indicators==4);
 assert(button(0x0e,1,ticket)&&button(0x0e,0,ticket));for(unsigned i=0;i<80;++i)audio_block(i);assert(compressor_reduction()==0);
 assert(button(0x0d,1,ticket)&&button(0x0d,0,ticket)&&compressor_type()==COMP_DUCK);audio_block(0);assert(native_calls>settled);
 for(unsigned i=0;i<6;++i)assert(reinterpret_cast<float(*)(void*,int)>(v[27])(instance.get(),i)==native_parameters[i]);
 showing=false;compressor_ui_tick();assert(button(0x0d,1,ticket)&&compressor_type()==COMP_DUCK);assert(!compressor_ui_owns(2,0x0d));
 assert(repaints);puts("PASS native master binding, Type caption/two indicators, B2 edge pairing, actual Duck/Opto audio dispatch, no serial compression, Opto bypass, native parameter preservation, stale press rejection");return 0;
}
