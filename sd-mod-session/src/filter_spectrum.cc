#include "filter_spectrum.h"
#include <cstdio>
#include <cstring>
#include <sys/mman.h>
#include <unistd.h>
#define F(p,o,T) (*reinterpret_cast<T*>(reinterpret_cast<char*>(p)+(o)))
static uintptr_t image;
static int (*page_enabled)(void*);
static thread_local unsigned filtered;
static thread_local bool sample_scope;
static thread_local void*sample_graphics;
static int (*tint_provider)(uint32_t*);
extern "C" void filter_spectrum_tint_provider(int(*provider)(uint32_t*)){tint_provider=provider;}
typedef int (*Waveform)(void*,const void*,int,int,void*,unsigned,float,float);
static Waveform waveform;
struct Scope {
    unsigned previous;
    bool previous_sample;
    void*previous_graphics;
    explicit Scope(bool on,bool sample,void*g):previous(filtered),previous_sample(sample_scope),previous_graphics(sample_graphics){filtered=on?previous+1:0;sample_scope=sample;sample_graphics=sample?g:nullptr;}
    ~Scope(){filtered=previous;sample_scope=previous_sample;sample_graphics=previous_graphics;}
};
/* Pinned LowLevelGraphicsContext: saveState14, restoreState15, setFill18.
 * Only the sample rasterizer is tinted. Restoring the context also preserves
 * gradients/opacity used by native curves, handles and subsequent children. */
struct WaveTint {
    void*context;uintptr_t*table;
    explicit WaveTint():context(nullptr),table(nullptr){
        uint32_t rgb=0;
        if(!sample_scope||!sample_graphics||!tint_provider||!tint_provider(&rgb))return;
        /* draw_samples receives a waveform raster adapter, NOT a Graphics
         * or LowLevelGraphicsContext. Use the real paint owner's Graphics. */
        void*g=F(sample_graphics,8,void*);if(!g)return;
        table=F(g,0,uintptr_t*);
        if(!table||!table[14]||!table[15]||!table[18])return;
        context=g;reinterpret_cast<void(*)(void*)>(table[14])(g);
        struct Fill {uint32_t colour,padding;void*gradient;void*image;float transform[6];} fill={rgb|0xff000000u,0,nullptr,nullptr,{1,0,0,0,1,0}};
        static_assert(sizeof(Fill)==48,"Pinned native FillType layout");
        reinterpret_cast<void(*)(void*,const void*)>(table[18])(g,&fill);
    }
    ~WaveTint(){if(context)reinterpret_cast<void(*)(void*)>(table[15])(context);}
};
static int draw_samples(void*g,const void*rect,int channels,int samples,void*out,unsigned first,float scale,float y){
    /* This helper draws sample min/max data, not the filter/envelope paths.
     * A false return means no waveform drawing occurred. */
    if(filtered)return 0;
    WaveTint tint;
    return waveform(g,rect,channels,samples,out,first,scale,y);
}
static void paint_waveform(void*view,void*g){
    void*root=view;
    for(unsigned i=0;i<32&&root;++i){
        if(F(root,0,uintptr_t)==image+0x6e212a0)break;
        root=F(root,0x18,void*);
    }
    bool sample=root&&F(root,0,uintptr_t)==image+0x6e212a0;
    Scope scope(sample&&page_enabled(root),sample,g);
    /* Keep the original complete painter: backgrounds, native filter and
     * envelope curves, handles and child layout remain its responsibility. */
    reinterpret_cast<void(*)(void*,void*)>(image+0x2d574b0)(view,g);
}
extern "C" int filter_spectrum_install(uintptr_t base,int(*enabled)(void*)){
    if(image)return image==base&&page_enabled==enabled;
    if(!base||!enabled||sysconf(_SC_PAGESIZE)!=4096)return 0;
    void*entry=reinterpret_cast<void*>(base+0x2d53470);
    uintptr_t*slot=reinterpret_cast<uintptr_t*>(base+0x6e25c10+0xd0);
    const uint32_t expected[]={0xd503233f,0xd10343ff,0xa9087bfd,0x910203fd};
    if(std::memcmp(entry,expected,sizeof(expected))||*slot!=base+0x2d574b0)return 0;
    void*relay=mmap(nullptr,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if(relay==MAP_FAILED)return 0;
    const uint32_t code[]={0xd503245f,expected[0],expected[1],expected[2],expected[3],0x58000070,0xd61f0200,0xd503201f};
    std::memcpy(relay,code,sizeof(code));F(relay,32,uintptr_t)=base+0x2d53480;
    __builtin___clear_cache(static_cast<char*>(relay),static_cast<char*>(relay)+40);
    if(mprotect(relay,4096,PROT_READ|PROT_EXEC)){munmap(relay,4096);return 0;}
    void*code_page=reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(entry)&~uintptr_t(4095));
    void*table_page=reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(slot)&~uintptr_t(4095));
    if(mprotect(table_page,4096,PROT_READ|PROT_WRITE)){munmap(relay,4096);return 0;}
    if(mprotect(code_page,4096,PROT_READ|PROT_WRITE)){
        if(mprotect(table_page,4096,PROT_READ))_exit(121);
        munmap(relay,4096);return 0;
    }
    image=base;page_enabled=enabled;waveform=reinterpret_cast<Waveform>(relay);
    const uint32_t jump[]={0x58000050,0xd61f0200};
    std::memcpy(entry,jump,8);F(entry,8,uintptr_t)=reinterpret_cast<uintptr_t>(draw_samples);
    *slot=reinterpret_cast<uintptr_t>(paint_waveform);
    __builtin___clear_cache(static_cast<char*>(entry),static_cast<char*>(entry)+16);
    if(mprotect(code_page,4096,PROT_READ|PROT_EXEC)||mprotect(table_page,4096,PROT_READ))_exit(121);
    std::fprintf(stderr,"FILTER_SPECTRUM WAVEFORM_ONLY_SUPPRESSED native_curves_preserved\n");return 1;
}
