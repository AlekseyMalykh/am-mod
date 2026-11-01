#include "filter_spectrum.h"
#include <cstdio>
#include <cstring>
#include <sys/mman.h>
#include <unistd.h>
#define F(p,o,T) (*reinterpret_cast<T*>(reinterpret_cast<char*>(p)+(o)))
static uintptr_t image;
static int (*page_enabled)(void*);
static thread_local unsigned filtered;
typedef int (*Waveform)(void*,const void*,int,int,void*,unsigned,float,float);
static Waveform waveform;
struct Scope {
    unsigned previous;
    explicit Scope(bool on):previous(filtered){filtered=on?previous+1:0;}
    ~Scope(){filtered=previous;}
};
static int draw_samples(void*g,const void*rect,int channels,int samples,void*out,unsigned first,float scale,float y){
    /* This helper draws sample min/max data, not the filter/envelope paths.
     * A false return means no waveform drawing occurred. */
    if(filtered)return 0;
    return waveform(g,rect,channels,samples,out,first,scale,y);
}
static void paint_waveform(void*view,void*g){
    void*root=view;
    for(unsigned i=0;i<32&&root;++i){
        if(F(root,0,uintptr_t)==image+0x6e212a0)break;
        root=F(root,0x18,void*);
    }
    Scope scope(root&&F(root,0,uintptr_t)==image+0x6e212a0&&page_enabled(root));
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
