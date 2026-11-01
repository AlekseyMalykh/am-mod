/* Production hook, native prologue and ABI; native full painter/body are
 * receivers that distinguish waveform rasterisation from curve drawing. */
#include "src/filter_spectrum.cc"
#include <cassert>
#include <stdexcept>
static unsigned char sample[0x1100],view[0x250],other[0x100];
static unsigned draws,curves,paints;static int tab,visible=1,fail,nested;
static int enabled(void*p){assert(p==sample);return visible&&(tab==0||tab==1);}
static int body(void*g,const void*r,int channels,int samples,void*out,unsigned first,float scale,float y){
    assert(g==reinterpret_cast<void*>(1)&&r==other&&channels==2&&samples==100&&out==view&&first==7&&scale==.25f&&y==.5f);++draws;return 1;
}
static void original_paint(void*v,void*g){
    assert(g==reinterpret_cast<void*>(1));++paints;
    for(unsigned i=0;i<2;++i){
        reinterpret_cast<Waveform>(image+0x2d53470)(g,other,2,100,view,7,.25f,.5f);
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
    munmap(mem,0x7500000);return 0;
}
