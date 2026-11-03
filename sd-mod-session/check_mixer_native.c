/* Phase 0: execute pinned native routing/summing/export entry points in QEMU.
 * Fixtures supply PCM, router storage and services; no live device is opened.
 * Firmware is mapped privately by the existing native test loader. */
#define main unused_mixer_fx_main
#include "check_fx_native.c"
#undef main
#include "src/mixer_meters.h"
#define F(p,o,T) (*(T*)((char*)(p)+(o)))

static void routing(void)
{
    unsigned char router[0x168]={0},physical[2*0x130]={0},before[sizeof(router)];
    F(router,8,void*)=physical;F(router,0x140,int)=2;
    memcpy(before,router,sizeof(router));
    void*(*resolve)(void*,const void*)=FN(0x18ccbc0,void*(*)(void*,const void*));
    for(int destination=-1;destination<=10;++destination)
        for(int index=-2;index<=10;++index){
            int route[8]={destination,index,3,0,0,1,0x3f800000,0};
            void *expected=destination==1&&index==0?router+0x10:
                destination==2&&index>=0&&index<2?physical+index*0x130:NULL;
            assert(resolve(router,route)==expected);
        }
    assert(!memcmp(before,router,sizeof(router)));
    puts("PASS native route resolver: only Master destination1/index0 and bounded physical destination2; no eight implicit Track buses");
}

static void summing(void)
{
    unsigned char source[0x130]={0},destination[0x130]={0};
    float left[16],right[16],out_l[16]={0},out_r[16]={0};
    float *in[]={left,right},*out[]={out_l,out_r};
    for(unsigned i=0;i<16;++i){left[i]=.25f;right[i]=-.125f;}
    F(source,0,int)=F(destination,0,int)=2;
    F(source,4,int)=F(destination,4,int)=16;
    F(source,16,void*)=in;F(destination,16,void*)=out;
    unsigned route[6]={3,0,0,1,0,0};float gain=.5f;memcpy(route+4,&gain,4);
    int(*mix)(void*,void*,void*,void*,unsigned)=FN(0x2769bf0,int(*)(void*,void*,void*,void*,unsigned));
    assert(mix(NULL,source,destination,route,16));
    for(unsigned i=0;i<16;++i){assert(out_l[i]==.125f&&out_r[i]==-.0625f);assert(left[i]==.25f&&right[i]==-.125f);}
    gain=1;memcpy(route+4,&gain,4);assert(mix(NULL,source,destination,route,16));
    for(unsigned i=0;i<16;++i)assert(out_l[i]==.375f&&out_r[i]==-.1875f);
    float saved_l[16],saved_r[16];memcpy(saved_l,out_l,sizeof(out_l));memcpy(saved_r,out_r,sizeof(out_r));
    F(source,0x128,unsigned char)=1;
    assert(!mix(NULL,source,destination,route,16));
    assert(!memcmp(saved_l,out_l,sizeof(out_l))&&!memcmp(saved_r,out_r,sizeof(out_r)));
    F(source,0x128,unsigned char)=0;gain=0;memcpy(route+4,&gain,4);
    assert(mix(NULL,source,destination,route,16));
    assert(!memcmp(saved_l,out_l,sizeof(out_l))&&!memcmp(saved_r,out_r,sizeof(out_r)));
    /* Measure the PCM just produced by real native summing, not a gain,
     * note event or guessed engine bus. This test chooses telemetry slot0. */
    struct mixer_meters*meters=mixer_meters_create(48000);assert(meters);
    struct mixer_meter_input taps[MIXER_METER_BUSES]={0};
    taps[0]=(struct mixer_meter_input){{out_l,out_r},2,0};struct mixer_meter_snapshot snapshot;
    assert(mixer_meters_push(meters,1,16,taps)==1&&mixer_meters_take(meters,1,&snapshot));
    assert(snapshot.valid_mask==1&&snapshot.peak[0][0]==.375f&&snapshot.peak[0][1]==.1875f);
    mixer_meters_destroy(meters);
    puts("PASS actual native stereo summing: route gain, accumulation, clear-source suppression and immutable source PCM");
    puts("PASS new meter core measures actual native summed PCM; native live tap not installed");
}

static unsigned export_calls,export_source;
static void *expected_runner,*expected_path,*expected_properties,*expected_callback;
static void export_capture(void*runner,void*path,void*properties,unsigned source,
    unsigned selection,unsigned a,unsigned b,unsigned rate,unsigned depth,void*callback)
{
    assert(runner==expected_runner&&path==expected_path&&properties==expected_properties&&callback==expected_callback);
    assert(!selection&&a==1&&b==1&&rate==48000&&depth==24);
    ++export_calls;export_source=source;
}
static void exports(void)
{
    /* Replace only the dispatch body in this isolated mapping. The original
     * Seq/Song entry wrappers themselves execute unchanged. */
    uint32_t jump[]={0x58000050,0xd61f0200};void*target=export_capture;
    memcpy((void*)(base+0x18c5a00),jump,sizeof(jump));
    memcpy((void*)(base+0x18c5a08),&target,sizeof(target));
    __builtin___clear_cache((char*)(base+0x18c5a00),(char*)(base+0x18c5a10));
    unsigned char objects[4]={0};expected_runner=objects;expected_path=objects+1;
    expected_properties=objects+2;expected_callback=objects+3;
    typedef void(*Export)(void*,void*,void*,unsigned,unsigned,unsigned,void*);
    FN(0x18c6330,Export)(expected_runner,expected_path,expected_properties,99,48000,24,expected_callback);
    assert(export_calls==1&&export_source==0);
    FN(0x18c62f0,Export)(expected_runner,expected_path,expected_properties,99,48000,24,expected_callback);
    assert(export_calls==2&&export_source==2);
    puts("PASS unchanged native Song export source0 and Seq export source2 delegate to the same dispatch; services/path/rate/depth/callback preserved");
}

int main(int argc,char**argv)
{
    assert(argc==2);struct sigaction sa={.sa_sigaction=fault,.sa_flags=SA_SIGINFO};
    sigaction(SIGSEGV,&sa,NULL);sigaction(SIGILL,&sa,NULL);sigaction(SIGABRT,&sa,NULL);
    load(argv[1]);phase="Mixer native resolver";routing();
    phase="Mixer native summing";summing();phase="Song export entry wrappers";exports();
    puts("PASS Mixer Phase0 native audit (synthetic buffers, not a live eight-bus mixer)");
    return 0;
}
