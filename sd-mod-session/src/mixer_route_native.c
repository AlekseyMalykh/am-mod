#include "mixer_route_native.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

#define BUFFER_BYTES 0x130u
struct route_buffer { unsigned char native[BUFFER_BYTES];float *channel[2]; };
struct route_source {void *instrument,*buffer;unsigned pad,native;};
struct mixer_route_native {
    struct mixer_buses *buses;
    struct mixer_route_ops ops;
    unsigned capacity,frames,count,captured,disabled,forwarding,flushed,meter_pushed;
    enum mixer_route_result result;
    void *program,*main;
    struct route_buffer shadow,scratch;
    float *pcm;
    struct route_source source[MIXER_PADS];
    void(*before_block)(void*);
    void*before_user;
};
static _Thread_local struct mixer_route_native *scope;
/* memcpy avoids aliasing/alignment assumptions on the reverse-engineered ABI. */
static void *ptr(const void*p,size_t off){void*v;memcpy(&v,(const char*)p+off,sizeof(v));return v;}
static unsigned word(const void*p,size_t off){unsigned v;memcpy(&v,(const char*)p+off,4);return v;}
static void set_word(void*p,size_t off,unsigned v){memcpy((char*)p+off,&v,4);}
static unsigned clear(const void*p){return *((const unsigned char*)p+0x128);}
static void set_clear(void*p,unsigned value){*((unsigned char*)p+0x128)=(unsigned char)value;}
static int stereo(const void*p,unsigned frames)
{
    if(!p||word(p,0)!=2||word(p,4)<frames||clear(p)>1)return 0;
    void **channels=ptr(p,16);
    return channels&&channels[0]&&channels[1]&&channels[0]!=channels[1];
}
static void init_buffer(struct route_buffer*b,unsigned capacity,float*l,float*r)
{
    b->channel[0]=l;b->channel[1]=r;
    set_word(b->native,0,2);set_word(b->native,4,capacity);
    void *channels=b->channel;memcpy(b->native+16,&channels,sizeof(channels));set_clear(b->native,1);
}
static void copy_buffer(void*to,const void*from,unsigned frames)
{
    float **out=ptr(to,16),**in=ptr(from,16);
    /* Shadow/fallback copies preserve even stale backing bytes: this is an
     * exact native-state clone, not a public PCM view or a meter input. */
    for(unsigned c=0;c<2;++c)memcpy(out[c],in[c],frames*sizeof(float));
    set_clear(to,clear(from));
}
static int overlaps(const void*first,const void*second,unsigned frames)
{
    float **a=ptr(first,16),**b=ptr(second,16);size_t bytes=frames*sizeof(float);
    for(unsigned x=0;x<2;++x)for(unsigned y=0;y<2;++y){
        uintptr_t p=(uintptr_t)a[x],q=(uintptr_t)b[y];
        if(p<q? q-p<bytes:p-q<bytes)return 1;
    }
    return 0;
}
struct mixer_route_native*mixer_route_prepare(const struct mixer_project*p,uint64_t part_id,
    double rate,unsigned capacity,const struct fx_backend*backend,const struct mixer_route_ops*ops,
    enum mixer_bus_result*error)
{
    if(error)*error=MIXER_BUS_INVALID;
    if(!ops||!ops->buffer||!ops->buffer_count||!ops->resolve||!ops->sum||!ops->stage||!ops->render)return NULL;
    struct mixer_buses*b=mixer_buses_prepare(p,part_id,rate,capacity,backend,error);
    if(!b)return NULL;
    struct mixer_route_native*a=calloc(1,sizeof(*a));
    if(a)a->pcm=calloc((size_t)capacity*4,sizeof(float));
    if(!a||!a->pcm){free(a);mixer_buses_destroy(b);if(error)*error=MIXER_BUS_RESOURCE;return NULL;}
    a->buses=b;a->ops=*ops;a->capacity=capacity;a->result=MIXER_ROUTE_INVALID;
    init_buffer(&a->shadow,capacity,a->pcm,a->pcm+capacity);
    init_buffer(&a->scratch,capacity,a->pcm+capacity*2,a->pcm+capacity*3);
    return a;
}
void mixer_route_destroy(struct mixer_route_native*a)
{
    if(a){mixer_buses_destroy(a->buses);free(a->pcm);free(a);}
}
/* Device diagnostic: last reason a block stayed native (0 = captured). */
static _Atomic unsigned last_reject,last_native_route;
unsigned mixer_route_last_native_route(void){return atomic_load_explicit(&last_native_route,memory_order_relaxed);}
unsigned mixer_route_last_reject(void){return atomic_load_explicit(&last_reject,memory_order_relaxed);}
#define REJECT(n) do{atomic_store_explicit(&last_reject,(n),memory_order_relaxed);return 0;}while(0)
static int eligible(struct mixer_route_native*a)
{
    a->main=a->ops.buffer((char*)a->program+0x78,0);
    /* Shared/nonempty program buffers can already contain voices outside the
     * captured list. Reject the complete block before suppressing any sum. */
    if(!stereo(a->main,a->frames))REJECT(1);
    if(!clear(a->main))REJECT(2);
    /* The native sum may add the second channel after clearing the flag.
     * Until native buffer-reset ownership is proved, accept only a genuinely
     * zeroed destination. A clear flag alone is not sufficient evidence. */
    float **main_pcm=ptr(a->main,16);
    for(unsigned c=0;c<2;++c)for(unsigned i=0;i<a->frames;++i)if(main_pcm[c][i]!=0)REJECT(10);
    uintptr_t begin=(uintptr_t)ptr(a->program,0x2820),end=(uintptr_t)ptr(a->program,0x2828);
    if(end<begin||(end-begin)%16||(end-begin)/16>MIXER_PADS)REJECT(3);
    a->count=(unsigned)((end-begin)/16);
    if(a->count&&!begin)REJECT(3);
    for(unsigned i=0;i<a->count;++i){
        void *instrument=ptr((const void*)(begin+i*16),0);
        if(!instrument||ptr(instrument,0x2310)!=a->program||
           word(instrument,0x2168)>=MIXER_PADS||a->ops.buffer_count(instrument)<=1)REJECT(4);
        void *source=a->ops.buffer(instrument,0);
        void *second=a->ops.buffer(instrument,1);
        if(!stereo(source,a->frames)||!stereo(second,a->frames)||source==a->main||
           overlaps(source,a->main,a->frames)||overlaps(second,a->main,a->frames))REJECT(5);
        for(unsigned j=0;j<i;++j)if(a->source[j].instrument==instrument||a->source[j].buffer==source)REJECT(6);
        const char *route=(const char*)instrument+0x560;
        void *destination=a->ops.resolve((char*)a->program+0xc10,route);
        unsigned native=0;
        if(destination){
            float gain;memcpy(&gain,route+24,4);
            /* Device: during playback some Pads route elsewhere than the
             * Program main (e.g. another output pair). Rejecting the whole
             * block dropped every Track and closed the Mixer. A Pad whose
             * destination is another buffer keeps its exact native route
             * (uncaptured, not in a Track); main is still overwritten only
             * by the captured sum, so it cannot be lost there. */
            if(destination==a->main&&(word(route,16)>1||word(route,20)>1||!isfinite(gain)))REJECT(7);
            if(destination!=a->main){
                native=1;atomic_store_explicit(&last_native_route,
                    (unsigned)(word(route,0)&0xff)|(unsigned)(word(route,8)&0xff)<<8|
                    (unsigned)(word(route,16)&0xff)<<16|(unsigned)(word(route,20)&0xff)<<24,memory_order_relaxed);
            }
        } /* null destination uses the native fallback route (stereo unity). */
        a->source[i]=(struct route_source){instrument,source,word(instrument,0x2168),native};
    }
    atomic_store_explicit(&last_reject,0,memory_order_relaxed);
    return 1;
}
static _Atomic unsigned last_recover;
unsigned mixer_route_last_recover(void){return atomic_load_explicit(&last_recover,memory_order_relaxed);}
static void recover_at(struct mixer_route_native*a,unsigned why)
{
    atomic_store_explicit(&last_recover,why,memory_order_relaxed);
    copy_buffer(a->main,a->shadow.native,a->frames);
    a->forwarding=1;a->disabled=1;a->result=MIXER_ROUTE_RECOVERED;
}
#define recover(a) recover_at((a),__LINE__)
enum mixer_route_result mixer_route_render(struct mixer_route_native*a,void*program,void*clock,unsigned frames)
{
    if(!a)return MIXER_ROUTE_INVALID;
    if(!program||!frames){if(scope!=a){a->result=MIXER_ROUTE_INVALID;a->frames=0;}return MIXER_ROUTE_INVALID;}
    if(scope){
        struct mixer_route_native *outer=scope;scope=NULL;
        a->ops.render(program,clock,frames);scope=outer;
        if(a!=outer){a->result=MIXER_ROUTE_PASSTHROUGH;a->frames=frames;a->meter_pushed=0;}
        return MIXER_ROUTE_PASSTHROUGH;
    }
    a->result=MIXER_ROUTE_PASSTHROUGH;a->frames=frames;a->meter_pushed=0;
    if(a->disabled||frames>a->capacity){atomic_store_explicit(&last_reject,a->disabled?8:9,memory_order_relaxed);a->ops.render(program,clock,frames);return a->result;}
    a->program=program;a->captured=0;a->forwarding=0;a->flushed=0;
    if(!eligible(a)){a->ops.render(program,clock,frames);return a->result;}
    if(a->before_block)a->before_block(a->before_user);
    if(mixer_buses_begin(a->buses,frames)!=MIXER_BUS_OK){a->disabled=1;a->ops.render(program,clock,frames);return a->result;}
    copy_buffer(a->shadow.native,a->main,frames);
    scope=a;a->ops.render(program,clock,frames);scope=NULL;
    /* The pinned eligible renderer must reach stage1 exactly once. */
    if(!a->flushed){
        if(!a->captured){a->disabled=1;a->result=MIXER_ROUTE_PASSTHROUGH;}
        else if(!a->forwarding)recover(a);
        mixer_buses_finish(a->buses,NULL,0);
    }
    return a->result;
}
int mixer_route_sum(const struct mixer_route_ops*ops,void*instrument,void*source,void*destination,
    const void*route,unsigned frames)
{
    struct mixer_route_native*a=scope;
    if(!a||a->forwarding||a->flushed)return ops->sum(instrument,source,destination,route,frames);
    struct route_source*s=a->captured<a->count?a->source+a->captured:NULL;
    /* Other programs on the same thread remain native. A known source whose
     * ownership changed is instead a failed capture and must recover first. */
    if(ptr(instrument,0x2310)!=a->program&&(!s||s->instrument!=instrument))
        return ops->sum(instrument,source,destination,route,frames);
    if(s&&s->native&&s->instrument==instrument&&s->buffer==source&&
       ptr(instrument,0x2310)==a->program&&frames==a->frames){
        ++a->captured;return ops->sum(instrument,source,destination,route,frames);
    }
    if(!s||s->instrument!=instrument||s->buffer!=source||destination!=a->main||
       ptr(instrument,0x2310)!=a->program||word(instrument,0x2168)!=s->pad||
       frames!=a->frames||!stereo(source,frames)){
        recover(a);return ops->sum(instrument,source,destination,route,frames);
    }
    /* Native shadow retains original summation order and route mapping/gain.
     * Per-Pad scratch goes through that SAME real native summing routine. */
    int result=ops->sum(instrument,source,a->shadow.native,route,frames);
    /* The native stereo summation clears its destination flag while writing
     * L, then may ADD R. Both backing channels must be zero, even when clear. */
    memset(a->scratch.channel[0],0,frames*sizeof(float));
    memset(a->scratch.channel[1],0,frames*sizeof(float));
    set_clear(a->scratch.native,1);
    ops->sum(instrument,source,a->scratch.native,route,frames);
    enum mixer_bus_result error=mixer_buses_accumulate(a->buses,s->pad,
        a->scratch.channel[0],a->scratch.channel[1],2,clear(a->scratch.native));
    ++a->captured;
    if(error!=MIXER_BUS_OK)recover(a);
    return result;
}
void mixer_route_stage(const struct mixer_route_ops*ops,void*processor,void*buffer,unsigned frames,unsigned stage)
{
    struct mixer_route_native*a=scope;
    if(a&&processor==(char*)a->program+0x78&&!a->flushed){
        if(!a->forwarding&&(buffer!=a->main||frames!=a->frames||stage!=1||a->captured!=a->count))recover(a);
        enum mixer_bus_result error=mixer_buses_finish(a->buses,NULL,0);
        if(!a->forwarding){
            struct mixer_pcm_view view;
            if(error!=MIXER_BUS_OK||!mixer_buses_view(a->buses,MIXER_METER_MASTER,&view))recover(a);
            else {
                float **out=ptr(a->main,16);unsigned quiet=1;
                for(unsigned c=0;c<2;++c){memcpy(out[c],view.channel[c],frames*sizeof(float));
                    for(unsigned i=0;i<frames;++i)if(out[c][i]!=0)quiet=0;}
                set_clear(a->main,quiet);a->result=MIXER_ROUTE_APPLIED;
            }
        }
        a->flushed=1;
    }
    ops->stage(processor,buffer,frames,stage);
}
int mixer_route_view(const struct mixer_route_native*a,unsigned bus,struct mixer_pcm_view*out)
{
    return a&&scope!=a&&a->result==MIXER_ROUTE_APPLIED&&mixer_buses_view(a->buses,bus,out);
}
int mixer_route_meters_push(struct mixer_route_native*a,struct mixer_meters*m,uint64_t epoch)
{
    if(!a||!m||!epoch||!a->frames||a->meter_pushed||scope==a)return -1;
    struct mixer_meter_input taps[MIXER_METER_BUSES]={0};
    if(a->result==MIXER_ROUTE_APPLIED){
        for(unsigned bus=0;bus<=MIXER_METER_DIRECT;++bus){
            struct mixer_pcm_view view;
            if(mixer_buses_view(a->buses,bus,&view)&&view.frames==a->frames){
                taps[bus].channel[0]=view.channel[0];taps[bus].channel[1]=view.channel[1];taps[bus].channels=2;
            }
        }
    }
    int result=mixer_meters_push(m,epoch,a->frames,taps);
    if(result>=0)a->meter_pushed=1;
    return result;
}
void mixer_route_before_block(struct mixer_route_native*a,void(*fn)(void*),void*user)
{
    if(a){a->before_block=fn;a->before_user=user;}
}
struct mixer_buses*mixer_route_buses(struct mixer_route_native*a){return a?a->buses:NULL;}
