/* This backend is a test adapter, not a registration in AC50 FX Select.
 * It executes the actual legacy Parametric EQ81, not modern PEQ4-Band. */
#define main fx_contract_main
#include "check_fx_native.c"
#undef main
#include "src/pad_render.h"
static unsigned created,destroyed,processed;
static void *effect_create(void*u,uint32_t id,double rate,const float*p)
{
    (void)u;if(id!=81)return NULL;
    void*eq=FN(0x24c8290,void*(*)(int,char*))(81,NULL);if(!eq)return NULL;
    uintptr_t*v=*(uintptr_t**)eq;assert((uintptr_t)v==base+0x6db4450);
    for(int i=0;i<8;++i){
        ((void(*)(void*,int,float))v[6])(eq,i,p[i]);
        assert(fabsf(((float(*)(void*,int))v[7])(eq,i)-p[i])<.000001f);
    }
    ((void(*)(void*,float))v[2])(eq,(float)rate);++created;return eq;
}
static void effect_destroy(void*u,void*eq){(void)u;uintptr_t*v=*(uintptr_t**)eq;((void(*)(void*))v[1])(eq);++destroyed;}
static void effect_process(void*u,void*eq,float*pcm,size_t n)
{
    (void)u;assert(n>0&&n<=PAD_RENDER_BLOCK_FRAMES);
    uintptr_t*v=*(uintptr_t**)eq;unsigned char quiet=0;
    ((void(*)(void*,float*,int,unsigned char*))v[5])(eq,pcm,(int)n,&quiet);++processed;
}
static const struct fx_backend native_backend={NULL,effect_create,effect_destroy,effect_process};
static struct pad_render_job*render(const float*signal,uint64_t frames,struct pad_render_request*r)
{
    struct pad_render_source source={signal,frames,2,48000};enum pad_render_status error;
    struct pad_render_job*j=pad_render_create(&source,r,&native_backend,&error);assert(j&&error==PAD_RENDER_WORKING);
    enum pad_render_status result;do{result=pad_render_step(j,1024);}while(result==PAD_RENDER_WORKING);
    assert(result==PAD_RENDER_DONE);return j;
}
int main(int argc,char**argv)
{
    assert(!fx_contract_main(argc,argv));phase="native EQ chain inside bounded pad renderer";
    float signal[2006],before[2006];
    for(unsigned f=0;f<1003;++f){signal[f*2]=.2f*sinf(f*.037f);signal[f*2+1]=.1f*cosf(f*.3f);}
    memcpy(before,signal,sizeof(signal));
    struct pad_render_request r={.first=0,.end=1003,.repeats=2,.tail_frames=257};
    struct fx_stage native={.kind=FX_STAGE_NATIVE,.native_id=81};
    for(unsigned i=0;i<8;++i)native.parameter[i]=.5f;
    native.parameter[2]=.8f;
    assert(fx_chain_add(&r.chain,&native,48000)==FX_OK);
    struct fx_stage eq={.kind=FX_STAGE_EQ};fx_eq_default(&eq.eq);eq.eq.band[1].gain_db=6;
    assert(fx_chain_add(&r.chain,&eq,48000)==FX_OK);
    struct pad_render_job*j=render(signal,1003,&r);
    uint64_t frames;double rate;const float*out=pad_render_output(j,&frames,&rate);
    assert(frames==2263&&rate==48000&&created==destroyed&&processed);
    double difference=0;for(unsigned f=0;f<2006;++f){assert(isfinite(out[f]));difference+=fabs(out[f]-signal[f]);}
    assert(difference>.0001&&!memcmp(signal,before,sizeof(signal)));
    struct pad_render_job*k=render(signal,1003,&r);
    const float*again=pad_render_output(k,NULL,NULL);assert(!memcmp(out,again,(size_t)frames*2*sizeof(float)));
    pad_render_destroy(j);pad_render_destroy(k);
    r.chain.count=FX_CHAIN_MAX;for(unsigned i=0;i<FX_CHAIN_MAX;++i){r.chain.stage[i]=native;r.chain.stage[i].bypass=1;}
    unsigned runs=processed;j=render(signal,1003,&r);out=pad_render_output(j,&frames,&rate);
    assert(processed==runs&&created==destroyed);
    for(unsigned f=0;f<2263;++f)for(unsigned c=0;c<2;++c)assert(out[f*2+c]==(f<2006?signal[(f%1003)*2+c]:0));
    pad_render_destroy(j);
    puts("PASS bounded renderer executes real native EQ81 plus independent4-band DSP in sequence, finite repeated stereo input/tail, reproducible fresh rerender, source immutable, five occupied bypassed slots skip DSP and release all native instances; no device Program/FX Select/assignment/preview/save integration");
    return 0;
}
