/* Static 320x240 Am MOD image, using the pinned firmware's software Image.
 * One allocation and one blit per paint; no display ownership, I/O, timers,
 * delays or audio work here. Modal error/progress windows stay native. */
#include "startup_splash.h"
#include <algorithm>
#include "am_mod_splash.inc"
#include "splash_branding.h"
#define F(p,o,T) (*reinterpret_cast<T*>(reinterpret_cast<char*>(p)+(o)))
static uintptr_t image_base;
static void*pixel_data;
static bool attempted;
struct StartupImage {
 void*data;
 StartupImage():data(nullptr){}
 StartupImage(const StartupImage&)=delete;
 StartupImage(StartupImage&&other):data(other.data){other.data=nullptr;}
 ~StartupImage(){
  if(data&&__atomic_sub_fetch(&F(data,8,unsigned),1,__ATOMIC_ACQ_REL)==0){
   auto vt=F(data,0,uintptr_t*);reinterpret_cast<void(*)(void*)>(vt[1])(data);
  }
 }
};
extern "C" int startup_splash_draw(uintptr_t base,void*g){
 if(!base||!g||!F(g,8,void*))return 0;
 if(!pixel_data){
  if(attempted)return 0;
  attempted=true;
  auto type=reinterpret_cast<uintptr_t*>(base+0x6d794c8);
  if(type[2]!=base+0x100dd30)return 0;
  uintptr_t instance=reinterpret_cast<uintptr_t>(type);
  using Create=StartupImage(*)(void*,int,int,int,bool);
  StartupImage made=reinterpret_cast<Create>(type[2])(&instance,2,320,240,true);
  void*p=made.data;
  if(!p||F(p,0,uintptr_t)!=base+0x6d79480||F(p,12,int)!=2||F(p,16,int)!=320||
     F(p,20,int)!=240||F(p,64,int)!=4||F(p,68,int)!=1280||!F(p,56,void*))return 0;
  auto pixels=F(p,56,uint32_t*);unsigned count=0;
  for(const auto&run:am_splash_runs){
   if(!run.count||run.count>320*240-count)return 0;
   std::fill_n(pixels+count,run.count,run.argb);count+=run.count;
  }
  if(count!=320*240)return 0;
  am_splash_branding(pixels,1280);
  image_base=base;pixel_data=p;made.data=nullptr;
  alignas(8) unsigned char bitmap[64]{};
  reinterpret_cast<void(*)(void*,void*,int,int,int)>(base+0x10874b0)(p,bitmap,0,0,1);
 }
 if(base!=image_base)return 0;
 void*context=F(g,8,void*);auto vt=F(context,0,uintptr_t*);
 const float identity[]={1,0,0,0,1,0};
 reinterpret_cast<void(*)(void*,const void*,const float*)>(vt[25])(context,&pixel_data,identity);
 return 1;
}
