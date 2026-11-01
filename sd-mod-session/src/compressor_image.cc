/* Pinned firmware 1.3.0.12 software Image adapter. One allocation on the UI
 * thread; every animation frame reuses its pixels and makes one image blit. */
#include "compressor.h"
#include <cstring>
#define F(p,o,T) (*reinterpret_cast<T*>(reinterpret_cast<char*>(p)+(o)))
static uintptr_t image_base;
static void*pixel_data;
static uint32_t*pixels;
struct NativeImage {
 void*data;
 NativeImage():data(nullptr){}
 NativeImage(const NativeImage&)=delete;
 NativeImage(NativeImage&&other):data(other.data){other.data=nullptr;}
 ~NativeImage(){
  if(data&&__atomic_sub_fetch(&F(data,8,unsigned),1,__ATOMIC_ACQ_REL)==0){
   auto vt=F(data,0,uintptr_t*);reinterpret_cast<void(*)(void*)>(vt[1])(data);
  }
 }
};
extern "C" uint32_t*compressor_image_pixels(uintptr_t base){
 if(pixel_data)return base==image_base?pixels:nullptr;
 if(!base)return nullptr;
 auto type=reinterpret_cast<uintptr_t*>(base+0x6d794c8);
 if(type[2]!=base+0x100dd30)return nullptr;
 uintptr_t instance=reinterpret_cast<uintptr_t>(type);
 using Create=NativeImage(*)(void*,int,int,int,bool);
 NativeImage made=reinterpret_cast<Create>(type[2])(&instance,2,320,164,true);
 void*p=made.data;
 if(!p||F(p,0,uintptr_t)!=base+0x6d79480||F(p,12,int)!=2||
    F(p,16,int)!=320||F(p,20,int)!=164||F(p,64,int)!=4||F(p,68,int)!=1280||!F(p,56,void*))return nullptr;
 image_base=base;pixel_data=p;pixels=F(p,56,uint32_t*);made.data=nullptr;
 return pixels;
}
extern "C" int compressor_image_draw(void*g){
 if(!g||!pixel_data)return 0;
 void*context=F(g,8,void*);if(!context)return 0;
 // Notify native image listeners after rewriting pixels, including any
 // renderer texture cache. The pinned BitmapData ABI writes within 32 bytes.
 alignas(8) unsigned char bitmap[64]{};
 reinterpret_cast<void(*)(void*,void*,int,int,int)>(image_base+0x10874b0)(pixel_data,bitmap,0,0,1);
 auto vt=F(context,0,uintptr_t*);
 // LowLevelGraphicsContext::drawImage(Image const&, AffineTransform const&).
 const float transform[]={1,0,0,0,1,34};
 reinterpret_cast<void(*)(void*,const void*,const float*)>(vt[25])(context,&pixel_data,transform);
 return 1;
}
