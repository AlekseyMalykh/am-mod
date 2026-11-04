#define MIXER_DSP_PROOF_MAIN unused_live_dsp_main
#include "check_mixer_route_dsp.c"
#include "src/mixer_live.h"
static unsigned char engine_owner[0x160],root_mixer[0x200],root_router[0x168],root_output[0x130];
static float root_pcm[2][N],root_master[2][N],*root_channels[2],*root_master_channels[2];
static void*program_list[1];
static unsigned engine_pre,engine_mid,downstream;
static unsigned detach_inside;
static void service(uintptr_t offset,void*fn)
{
    void*page=(void*)((base+offset)&~(uintptr_t)4095);
    assert(!mprotect(page,4096,PROT_READ|PROT_WRITE));jump(offset,fn);
    assert(!mprotect(page,4096,PROT_READ|PROT_EXEC));
}
static void reset_engine(void*cache,void*clock,unsigned frames)
{
    assert(cache==engine_owner&&clock==entry_clock&&frames==N);++engine_pre;
    for(unsigned i=0;i<4;++i)FN(0x276a830,void(*)(void*))(native_collection[i]);
    memset(root_pcm,0,sizeof(root_pcm));memset(root_master,0,sizeof(root_master));
    F(root_output,0x128,unsigned char)=F(root_router+16,0x128,unsigned char)=1;
}
static void events(void*e,unsigned frames){assert(e==engine_owner&&frames==N);++engine_mid;
    if(detach_inside){assert(mixer_live_native_attach(NULL));assert(!mixer_live_native_take_retired());}}
static int route_program(void*p,void*unused,void*router,unsigned frames)
{
    (void)unused;assert(p==program&&router==root_router&&frames==N);
    return FN(0x28a17f0,int(*)(void*,void*,void*,unsigned))(p,ops.buffer(program+0x78,0),router,frames);
}
static void render_list(unsigned frames,void*list,void*router,void*clock)
{
    assert(frames==N&&router==root_router&&clock==entry_clock);
    if(list==root_mixer+0x100)FN(0x286c3b0,void(*)(void*,void*,unsigned))(program,clock,frames);
    else {assert(list==root_mixer+0x118);++downstream;
        /* Downstream insert fixture and an independent physical route. The
         * new Master must see their final combination exactly once. */
        for(unsigned i=0;i<N;++i){root_pcm[0][i]=root_pcm[0][i]*.75f+.01f;root_pcm[1][i]=root_pcm[1][i]*.75f-.005f;}}
}
static void live_fixture(void)
{
    entry_fixture();F(program,0x1234,unsigned)=8;program_table[0x1c8/8]=(uintptr_t)route_program;
    unsigned route[8]={2,0,3,0,0,1,0x3f800000,0};memcpy(program+0x5d8,route,sizeof(route));
    memset(root_mixer,0,sizeof(root_mixer));memset(root_router,0,sizeof(root_router));memset(engine_owner,0,sizeof(engine_owner));
    for(unsigned c=0;c<2;++c){root_channels[c]=root_pcm[c];root_master_channels[c]=root_master[c];}
    buffer(root_output,root_channels);buffer(root_router+16,root_master_channels);
    F(root_router,8,void*)=root_output;F(root_router,0x140,unsigned)=1;
    program_list[0]=program;F(root_mixer,0x100,void*)=program_list;F(root_mixer,0x108,void*)=program_list+1;
    F(root_mixer,0x130,void*)=root_router;F(engine_owner,0x110,void*)=root_mixer;F(engine_owner,0x130,void*)=engine_owner;
    F(engine_owner,0x128,void*)=entry_clock;
}
static void run_live(void)
{
    in_audio=1;FN(0x2226fb0,void(*)(void*,void*,unsigned))(engine_owner,entry_clock,N);in_audio=0;
}
static void tests_live(void)
{
    service(0x220a4a0,reset_engine);service(0x2226d10,events);service(0x18c6990,render_list);service(0x221ee10,voice_chunk);
    service(0x2760ae0,native_source);assert(pad_eq_native_install(base));
    use_voice_pcm=1;pad_eq_publish(0,pad_eq_edit(pad_eq_default(),PAD_EQ_GAIN,22,0));
    live_fixture();pad_eq_native_program(program);run_live();float baseline[2][N];memcpy(baseline,root_pcm,sizeof(baseline));
    assert(energy(root_pcm[0])>0&&engine_pre==1&&engine_mid==1&&downstream==1);
    struct mixer_project p;mixer_defaults(&p);p.output[0]=1;p.output[1]=2;enum mixer_bus_result error;
    p.master.mix.level_db=-6.0206f;
    struct mixer_live*s=mixer_live_prepare(&p,0,48000,N,11,engine_owner,program,mixer_live_native_ops(),NULL,&error);assert(s&&error==MIXER_BUS_OK);
    assert(mixer_live_native_attach(s));live_fixture();run_live();compare_pcm(root_pcm[0],baseline[0],.5f);compare_pcm(root_pcm[1],baseline[1],.5f);
    struct mixer_meter_snapshot snapshot;assert(mixer_live_take_meter(s,&snapshot)&&snapshot.valid_mask==0x3ff);
    for(unsigned c=0;c<2;++c){float peak=0;for(unsigned i=0;i<N;++i)peak=fmaxf(peak,fabsf(root_pcm[c][i]));assert(snapshot.peak[9][c]==peak);}
    assert(engine_pre==2&&engine_mid==2&&downstream==2);
    p.track[0].mix.level_db=-6.0206f;p.output[1]=3;p.master.mix.level_db=0;
    assert(mixer_live_publish(s,&p,0,1)==MIXER_BUS_OK);
    assert(mixer_live_publish(s,&p,0,1)==MIXER_BUS_INVALID);
    live_fixture();run_live();assert(mixer_live_applied_revision(s)==1);
    assert(mixer_live_take_meter(s,&snapshot)&&snapshot.valid_mask==0x3ff);
    float**a=F(ops.buffer(instruments[0],0),16,float**),**b=F(ops.buffer(instruments[1],0),16,float**),**d=F(ops.buffer(instruments[2],0),16,float**);
    for(unsigned i=240;i<N;++i)assert(fabsf(root_pcm[0][i]-((a[0][i]*.5f+b[0][i]+d[0][i])*.5f*.75f+.01f))<2e-7f);
    mixer_live_collect(s);detach_inside=1;live_fixture();run_live();detach_inside=0;
    assert(mixer_live_native_take_retired()==s);mixer_live_destroy(s);
    live_fixture();run_live();compare_pcm(root_pcm[0],baseline[0],1);assert(engine_pre==5&&engine_mid==5&&downstream==5);
    pad_eq_native_clear();puts("PASS installed pinned live hooks: native2226fb0/18c79f0 downstream and actual Program summing, final physical Master gain/meter, coherent Track/routing/Master update and native detach");
}
int main(int argc,char**argv)
{
    assert(argc==2);setbuf(stdout,NULL);struct sigaction sa={.sa_sigaction=fault,.sa_flags=SA_SIGINFO};
    sigaction(SIGSEGV,&sa,NULL);sigaction(SIGILL,&sa,NULL);sigaction(SIGABRT,&sa,NULL);load(argv[1]);phase="Mixer installed live routing";
    assert(!mixer_live_native_install(base+4096));assert(mixer_live_native_install(base)&&mixer_live_native_install(base)&&!mixer_live_native_install(base+4096));
    ops=mixer_live_native_ops()->route;collection_checks();tests_live();release_collections();return 0;
}
