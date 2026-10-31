#include "pad_sample_source.h"
#include "pad_native_block.h"
#include <cmath>
#include <cstring>
#include <new>
#define F(p,o,T) (*reinterpret_cast<T*>(reinterpret_cast<char*>(p)+(o)))
extern "C" void pad_source_result(void*,uintptr_t*,uintptr_t);
extern "C" void pad_source_read_block(void*,int64_t,struct pad_native_block*,uintptr_t);
extern "C" void pad_source_layer_sample(void*,int,uintptr_t*,uintptr_t);
struct NativeSource {
    uintptr_t base,owned[2],reference;
    uint64_t frames,position;
    unsigned channels;
};
static void close_source(void*data)noexcept
{
    auto*s=static_cast<NativeSource*>(data);
    if(s->owned[1]&&s->owned[0]){
        uintptr_t stream=s->owned[1];s->owned[1]=0;
        try{reinterpret_cast<void(*)(void*)>(s->owned[0])(reinterpret_cast<void*>(stream));}catch(...){}
    }
    if(s->reference){
        try{reinterpret_cast<void(*)(void*)>(s->base+0x123bca0)(reinterpret_cast<void*>(s->reference));}catch(...){}
    }
    delete s;
}
static int read_source(void*data,uint64_t first,size_t count,float*out)noexcept
{
    auto*s=static_cast<NativeSource*>(data);
    if(!out||!count||count>PAD_RENDER_BLOCK_FRAMES||first>=s->frames||count>s->frames-first)return 0;
    try{
        void*stream=reinterpret_cast<void*>(s->owned[1]);
        if(first!=s->position){
            /* Public native seek also clears stream EOF flags. */
            if(!(reinterpret_cast<int(*)(void*,int64_t)>(s->base+0x20c5f30)(stream,(int64_t)first)&255))return 0;
            s->position=first;
        }
        size_t done=0;
        while(done<count){
            if(reinterpret_cast<int(*)(void*)>(s->base+0x20c58f0)(stream)&255)return 0;
            struct Scope{pad_native_block block{};bool complete=false;~Scope(){if(complete)pad_native_block_finish(&block);}} scope;
            pad_source_read_block(stream,(int64_t)(count-done),&scope.block,s->base+0x20c5f60);
            scope.complete=true;
            if(!scope.block.pcm||scope.block.frames<=0||(uint64_t)scope.block.frames>count-done)return 0;
            for(int64_t i=0;i<scope.block.frames;++i){
                const float*p=scope.block.pcm+i*s->channels;
                out[(done+i)*2]=p[0];out[(done+i)*2+1]=p[s->channels==1?0:1];
            }
            done+=(size_t)scope.block.frames;s->position+=(uint64_t)scope.block.frames;
        }
        return 1;
    }catch(...){return 0;}
}
extern "C" enum pad_render_status pad_sample_stream_open(uintptr_t base,
    const uintptr_t sample[2],struct pad_render_stream*out)
{
    if(!out)return PAD_RENDER_INVALID;
    std::memset(out,0,sizeof(*out));
    if(!base||!sample||!sample[0]||!sample[1]||!F(reinterpret_cast<void*>(sample[0]),0x130,void*))return PAD_RENDER_INVALID;
    auto*s=new(std::nothrow)NativeSource{};if(!s)return PAD_RENDER_RESOURCE;s->base=base;
    enum pad_render_status error=PAD_RENDER_INVALID;
    try{
        void*p=reinterpret_cast<void*>(sample[0]);
        int frames=reinterpret_cast<int(*)(void*)>(base+0x2941f80)(p);
        int channels=reinterpret_cast<int(*)(void*)>(base+0x2941f70)(p);
        int rate=reinterpret_cast<int(*)(void*)>(base+0x2941f90)(p);
        if(frames<=0||(channels!=1&&channels!=2)||!std::isfinite(rate)||rate<8000||rate>192000){close_source(s);return error;}
        s->reference=sample[1];__atomic_fetch_add(reinterpret_cast<unsigned*>(sample[1]+8),1,__ATOMIC_ACQUIRE);
        pad_source_result(p,s->owned,base+0x2943250);
        if(!s->owned[0]||!s->owned[1]){close_source(s);return PAD_RENDER_RESOURCE;}
        void*stream=reinterpret_cast<void*>(s->owned[1]);void*metadata=F(stream,8,void*);
        /* uint16+0 channels, int+4 frames, int+8 sample rate. Float+3d0
         * is source BPM (clamped20..999), never the sample rate. */
        if(!metadata||F(metadata,4,int)!=frames||F(metadata,0,uint16_t)!=channels||F(metadata,8,int)!=rate){close_source(s);return error;}
        s->frames=(uint64_t)frames;s->channels=(unsigned)channels;
        auto*v=F(stream,0,uintptr_t*);
        if(!v||!v[3]||!v[5]||!v[6]){close_source(s);return error;}
        /* Reader factories normally start at0, but do not assume that. */
        int64_t position=reinterpret_cast<int64_t(*)(void*)>(v[3])(stream);
        if(position<0||position>frames){close_source(s);return error;}
        s->position=(uint64_t)position;
        *out={s,(uint64_t)frames,(double)rate,read_source,close_source};return PAD_RENDER_WORKING;
    }catch(...){close_source(s);return PAD_RENDER_RESOURCE;}
}
extern "C" enum pad_render_status pad_sample_stream_open_pad(uintptr_t base,void*program,
    unsigned pad,struct pad_sample_selection*out)
{
    if(!out)return PAD_RENDER_INVALID;
    std::memset(out,0,sizeof(*out));
    if(!base||!program||pad>=128)return PAD_RENDER_INVALID;
    uintptr_t sample[2]={};enum pad_render_status status=PAD_RENDER_INVALID;
    try{
        void*instrument=reinterpret_cast<void*(*)(void*,int)>(base+0x286bbe0)(program,(int)pad);
        if(!instrument||F(instrument,0x2310,void*)!=program)return status;
        void*layer=F(instrument,0x22e8,void*);if(!layer)return status;
        int64_t first=F(layer,0x740+0x78,int64_t),end=F(layer,0x740+0xd0,int64_t);
        if(first<0||end<=first)return status;
        pad_source_layer_sample(instrument,0,sample,base+0x2803b90);
        if(sample[0]&&sample[1]){
            status=pad_sample_stream_open(base,sample,&out->stream);
            if(status==PAD_RENDER_WORKING){
                if((uint64_t)end>out->stream.frames){out->stream.close(out->stream.user);std::memset(out,0,sizeof(*out));status=PAD_RENDER_INVALID;}
                else{out->first=(uint64_t)first;out->end=(uint64_t)end;out->pad=pad;}
            }
        }
    }catch(...){status=PAD_RENDER_RESOURCE;}
    if(sample[1])reinterpret_cast<void(*)(void*)>(base+0x123bca0)(reinterpret_cast<void*>(sample[1]));
    return status;
}
