#include "pad_output_stream.h"
#include "pad_native_block.h"
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#define F(p,o,T) (*reinterpret_cast<T*>(reinterpret_cast<char*>(p)+(o)))
extern "C" void pad_output_frame(void);
struct OutputStream {
    uintptr_t *vptr;
    uintptr_t metadata,metadata_ref,flags;
    uint64_t position;
    uintptr_t base;
    struct pad_output_reference audio;
    uintptr_t table[10];
};
static_assert(offsetof(OutputStream,position)==0x20,"Native ProcessingStream prefix");
static void destroy_stream(void*p)noexcept{
    auto*s=static_cast<OutputStream*>(p);
    if(s->audio.release){auto release=s->audio.release;s->audio.release=nullptr;release(s->audio.owner);}
    reinterpret_cast<void(*)(void*)>(s->base+0x20c7360)(p);
}
static void delete_stream(void*p)noexcept{destroy_stream(p);std::free(p);}
static int64_t position(void*p)noexcept{return (int64_t)static_cast<OutputStream*>(p)->position;}
static int seek(void*p,int64_t frame)noexcept{
    auto*s=static_cast<OutputStream*>(p);if(frame<0||(uint64_t)frame>s->audio.frames)return 0;
    s->position=(uint64_t)frame;return 1;
}
extern "C" void pad_output_frame_result(void*p,int64_t requested,pad_native_block*out)noexcept{
    auto*s=static_cast<OutputStream*>(p);
    /* Match native empty-completion construction: its manager ignores the
     * opaque payload. Never write extra reserved payload bytes into a native
     * caller's return object. Our reader reserves64 bytes conservatively. */
    out->frames=0;out->pcm=nullptr;out->invoke=s->base+0xb63488;out->manage=s->base+0x20c54d0;
    if(requested<=0||s->position>=s->audio.frames)return;
    uint64_t count=(uint64_t)requested;if(count>s->audio.frames-s->position)count=s->audio.frames-s->position;
    out->frames=(int64_t)count;out->pcm=s->audio.pcm+s->position*2;s->position+=count;
}
extern "C" enum pad_render_status pad_output_stream_open(uintptr_t base,
    const struct pad_output_reference*a,uintptr_t out[2])
{
    if(!out)return PAD_RENDER_INVALID;
    out[0]=out[1]=0;
    if(!base||!a||!a->owner||!a->pcm||!a->retain||!a->release||!a->frames||
        a->frames>PAD_RENDER_MAX_PCM_BYTES/(2*sizeof(float))||!std::isfinite(a->rate)||
        a->rate<8000||a->rate>192000||std::floor(a->rate)!=a->rate)return PAD_RENDER_INVALID;
    auto*s=static_cast<OutputStream*>(std::calloc(1,sizeof(OutputStream)));if(!s)return PAD_RENDER_RESOURCE;
    bool constructed=false;
    try{
        uintptr_t none[2]={};reinterpret_cast<void(*)(void*,const uintptr_t*)>(base+0x20c74f0)(s,none);constructed=true;s->base=base;
        /* Keep native interface hooks/RTTI and the metadata prefix. Override
         * only destruction, position, read-block and seek; never impersonate
         * another concrete native stream. */
        s->table[0]=F(reinterpret_cast<void*>(base),0x6e7d778,uintptr_t);
        s->table[1]=F(reinterpret_cast<void*>(base),0x6e7d780,uintptr_t);
        s->table[2]=reinterpret_cast<uintptr_t>(destroy_stream);s->table[3]=reinterpret_cast<uintptr_t>(delete_stream);
        s->table[4]=base+0x20c54b0;s->table[5]=reinterpret_cast<uintptr_t>(position);s->table[6]=base+0x20c58f0;
        s->table[7]=reinterpret_cast<uintptr_t>(pad_output_frame);s->table[8]=reinterpret_cast<uintptr_t>(seek);s->table[9]=base+0x20c54c0;s->vptr=s->table+2;
        void*m=reinterpret_cast<void*>(s->metadata);
        reinterpret_cast<void(*)(void*,int)>(base+0x20c56b0)(m,2);
        reinterpret_cast<void(*)(void*,int)>(base+0x20c56d0)(m,(int)a->frames);
        reinterpret_cast<void(*)(void*,int)>(base+0x20c56f0)(m,(int)a->rate);
        if(!a->retain(a->owner)){destroy_stream(s);std::free(s);return PAD_RENDER_RESOURCE;}
        s->audio=*a;out[0]=base+0x20c58d0;out[1]=reinterpret_cast<uintptr_t>(s);return PAD_RENDER_WORKING;
    }catch(...){if(constructed)reinterpret_cast<void(*)(void*)>(base+0x20c7360)(s);std::free(s);return PAD_RENDER_RESOURCE;}
}
