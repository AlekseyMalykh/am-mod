/* Run the REAL separate-buffer DrumProgram loop, optimized Pad merge,
 * resolver and sum. Synthetic program/instruments and Pad/Program DSP services.
 * All code patches are confined to this private QEMU ELF mapping. */
#define main unused_route_fx_main
#include "check_fx_native.c"
#undef main
#include "src/mixer_route_native.h"
#define F(p,o,T) (*(T*)((char*)(p)+(o)))
#ifndef N
#define N 16u
#endif
static unsigned in_audio;
void *__real_malloc(size_t);
void *__real_calloc(size_t,size_t);
void *__real_realloc(void*,size_t);
void __real_free(void*);
void *__wrap_malloc(size_t n){assert(!in_audio);return __real_malloc(n);}
void *__wrap_calloc(size_t n,size_t s){assert(!in_audio);return __real_calloc(n,s);}
void *__wrap_realloc(void*p,size_t n){assert(!in_audio);return __real_realloc(p,n);}
void __wrap_free(void*p){assert(!in_audio);__real_free(p);}
static enum mixer_route_result checked_render(struct mixer_route_native*a,void*p,void*c,unsigned n)
{
    assert(!in_audio);in_audio=1;enum mixer_route_result result=mixer_route_render(a,p,c,n);in_audio=0;return result;
}
#define mixer_route_render checked_render
static struct mixer_route_ops ops;
static unsigned pad_calls,stage_calls,optimized_calls,expected_pads;
static int poison;
static void *expected_program;
static unsigned char program[0x2900],instruments[3][0x2400];
static unsigned char pad_buffers[3][2][0x130];
static unsigned char containers[4][24],active[3][16],physical[0x130];
static float pad_pcm[3][2][2][N],main_pcm[2][N],physical_pcm[2][N];
static float *pad_channels[3][2][2],*main_channels[2],*physical_channels[2];
static void *vtable[16];
static unsigned effects[3];
static unsigned change_owner,remove_idle;
static unsigned index_of(void *instrument)
{
    for(unsigned i=0;i<3;++i)if(instrument==instruments[i])return i;
    abort();
}
static void pad_process(void*instrument,void*buffer,unsigned frames,unsigned fx)
{
    assert(frames==N&&stage_calls==0);++pad_calls;
    unsigned index=index_of(instrument);effects[index]|=1u<<fx;
    float **pcm=F(buffer,16,float**);
    if(!F(buffer,0x128,unsigned char))
        for(unsigned c=0;c<2;++c)for(unsigned i=0;i<frames;++i)pcm[c][i]*=2;
    if(poison&&index==0)pcm[0][0]=NAN;
    if(change_owner&&index==1)F(instrument,0x2310,void*)=NULL;
}
static void pad_virtual(void*i,void*b,unsigned n){pad_process(i,b,n,0);}
static int idle(void*instrument){return !(remove_idle&&instrument==instruments[1]);}
static int no_extra_fx(void*p){(void)p;++optimized_calls;return 0;}
static void program_stage(void*processor,void*buffer,unsigned frames,unsigned stage)
{
    assert(processor==(char*)expected_program+0x78&&frames==N);
    assert(buffer==program+0xc20);
    assert(pad_calls==expected_pads);
    assert(stage==stage_calls+1);++stage_calls;
    /* Native program FX remain AFTER the adapter's return buffer. */
    if(stage==3&&!F(buffer,0x128,unsigned char)){
        float **p=F(buffer,16,float**);
        for(unsigned c=0;c<2;++c)for(unsigned i=0;i<frames;++i)p[c][i]*=.5f;
    }
}
static void passthrough_stage(void*p,void*b,unsigned n){(void)p;(void)b;assert(n==N);}
static int sum_dispatch(void*i,void*s,void*d,const void*r,unsigned n){return mixer_route_sum(&ops,i,s,d,r,n);}
static void stage_dispatch(void*p,void*b,unsigned n,unsigned s){mixer_route_stage(&ops,p,b,n,s);}
static void jump(uintptr_t offset,void*fn)
{
    uint32_t words[]={0xd503245f,0x58000070,0xd61f0200,0xd503201f};
    memcpy((void*)(base+offset),words,sizeof(words));memcpy((void*)(base+offset+16),&fn,8);
    __builtin___clear_cache((char*)(base+offset),(char*)(base+offset+24));
}
static void call(uintptr_t site,uint32_t expected,uintptr_t relay,void*fn)
{
    assert(*(uint32_t*)(base+site)==expected);
    jump(relay,fn);intptr_t delta=(intptr_t)relay-(intptr_t)site;
    assert(!(delta&3)&&delta>=-0x8000000&&delta<0x8000000);
    *(uint32_t*)(base+site)=0x94000000u|((uint32_t)(delta/4)&0x03ffffffu);
    __builtin___clear_cache((char*)(base+site),(char*)(base+site+4));
}
static void buffer(void*b,float**channels)
{
    memset(b,0,0x130);F(b,0,unsigned)=2;F(b,4,unsigned)=N;F(b,16,float**)=channels;
    F(b,0x128,unsigned char)=1;
}
static void fixture(void)
{
    memset(program,0,sizeof(program));memset(instruments,0,sizeof(instruments));
    memset(containers,0,sizeof(containers));memset(pad_pcm,0,sizeof(pad_pcm));
    memset(main_pcm,0,sizeof(main_pcm));memset(physical_pcm,0,sizeof(physical_pcm));
    memset(effects,0,sizeof(effects));pad_calls=stage_calls=optimized_calls=0;poison=0;change_owner=remove_idle=0;expected_pads=3;
    expected_program=program;vtable[3]=idle;vtable[6]=pad_virtual;
    for(unsigned c=0;c<2;++c){main_channels[c]=main_pcm[c];physical_channels[c]=physical_pcm[c];}
    buffer(program+0xc20,main_channels);buffer(physical,physical_channels);
    F(program,0xc18,void*)=physical;F(program,0xd50,unsigned)=1;
    F(program,0xba8,void*)=containers[3];F(containers[3],8,void*)=program+0xc20;
    F(containers[3],16,void*)=program+0xc20+0x130;
    F(program,0x2820,void*)=active;F(program,0x2828,void*)=(char*)active+sizeof(active);
    F(program,0x990,float)=1;
    for(unsigned p=0;p<3;++p){
        F(instruments[p],0,void*)=vtable;F(instruments[p],0x2310,void*)=program;
        F(instruments[p],0x2168,unsigned)=p==2?127:p;
        F(instruments[p],0xb30,void*)=containers[p];
        F(containers[p],8,void*)=pad_buffers[p];F(containers[p],16,void*)=pad_buffers[p]+2;
        F(active[p],0,void*)=instruments[p];F(active[p],8,unsigned)=1;
        unsigned route[8]={1,0,3,0,0,1,0x3f000000,0};memcpy(instruments[p]+0x560,route,sizeof(route));
        for(unsigned b=0;b<2;++b){
            for(unsigned c=0;c<2;++c)pad_channels[p][b][c]=pad_pcm[p][b][c];
            buffer(pad_buffers[p][b],pad_channels[p][b]);
        }
        F(pad_buffers[p][0],0x128,unsigned char)=0;
        for(unsigned n=0;n<N;++n){pad_pcm[p][0][0][n]=(p+1)*.125f;pad_pcm[p][0][1][n]=-(float)(p+1)*.0625f;}
    }
}
static struct mixer_route_native*prepare(float db)
{
    struct mixer_project project;mixer_defaults(&project);
    project.output[0]=1;project.output[1]=2; /* bank H Pad16 remains Direct */
    project.track[0].mix.level_db=db;enum mixer_bus_result error;
    struct mixer_route_native*a=mixer_route_prepare(&project,0,48000,N,NULL,&ops,&error);
    assert(a&&error==MIXER_BUS_OK);return a;
}
static void expect_audio(float left,float right)
{
    if(fabsf(main_pcm[0][0]-left)>=1e-7f||fabsf(main_pcm[1][0]-right)>=1e-7f)
        fprintf(stderr,"audio got %.9g %.9g expected %.9g %.9g pad=%u optimized=%u stage=%u\n",
            (double)main_pcm[0][0],(double)main_pcm[1][0],(double)left,(double)right,pad_calls,optimized_calls,stage_calls);
    for(unsigned n=0;n<N;++n){assert(fabsf(main_pcm[0][n]-left)<1e-7f);assert(fabsf(main_pcm[1][n]-right)<1e-7f);}
    assert(stage_calls==3);
}
struct tail {float value[2];unsigned id;};
static void*fx_create(void*user,uint32_t id,double rate,const float*parameter)
{
    (void)user;(void)parameter;assert(rate==48000);
    struct tail*t=calloc(1,sizeof(*t));assert(t);t->id=id;return t;
}
static void fx_destroy(void*user,void*instance){(void)user;free(instance);}
static void fx_process(void*user,void*instance,float*pcm,size_t frames)
{
    (void)user;struct tail*t=instance;
    for(unsigned c=0;c<2;++c){float input=pcm[c];
        for(size_t n=0;n<frames;++n)pcm[n*2+c]=t->id==2?NAN:pcm[n*2+c]+t->value[c]*.5f;
        t->value[c]=input;
    }
}
static struct mixer_route_native*prepare_fx(unsigned id)
{
    struct mixer_project p;mixer_defaults(&p);p.output[0]=1;
    p.track[0].fx.count=1;p.track[0].fx.stage[0].kind=FX_STAGE_NATIVE;p.track[0].fx.stage[0].native_id=id;
    struct fx_backend backend={NULL,fx_create,fx_destroy,fx_process};enum mixer_bus_result error;
    struct mixer_route_native*a=mixer_route_prepare(&p,0,48000,N,&backend,&ops,&error);assert(a&&error==MIXER_BUS_OK);return a;
}
static void tests(void)
{
    fixture();ops.render(program,NULL,N);expect_audio(.375f,-.1875f);
    float baseline[2][N];memcpy(baseline,main_pcm,sizeof(baseline));
    struct mixer_route_native*a=prepare(0);fixture();
    assert(mixer_route_render(a,program,NULL,N)==MIXER_ROUTE_APPLIED);
    expect_audio(.375f,-.1875f);assert(!memcmp(baseline,main_pcm,sizeof(baseline)));
    struct mixer_pcm_view v;assert(mixer_route_view(a,0,&v)&&v.channel[0][0]==.125f);
    assert(mixer_route_view(a,1,&v)&&v.channel[0][0]==.25f);
    assert(mixer_route_view(a,8,&v)&&v.channel[0][0]==.375f);
    assert(mixer_route_view(a,9,&v)&&v.channel[0][0]==.75f); /* before program FX */
    fixture();remove_idle=1;F(active[1],8,unsigned)=0;
    assert(mixer_route_render(a,program,NULL,N)==MIXER_ROUTE_APPLIED);expect_audio(.375f,-.1875f);
    assert(F(program,0x2828,void*)==(char*)active+32&&F(active[1],0,void*)==instruments[2]);
    mixer_route_destroy(a);
    a=prepare(-6.0206f);fixture();assert(mixer_route_render(a,program,NULL,N)==MIXER_ROUTE_APPLIED);
    expect_audio(.34375f,-.171875f);mixer_route_destroy(a);
    puts("PASS real native Pad loop, route gain, three independent outputs and unity baseline without double summing; native Program FX run afterwards");

    /* Actual optimized 2803470 processes both buffers and merges them before
     * the same native per-Pad sum. Only its external DSP services are fixtures. */
    a=prepare(0);fixture();F(instruments[0],0x21c0,unsigned char)=1;expected_pads=4;
    F(pad_buffers[0][1],0x128,unsigned char)=0;
    for(unsigned n=0;n<N;++n){pad_pcm[0][1][0][n]=.0625f;pad_pcm[0][1][1][n]=-.03125f;}
    assert(mixer_route_render(a,program,NULL,N)==MIXER_ROUTE_APPLIED);
    expect_audio(.40625f,-.203125f);assert(optimized_calls==1&&effects[0]==3);
    assert(mixer_route_view(a,0,&v)&&v.channel[0][0]==.1875f);mixer_route_destroy(a);
    puts("PASS real optimized native two-buffer Pad merge converges on exactly one captured Pad sum");

    a=prepare(-6.0206f);fixture();F(program+0xc20,0x128,unsigned char)=0;
    for(unsigned n=0;n<N;++n){main_pcm[0][n]=.25f;main_pcm[1][n]=-.125f;}
    assert(mixer_route_render(a,program,NULL,N)==MIXER_ROUTE_PASSTHROUGH);expect_audio(.5f,-.25f);
    assert(!mixer_route_view(a,0,&v));
    fixture();F(instruments[0],0x560,unsigned)=2;
    assert(mixer_route_render(a,program,NULL,N)==MIXER_ROUTE_PASSTHROUGH);expect_audio(.3125f,-.15625f);
    assert(physical_pcm[0][0]==.125f&&physical_pcm[1][0]==-.0625f);
    fixture();F(containers[0],16,void*)=pad_buffers[0]+1;
    assert(mixer_route_render(a,program,NULL,N)==MIXER_ROUTE_PASSTHROUGH);
    assert(!stage_calls&&!pad_calls); /* real early return, not a fake zero mix */
    fixture();F(instruments[0],0x560,unsigned)=99;
    assert(mixer_route_render(a,program,NULL,N)==MIXER_ROUTE_APPLIED);expect_audio(.375f,-.1875f);
    mixer_route_destroy(a);
    puts("PASS whole-block native fallback for shared/physical-output paths and real one-buffer early return; native unknown-route unity fallback preserved");

    /* Stereo route mapping (including both inputs to one output) is owned by
     * the native summation, not reimplemented as a guessed gain/pan law. */
    for(unsigned left=0;left<2;++left)for(unsigned right=0;right<2;++right){
        a=prepare(0);fixture();F(instruments[0],0x570,unsigned)=left;F(instruments[0],0x574,unsigned)=right;
        F(instruments[0],0x578,float)=-.25f;ops.render(program,NULL,N);memcpy(baseline,main_pcm,sizeof(baseline));
        fixture();F(instruments[0],0x570,unsigned)=left;F(instruments[0],0x574,unsigned)=right;
        F(instruments[0],0x578,float)=-.25f;
        assert(mixer_route_render(a,program,NULL,N)==MIXER_ROUTE_APPLIED);
        assert(!memcmp(baseline,main_pcm,sizeof(baseline)));mixer_route_destroy(a);
    }
    puts("PASS native stereo route swaps, summed-channel destinations and negative route gain match unchanged native PCM");

    a=prepare(0);fixture();poison=1;
    assert(mixer_route_render(a,program,NULL,N)==MIXER_ROUTE_RECOVERED);
    assert(isnan(main_pcm[0][0])); /* exact native behavior, no invented silence */
    for(unsigned n=1;n<N;++n)assert(main_pcm[0][n]==.375f);
    assert(main_pcm[1][0]==-.1875f&&!mixer_route_view(a,0,&v));
    fixture();assert(mixer_route_render(a,program,NULL,N)==MIXER_ROUTE_PASSTHROUGH);
    expect_audio(.375f,-.1875f);mixer_route_destroy(a);
    puts("PASS failed captured block restores actual native shadow sum, forwards remaining Pads and stays disabled until reprepare");

    a=prepare(0);fixture();change_owner=1;
    assert(mixer_route_render(a,program,NULL,N)==MIXER_ROUTE_RECOVERED);expect_audio(.375f,-.1875f);mixer_route_destroy(a);
    a=prepare_fx(2);fixture();assert(mixer_route_render(a,program,NULL,N)==MIXER_ROUTE_RECOVERED);
    expect_audio(.375f,-.1875f);assert(!mixer_route_view(a,9,&v));mixer_route_destroy(a);
    puts("PASS unexpected source ownership change and finish-time FX failure both recover complete native audio before Program stages");

    a=prepare(0);fixture();F(program+0xc20,0x128,unsigned char)=1;
    for(unsigned n=0;n<N;++n){main_pcm[0][n]=.25f;main_pcm[1][n]=-.125f;}
    assert(mixer_route_render(a,program,NULL,N)==MIXER_ROUTE_PASSTHROUGH);expect_audio(.375f,-.25f);
    fixture();pad_channels[0][0][0]=main_pcm[0];
    assert(mixer_route_render(a,program,NULL,N)==MIXER_ROUTE_PASSTHROUGH);
    assert(!mixer_route_view(a,9,&v));mixer_route_destroy(a);
    puts("PASS stale clear backing-store and aliased shared PCM reject the whole block without capturing it");

    a=prepare_fx(1);fixture();assert(mixer_route_render(a,program,NULL,N)==MIXER_ROUTE_APPLIED);
    expect_audio(.375f,-.1875f);
    fixture();F(program,0x2828,void*)=active;expected_pads=0;
    assert(mixer_route_render(a,program,NULL,N)==MIXER_ROUTE_APPLIED);expect_audio(.03125f,-.015625f);
    mixer_route_destroy(a);
    puts("PASS independent Track FX tails continue into empty native blocks through the normal Program stages");

    a=prepare(0);fixture();F(program,0x2828,void*)=active;expected_pads=0;
    assert(mixer_route_render(a,program,NULL,N)==MIXER_ROUTE_APPLIED);expect_audio(0,0);
    assert(F(program+0xc20,0x128,unsigned char)==1);mixer_route_destroy(a);
    puts("PASS empty eligible block runs graph/tail stage and native Program stages; silence clear flag preserved");

    struct mixer_meters*meters=mixer_meters_create(48000);assert(meters);struct mixer_meter_snapshot snapshot;
    a=prepare(0);assert(mixer_route_meters_push(a,meters,1)==-1);
    for(unsigned block=0;block<6;++block){
        fixture();assert(mixer_route_render(a,program,NULL,N)==MIXER_ROUTE_APPLIED);
        assert(mixer_route_meters_push(a,meters,1)==(block<4?1:0));
        assert(mixer_route_meters_push(a,meters,1)==-1);
    }
    assert(mixer_meters_take(meters,1,&snapshot)&&snapshot.frame_end==4*N&&snapshot.valid_mask==0x1ff);
    fixture();assert(mixer_route_render(a,program,NULL,N)==MIXER_ROUTE_APPLIED);
    assert(mixer_route_meters_push(a,meters,1)==1&&mixer_meters_take(meters,1,&snapshot));
    assert(snapshot.frame_end==7*N&&snapshot.peak[0][0]==.125f);
    fixture();poison=1;assert(mixer_route_render(a,program,NULL,N)==MIXER_ROUTE_RECOVERED);
    assert(mixer_route_meters_push(a,meters,2)==1&&mixer_meters_take(meters,2,&snapshot));
    assert(snapshot.frame_end==N&&snapshot.valid_mask==0&&snapshot.peak[0][0]==0);
    mixer_meters_destroy(meters);mixer_route_destroy(a);
    puts("PASS adapter meter bridge preserves block clock under queue backpressure, rejects retry and invalidates recovered audio in a new epoch");

    /* With both call sites absent, a mistaken scoped invocation must leave
     * the native render result intact rather than copying an empty shadow. */
    *(uint32_t*)(base+0x286bf44)=0x97fbf72b;*(uint32_t*)(base+0x286bfa4)=0x97fbf8db;
    __builtin___clear_cache((char*)(base+0x286bf44),(char*)(base+0x286bfa8));
    a=prepare(-6.0206f);fixture();assert(mixer_route_render(a,program,NULL,N)==MIXER_ROUTE_PASSTHROUGH);
    expect_audio(.375f,-.1875f);mixer_route_destroy(a);
    puts("PASS uninstalled call sites leave an accidental scoped invocation entirely native");
}
#ifndef MIXER_ROUTE_PROOF_MAIN
#define MIXER_ROUTE_PROOF_MAIN main
#endif
int MIXER_ROUTE_PROOF_MAIN(int argc,char**argv)
{
    assert(argc==2);setbuf(stdout,NULL);struct sigaction sa={.sa_sigaction=fault,.sa_flags=SA_SIGINFO};
    sigaction(SIGSEGV,&sa,NULL);sigaction(SIGILL,&sa,NULL);sigaction(SIGABRT,&sa,NULL);
    load(argv[1]);phase="scoped native routing";
    ops=(struct mixer_route_ops){FN(0x276abc0,void*(*)(void*,unsigned long)),
        FN(0x276abe0,unsigned long(*)(void*)),FN(0x18ccbc0,void*(*)(void*,const void*)),
        FN(0x2769bf0,int(*)(void*,void*,void*,const void*,unsigned)),
        FN(0x276a310,void(*)(void*,void*,unsigned,unsigned)),FN(0x286be30,void(*)(void*,void*,unsigned))};
    call(0x286bf44,0x97fbf72b,0x7400000,sum_dispatch);
    call(0x286bfa4,0x97fbf8db,0x7400040,stage_dispatch);
    jump(0x276a310,program_stage);jump(0x276a520,passthrough_stage);jump(0x2769d60,passthrough_stage);
    jump(0x2802f90,pad_process);jump(0x2760a40,no_extra_fx);
    tests();puts("PASS linked adapter/core performs no malloc/calloc/realloc/free during scoped audio callbacks");
    puts("PASS scoped Mixer native adapter (isolated call-site patches only; no installed live routing)");return 0;
}
