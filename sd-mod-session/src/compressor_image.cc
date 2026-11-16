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

/* Images stay in the user's pinned firmware mapping; no copied PNG assets.
 * Decode once on the UI thread and retain JUCE's shared Image handles. */
struct StockIcon {unsigned offset,bytes,width,height;void*data;};
static StockIcon stock_icons[]={
 {0x49e7210,193,26,26,nullptr}, /* Key / Scale */
 {0x4b57ca0,384,16,16,nullptr}, /* Settings */
 {0x49e8840,700,49,50,nullptr}, /* Screen Mirror */
 {0x49e9f80,197,26,26,nullptr}, /* Delay */
 {0x49ea120,218,26,26,nullptr}, /* Modulation */
 {0x49ea2e0,213,26,26,nullptr}, /* Drive */
 {0x49ea4a0,182,26,26,nullptr}, /* EQ / Filter */
 {0x49ea610,231,26,26,nullptr}, /* Dynamics */
 {0x49ea7f0,198,26,26,nullptr}, /* Emulator */
};
static StockIcon*stock_icon(uintptr_t base,unsigned index){
 if(!base||index>=sizeof(stock_icons)/sizeof(stock_icons[0]))return nullptr;
 StockIcon*i=stock_icons+index;
 if(!i->data){
  using Load=NativeImage(*)(const void*,int);
  NativeImage made=reinterpret_cast<Load>(base+0x1083c50)(reinterpret_cast<void*>(base+i->offset),(int)i->bytes);
  i->data=made.data;made.data=nullptr;
 }
 return i->data?i:nullptr;
}
extern "C" int stock_icon_draw(uintptr_t base,void*g,unsigned index,int x,int y,int size){
 if(!g)return 0;
 StockIcon*i=stock_icon(base,index);if(!i)return 0;
 void*context=F(g,8,void*);if(!context)return 0;
 auto vt=F(context,0,uintptr_t*);
 float scale=(float)size/(float)(i->width>i->height?i->width:i->height);
 const float transform[]={scale,0,x+(size-i->width*scale)*.5f,0,scale,y+(size-i->height*scale)*.5f};
 reinterpret_cast<void(*)(void*,const void*,const float*)>(vt[25])(context,&i->data,transform);
 return 1;
}
/* Project's native row renderer uses each Image's dimensions for layout.
 * Give every normal/selected image the same transparent 24px cell. */
static struct ProjectIcon {void*source;void*cell;} project_icons[32];
static void*project_icon_cell(uintptr_t base,void*source){
 if(!source)return nullptr;
 for(auto&i:project_icons)if(i.cell&&(i.cell==source||i.source==source))return i.cell;
 ProjectIcon*entry=nullptr;for(auto&i:project_icons)if(!i.cell){entry=&i;break;}
 if(!entry)return nullptr;
 uintptr_t type=base+0x6d794c8;
 using Create=NativeImage(*)(void*,int,int,int,bool);
 NativeImage cell=reinterpret_cast<Create>(base+0x100dd30)(&type,2,24,24,true);
 if(!cell.data)return nullptr;
 auto vt=F(cell.data,0,uintptr_t*);
 void*ctx;
 struct Context {
  void*data;
  Context():data(nullptr){}
  Context(const Context&)=delete;
  Context(Context&&r):data(r.data){r.data=nullptr;}
  ~Context(){if(data)reinterpret_cast<void(*)(void*)>(F(data,0,uintptr_t*)[1])(data);}
 };
 Context owner=reinterpret_cast<Context(*)(void*)>(vt[2])(cell.data);ctx=owner.data;
 if(!ctx)return nullptr;
 int w=F(source,16,int),h=F(source,20,int);if(w<=0||h<=0)return nullptr;
 float scale=20.f/(float)(w>h?w:h);
 float transform[]={scale,0,(24-w*scale)*.5f,0,scale,(24-h*scale)*.5f};
 reinterpret_cast<void(*)(void*,const void*,const float*)>(F(ctx,0,uintptr_t*)[25])(ctx,&source,transform);
 reinterpret_cast<void(*)(void*,const void*)>(base+0x10333c0)(&entry->source,&source);
 entry->cell=cell.data;cell.data=nullptr;return entry->cell;
}
extern "C" int stock_project_icons(uintptr_t base,void*project){
 if(!project)return 0;
 void*model=F(project,0x410+0xc0,void*);if(!model)return 0;
 auto begin=F(model,8,uintptr_t),end=F(model,16,uintptr_t);
 if(!begin||end<begin||end-begin!=7*0x80)return 0;
 int changed=0;
 for(unsigned n=0;n<7;++n){
  StockIcon*i=n>=4?stock_icon(base,n-4):nullptr;
  for(unsigned slot=8;slot<=16;slot+=8){
   void*dst=reinterpret_cast<void*>(begin+n*0x80+slot);
   void*source=i?i->data:F(dst,0,void*);
   void*cell=project_icon_cell(base,source);
   if(!cell||F(dst,0,void*)==cell)continue;
   reinterpret_cast<void(*)(void*)>(base+0x10334b0)(dst);
   reinterpret_cast<void(*)(void*,const void*)>(base+0x10333c0)(dst,&cell);changed=1;
  }
 }
 return changed;
}

struct DuckText {
 const char*data;
 DuckText():data(nullptr){}
 DuckText(const DuckText&)=delete;
 DuckText(DuckText&&s):data(s.data){s.data=nullptr;}
 ~DuckText(){if(data)reinterpret_cast<void(*)(void*)>(duck_text_base+0xf0a290)(this);}
 static uintptr_t duck_text_base;
};
uintptr_t DuckText::duck_text_base;
extern "C" int compressor_parameter_text(uintptr_t base,void*p,unsigned parameter,char*out,size_t size){
 if(!base||!p||!out||!size)return 0;
 DuckText::duck_text_base=base;
 auto vt=F(p,0,uintptr_t*);
 DuckText text=reinterpret_cast<DuckText(*)(void*,int)>(vt[33])(p,(int)parameter);
 if(!text.data)return 0;
 std::strncpy(out,text.data,size-1);out[size-1]=0;return 1;
}
