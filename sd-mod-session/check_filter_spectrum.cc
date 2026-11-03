/* Production hook, native prologue and ABI; native full painter/body are
 * receivers that distinguish waveform rasterisation from curve drawing. */
#include "src/filter_spectrum.cc"
#include <cassert>
#include <stdexcept>
static unsigned char sample[0x1100],view[0x250],other[0x100];
static unsigned draws,curves,paints;static int tab,visible=1,fail,nested;
static void*expected_graphics=reinterpret_cast<void*>(1);static int tint_enabled,raster_fail;
static uint32_t ink=0xffc247,saved_ink;static unsigned saves,restores,fills;
static uintptr_t context_table[19];static uintptr_t*context=context_table;
static uintptr_t graphics[3];static void*raster_adapter=reinterpret_cast<void*>(1);
static int tint(uint32_t*out){if(!tint_enabled)return 0;*out=0x8830ff;return 1;}
static void save(void*g){assert(g==&context);saved_ink=ink;++saves;}
static void restore(void*g){assert(g==&context);ink=saved_ink;++restores;}
static void fill(void*g,const void*value){void*f=const_cast<void*>(value);assert(g==&context);assert(F(f,24,float)==1&&F(f,40,float)==1&&!F(f,8,uintptr_t)&&!F(f,16,uintptr_t));ink=F(f,0,uint32_t);++fills;}
static int enabled(void*p){assert(p==sample);return visible&&(tab==0||tab==1);}
static int body(void*g,const void*r,int channels,int samples,void*out,unsigned first,float scale,float y){
    assert(g==raster_adapter&&r==other&&channels==2&&samples==100&&out==view&&first==7&&scale==.25f&&y==.5f);++draws;
    if(tint_enabled&&sample_scope)assert(ink==0xff8830ff);
    if(raster_fail)throw std::runtime_error("raster receiver");
    return 1;
}
static void original_paint(void*v,void*g){
    assert(g==expected_graphics);++paints;
    for(unsigned i=0;i<2;++i){
        reinterpret_cast<Waveform>(image+0x2d53470)(raster_adapter,other,2,100,view,7,.25f,.5f);
        ++curves; /* Native painter continues to its independent overlay. */
        if(fail)throw std::runtime_error("native paint receiver");
    }
    if(nested&&v==view){nested=0;paint_waveform(other,g);}
}
static void stub(uintptr_t off,void*fn){uint32_t jump[]={0x58000050,0xd61f0200};std::memcpy(reinterpret_cast<void*>(image+off),jump,8);F(reinterpret_cast<void*>(image+off),8,uintptr_t)=reinterpret_cast<uintptr_t>(fn);}
int main(){
    void*mem=mmap(nullptr,0x7500000,7,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(mem!=MAP_FAILED);uintptr_t base=reinterpret_cast<uintptr_t>(mem);image=base;
    const uint32_t prologue[]={0xd503233f,0xd10343ff,0xa9087bfd,0x910203fd};
    std::memcpy(reinterpret_cast<void*>(base+0x2d53470),prologue,16);
    /* Execute the relocated real prologue, then restore its native frame
     * before passing unchanged integer and floating-point args to a receiver. */
    const uint32_t resume[]={0xa9487bfd,0x910343ff,0xd50323bf,0x58000070,0xd61f0200,0xd503201f};
    std::memcpy(reinterpret_cast<void*>(base+0x2d53480),resume,24);F(reinterpret_cast<void*>(base+0x2d53480),24,uintptr_t)=reinterpret_cast<uintptr_t>(body);
    stub(0x2d574b0,reinterpret_cast<void*>(original_paint));
    uintptr_t*slot=reinterpret_cast<uintptr_t*>(base+0x6e25c10+0xd0);*slot=base+0x2d574b0;
    __builtin___clear_cache(static_cast<char*>(mem),static_cast<char*>(mem)+0x7500000);
    image=0;assert(!filter_spectrum_install(base+4,enabled));assert(!image);assert(filter_spectrum_install(base,enabled));
    assert(filter_spectrum_install(base,enabled)&&!filter_spectrum_install(base+4096,enabled));
    F(sample,0,uintptr_t)=base+0x6e212a0;F(view,0x18,void*)=sample;
    auto paint=reinterpret_cast<void(*)(void*,void*)>(*slot);assert(paint==paint_waveform);
    tab=2;paint(view,reinterpret_cast<void*>(1));assert(draws==2&&curves==2&&!filtered);
    for(tab=0;tab<=1;++tab){unsigned d=draws,c=curves;paint(view,reinterpret_cast<void*>(1));assert(draws==d&&curves==c+2&&!filtered);}
    visible=0;paint(view,reinterpret_cast<void*>(1));assert(draws==4);visible=1;
    tab=0;nested=1;paint(view,reinterpret_cast<void*>(1));assert(draws==6&&!filtered); /* Unrelated nested view retains its waveform. */
    fail=1;try{paint(view,reinterpret_cast<void*>(1));assert(false);}catch(const std::runtime_error&){}assert(!filtered);fail=0;
    tab=2;unsigned d=draws;paint(view,reinterpret_cast<void*>(1));assert(draws==d+2);
    assert(paints==8);std::puts("PASS waveform-only suppression on Filter/Filter Env: native curve stages continue, Trim/Tune/unrelated/nested views pass through, exception restores scope; pinned native prologue and float/int argument forwarding executed");
    context_table[14]=reinterpret_cast<uintptr_t>(save);context_table[15]=reinterpret_cast<uintptr_t>(restore);context_table[18]=reinterpret_cast<uintptr_t>(fill);
    graphics[1]=reinterpret_cast<uintptr_t>(&context);
    filter_spectrum_tint_provider(tint);tint_enabled=1;expected_graphics=graphics;
    tab=2;paint(view,expected_graphics);assert(saves==2&&restores==2&&fills==2&&ink==0xffc247);
    tab=0;paint(view,expected_graphics);assert(saves==2&&restores==2); /* Filter curves stay native. */
    paint(other,expected_graphics);assert(saves==2&&ink==0xffc247); /* unrelated view */
    tab=2;raster_fail=1;try{paint(view,expected_graphics);assert(false);}catch(const std::runtime_error&){}raster_fail=0;
    assert(saves==3&&restores==3&&ink==0xffc247&&!sample_scope&&!filtered);
    tint_enabled=0;paint(view,expected_graphics);assert(saves==3&&restores==3);
    std::puts("PASS optional pad waveform tint scopes the pinned native FillType and save/restore to sample rasterization; off/filter/unrelated views retain original colours, exceptions restore native graphics state");
    munmap(mem,0x7500000);return 0;
}
