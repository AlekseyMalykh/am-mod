/* Integrate the scoped routing graph with actual native Instrument processing,
 * the installed AM Pad EQ wrapper, native Program stages/gains and native
 * allocated buffers. Voices and app services remain controlled fixtures. */
#define N 2048u
#define MIXER_ROUTE_PROOF_MAIN unused_mixer_route_main
#include "check_mixer_route_native.c"
#include "src/pad_eq.h"
#include "src/pad_eq_native.h"

static unsigned char rate_property[0xc0],program_internal[0x130],renderer[4][0x1b0];
static unsigned char native_collection[4][32];
static uintptr_t program_table[96];
static float internal_pcm[2][N],*internal_channels[2];
static unsigned source_blocks,collection_registrations,program_stage_mask;
static unsigned char *expected_collection;
static struct mixer_route_native *watch_adapter;
static struct mixer_meters *watch_meters;
static float source_gain=1;
static int source_silent;
static int use_voice_pcm;

static void register_collection(void*service,void*callback,void*collection)
{
    assert(service==rate_property+0x58&&collection==expected_collection);
    assert(F(callback,0,void*)==collection&&F(callback,8,uintptr_t)==base+0x2768070);
    ++collection_registrations;
}
static void release_collections(void)
{
    for(unsigned i=0;i<4;++i){
        FN(0x27737f0,void(*)(void*))(native_collection[i]+8);
        memset(native_collection[i],0,sizeof(native_collection[i]));
    }
}
static void collection_checks(void)
{
    /* The actual Instrument caller passes extra=1, Program passes extra=0;
     * the base ctor adds one before calling the tested native allocator. */
    assert(F((void*)base,0x281016c,uint32_t)==0xd2800020); /* mov x0,#1 */
    assert(F((void*)base,0x2810188,uint32_t)==0xf90003e0); /* str x0,[sp] */
    assert(F((void*)base,0x28ce204,uint32_t)==0xf90003ff); /* str xzr,[sp] */
    assert(F((void*)base,0x2810140,uint32_t)==0x52800001); /* default destination0 */
    assert(F((void*)base,0x2810100,uint32_t)==0x52800002); /* default subindex0 */
    jump(0x2768230,register_collection);
    F(rate_property,0xa8,unsigned)=N;F(rate_property,0x50,float)=48000;
    for(unsigned p=0;p<4;++p){
        expected_collection=native_collection[p];
        FN(0x276b9a0,void(*)(void*,unsigned long,void*))(native_collection[p],p<3?2:1,rate_property);
        unsigned char owner[0xb38]={0};F(owner,0xb30,void*)=native_collection[p];
        assert(ops.buffer_count(owner)==(p<3?2u:1u));
        void*first=ops.buffer(owner,0);float **pcm=F(first,16,float**);
        assert(F(first,0,unsigned)==2&&F(first,4,unsigned)==N&&F(first,0x128,unsigned char)==1);
        for(unsigned n=0;n<N;++n)assert(pcm[0][n]==0&&pcm[1][n]==0);
        for(unsigned n=0;n<N;++n){pcm[0][n]=.25f;pcm[1][n]=-.125f;}
        F(first,0x128,unsigned char)=0;
        /* Real reset physically clears both channels and sets clear. */
        FN(0x276a830,void(*)(void*))(native_collection[p]);
        assert(F(first,0x128,unsigned char)==1);
        for(unsigned n=0;n<N;++n)assert(pcm[0][n]==0&&pcm[1][n]==0);
        /* A flag already set skips the clearing loop: defensive preflight
         * must still reject externally corrupted stale-clear destinations. */
        pcm[1][0]=.75f;FN(0x276a830,void(*)(void*))(native_collection[p]);
        assert(pcm[1][0]==.75f);F(first,0x128,unsigned char)=0;
        FN(0x276a830,void(*)(void*))(native_collection[p]);assert(pcm[1][0]==0);
    }
    assert(collection_registrations==4);
    puts("PASS actual native buffer allocator: Instrument constructor input selects two buffers, Program selects one; reset clears both channels only when dirty");
}
static void native_source(void*state,void*buffer,unsigned frames)
{
    assert(frames==N);
    if(state==renderer[3])return; /* empty native Program insert engine */
    unsigned p=0;while(p<3&&state!=renderer[p])++p;assert(p<3);++source_blocks;
    if(use_voice_pcm)return;
    float **pcm=F(buffer,16,float**);
    const double frequency[]={1000,2000,500};
    for(unsigned n=0;n<N;++n){
        float v=source_silent?0:source_gain*.1f*(float)sin(2*3.141592653589793*frequency[p]*n/48000);
        pcm[0][n]=v;pcm[1][n]=v*.5f;
    }
    F(buffer,0x128,unsigned char)=source_silent;
}
static void native_stage(void*p,void*b,unsigned n,unsigned s)
{
    assert(p==program+0x78&&s>=1&&s<=3);program_stage_mask|=1u<<(s-1);
    if(watch_adapter){struct mixer_pcm_view view;
        assert(mixer_route_meters_push(watch_adapter,watch_meters,1)==-1);
        assert(!mixer_route_view(watch_adapter,0,&view));}
    FN(0x276a310,void(*)(void*,void*,unsigned,unsigned))(p,b,n,s);
}
static void native_fixture(void)
{
    fixture();source_blocks=program_stage_mask=0;
    memset(renderer,0,sizeof(renderer));memset(program_table,0,sizeof(program_table));
    /* This exact virtual implementation skips the optional Program callback
     * in real Instrument processing, matching the stock DrumProgram case. */
    program_table[66]=base+0x18de470;F(program,0,void*)=program_table;
    F(program,0xdc8,const char*)="mixer-native-dsp-proof";
    vtable[6]=(void*)(base+0x2803430);
    for(unsigned p=0;p<3;++p){
        F(instruments[p],0x638,void*)=rate_property;F(instruments[p],0x918,float)=1;
        F(instruments[p],0x910,void*)=renderer[p];F(instruments[p],0xb30,void*)=native_collection[p];
        F(instruments[p],0x800,float)=F(instruments[p],0x804,float)=1;
        F(instruments[p],0x81c,float)=F(instruments[p],0x820,float)=1;
    }
    /* Actual Program storage comes from the native allocator. Construct the
     * actual stock Instrument route (destination0/subindex0) using its native
     * helper. Resolver returns null for0; renderer uses the Program buffer. */
    F(program,0xba8,void*)=native_collection[3];
    unsigned route[6]={3,0,0,1,0x3f800000,0};
    for(unsigned p=0;p<3;++p)FN(0x2774a40,void(*)(void*,unsigned,unsigned,const void*))
        (instruments[p]+0x560,0,0,route);
    void*processor=program+0x78;F(processor,0x910,void*)=renderer[3];
    for(unsigned c=0;c<2;++c)internal_channels[c]=internal_pcm[c];
    buffer(program_internal,internal_channels);memcpy((char*)processor+0x998,program_internal,0x130);
    F(processor,0xac8,float)=F(processor,0xacc,float)=1;
    /* Execute real native stereo gain, rather than fixture Program stage3. */
    F(processor,0x800,float)=F(processor,0x804,float)=.5f;
    F(processor,0x81c,float)=F(processor,0x820,float)=.5f;
    for(unsigned p=0;p<4;++p)FN(0x276a830,void(*)(void*))(native_collection[p]);
    /* Reset existing Pad EQ histories using its public silent-buffer contract,
     * so reference and routed runs start with the identical filter state. */
    for(unsigned p=0;p<3;++p){unsigned id=p==2?127:p;
        assert(pad_eq_audio(id,(uintptr_t)instruments[p],48000,pad_channels[p][0],2,N,1));}
}
static double energy(const float*pcm)
{
    double sum=0;for(unsigned n=N/2;n<N;++n)sum+=(double)pcm[n]*pcm[n];return sum;
}
static void compare_pcm(const float*actual,const float*expected,float gain)
{
    for(unsigned n=0;n<N;++n)assert(isfinite(actual[n])&&fabsf(actual[n]-expected[n]*gain)<2e-7f);
}
static void dsp_checks(void)
{
    /* Only external voice/insert generation is a fixture; 2803430/2802f90,
     * AM Pad EQ hook, native Program stages and native gain all execute. */
    jump(0x2760ae0,native_source);
    assert(pad_eq_native_install(base));
    ops.stage=native_stage;
    uint64_t cut=pad_eq_edit(pad_eq_default(),PAD_EQ_GAIN,22,0);
    pad_eq_publish(0,cut);native_fixture();pad_eq_native_program(program);
    ops.render(program,NULL,N);assert(source_blocks==3);
    void*main=ops.buffer(program+0x78,0);float **native_pcm=F(main,16,float**);
    float baseline[2][N];memcpy(baseline[0],native_pcm[0],sizeof(baseline[0]));memcpy(baseline[1],native_pcm[1],sizeof(baseline[1]));
    struct mixer_route_native*a=prepare(0);native_fixture();
    enum mixer_route_result result=mixer_route_render(a,program,NULL,N);
    assert(result==MIXER_ROUTE_APPLIED&&source_blocks==3&&program_stage_mask==1);
    compare_pcm(native_pcm[0],baseline[0],1);compare_pcm(native_pcm[1],baseline[1],1);
    struct mixer_pcm_view t0,t1,direct,master;
    assert(mixer_route_view(a,0,&t0)&&mixer_route_view(a,1,&t1)&&mixer_route_view(a,8,&direct)&&mixer_route_view(a,9,&master));
    float **pad0=F(ops.buffer(instruments[0],0),16,float**);
    float **pad1=F(ops.buffer(instruments[1],0),16,float**);
    float **pad127=F(ops.buffer(instruments[2],0),16,float**);
    compare_pcm(t0.channel[0],pad0[0],1);compare_pcm(t1.channel[0],pad1[0],1);compare_pcm(direct.channel[0],pad127[0],1);
    compare_pcm(native_pcm[0],master.channel[0],.5f);compare_pcm(native_pcm[1],master.channel[1],.5f);
    double filtered=energy(t0.channel[0]);assert(filtered<energy(t1.channel[0])*.25);
    mixer_route_destroy(a);
    puts("PASS real native Instrument processing plus installed AM Pad EQ reaches only its assigned Track; native Program stages/stereo gain stay after graph output");

    a=prepare(-6.0206f);native_fixture();assert(mixer_route_render(a,program,NULL,N)==MIXER_ROUTE_APPLIED);
    assert(mixer_route_view(a,0,&t0));compare_pcm(t0.channel[0],pad0[0],.5f);
    for(unsigned n=0;n<N;++n){float expected=(t0.channel[0][n]+pad1[0][n]+pad127[0][n])*.5f;assert(fabsf(native_pcm[0][n]-expected)<2e-7f);}
    mixer_route_destroy(a);
    puts("PASS changed Track Level scales the post-Pad-EQ PCM once while preserving native Program gain and all other buses");

    pad_eq_publish(0,0);a=prepare(0);native_fixture();assert(mixer_route_render(a,program,NULL,N)==MIXER_ROUTE_APPLIED);
    assert(mixer_route_view(a,0,&t0)&&energy(t0.channel[0])>filtered*4);mixer_route_destroy(a);
    puts("PASS removing Pad EQ restores dry Track energy without changing Mixer routing");

    pad_eq_publish(0,cut);a=prepare(0);
    for(unsigned block=0;block<12;++block){
        native_fixture();source_gain=block%2?.75f:1;
        assert(mixer_route_render(a,program,NULL,N)==MIXER_ROUTE_APPLIED);
        for(unsigned p=0;p<4;++p)FN(0x276a830,void(*)(void*))(native_collection[p]);
        for(unsigned p=0;p<4;++p){void*b=F(native_collection[p],8,void*);float **pcm=F(b,16,float**);
            assert(F(b,0x128,unsigned char)==1);for(unsigned n=0;n<N;++n)assert(pcm[0][n]==0&&pcm[1][n]==0);}
    }
    source_gain=1;source_silent=1;native_fixture();assert(mixer_route_render(a,program,NULL,N)==MIXER_ROUTE_APPLIED);
    assert(energy(native_pcm[0])==0&&energy(native_pcm[1])==0);source_silent=0;mixer_route_destroy(a);
    struct pad_eq_native_stats stats;pad_eq_native_stats_read(&stats);assert(stats.processed>=15&&stats.matched==stats.processed);
    puts("PASS repeated native buffer reset/render cycles contain no stale-channel bleed; real Pad EQ/native Program path handles silence");
    pad_eq_native_clear();
}
#undef mixer_route_render
static unsigned char voices[3][16],engine[0x130];
static void *voice_list[3];
static unsigned voice_frames[3],voice_chunks[3];
static struct mixer_route_native *bound_adapter;
static enum mixer_route_result entry_result;
static void *entry_clock;
static int voice_chunk(void*voice,unsigned frames,unsigned offset)
{
    unsigned p=0;while(p<3&&voice!=voices[p])++p;assert(p<3);
    assert(frames>0&&frames<=64&&offset==voice_frames[p]&&offset+frames<=N);
    ++voice_chunks[p];voice_frames[p]+=frames;
    void*b=ops.buffer(instruments[p],0);float **pcm=F(b,16,float**);
    const double frequency[]={1000,2000,500};
    for(unsigned i=offset;i<offset+frames;++i){
        pcm[0][i]=.1f*(float)sin(2*3.141592653589793*frequency[p]*i/48000);pcm[1][i]=pcm[0][i]*.5f;
    }
    F(b,0x128,unsigned char)=0;return 1;
}
static void post_voice(void*p,void*clock,unsigned frames)
{
    assert(p==program&&clock==entry_clock&&frames==N);
    for(unsigned i=0;i<3;++i)assert(voice_frames[i]==N&&voice_chunks[i]==33);
    if(bound_adapter)entry_result=(mixer_route_render)(bound_adapter,p,clock,frames);
    else ops.render(p,clock,frames);
}
static void entry_fixture(void)
{
    native_fixture();memset(voice_frames,0,sizeof(voice_frames));memset(voice_chunks,0,sizeof(voice_chunks));
    for(unsigned p=0;p<3;++p)voice_list[p]=voices[p];
    F(program,0x1fb8,void*)=engine;F(program,0x2838,void*)=voice_list;
    F(program,0x2840,void*)=(char*)voice_list+sizeof(voice_list);
    F(program,0x28b8,unsigned)=13; /* intentionally not aligned to64 */
    program_table[0x2e0/8]=base+0x2868000;entry_clock=engine;
}
static void run_entry(struct mixer_route_native*a)
{
    bound_adapter=a;in_audio=1;
    FN(0x286c3b0,void(*)(void*,void*,unsigned))(program,entry_clock,N);
    in_audio=0;bound_adapter=NULL;
    assert(F(program,0x28b8,unsigned)==13+N&&source_blocks==3);
    assert(F(program,0x2840,void*)==(char*)voice_list+sizeof(voice_list));
}
static void entry_checks(void)
{
    /* Run the actual 64-frame chunking and active-voice walk. Only the voice
     * oscillator body is a fixture; it supplies per-Pad PCM before real DSP. */
    jump(0x221ee10,voice_chunk);
    uint32_t tail=0x14000000u|((uint32_t)((0x286be30-0x286c478)/4)&0x03ffffffu);
    assert(F((void*)base,0x286c478,uint32_t)==tail);
    jump(0x7400080,post_voice);
    F((void*)base,0x286c478,uint32_t)=0x14000000u|((0x7400080u-0x286c478u)/4);
    __builtin___clear_cache((char*)(base+0x286c478),(char*)(base+0x286c47c));
    use_voice_pcm=1;pad_eq_publish(0,pad_eq_edit(pad_eq_default(),PAD_EQ_GAIN,22,0));
    entry_fixture();pad_eq_native_program(program);run_entry(NULL);
    void*main=ops.buffer(program+0x78,0);float **pcm=F(main,16,float**);float baseline[2][N];
    memcpy(baseline[0],pcm[0],sizeof(baseline[0]));memcpy(baseline[1],pcm[1],sizeof(baseline[1]));
    struct mixer_route_native*a=prepare(0);entry_fixture();
    struct mixer_meters*meters=mixer_meters_create(48000);assert(meters);struct mixer_meter_snapshot snapshot;
    watch_adapter=a;watch_meters=meters;run_entry(a);watch_adapter=NULL;watch_meters=NULL;
    assert(entry_result==MIXER_ROUTE_APPLIED);
    compare_pcm(pcm[0],baseline[0],1);compare_pcm(pcm[1],baseline[1],1);
    in_audio=1;assert(mixer_route_meters_push(a,meters,1)==1);in_audio=0;
    assert(mixer_route_meters_push(a,meters,1)==-1&&mixer_meters_take(meters,1,&snapshot));
    assert(snapshot.valid_mask==0x1ff&&snapshot.frame_end==N&&!snapshot.fault_mask&&snapshot.peak[9][0]==0);
    for(unsigned bus=0;bus<9;++bus){struct mixer_pcm_view view;assert(mixer_route_view(a,bus,&view));
        for(unsigned c=0;c<2;++c){float peak=0;for(unsigned n=0;n<N;++n)peak=fmaxf(peak,fabsf(view.channel[c][n]));
            assert(snapshot.peak[bus][c]==peak);}}
    mixer_route_destroy(a);
    a=prepare(-6.0206f);entry_fixture();run_entry(a);assert(entry_result==MIXER_ROUTE_APPLIED);
    struct mixer_pcm_view track;assert(mixer_route_view(a,0,&track));
    float **pad0=F(ops.buffer(instruments[0],0),16,float**);compare_pcm(track.channel[0],pad0[0],.5f);
    float **pad1=F(ops.buffer(instruments[1],0),16,float**),**direct=F(ops.buffer(instruments[2],0),16,float**);
    for(unsigned n=0;n<N;++n)assert(fabsf(pcm[0][n]-(track.channel[0][n]+pad1[0][n]+direct[0][n])*.5f)<2e-7f);
    assert(mixer_route_meters_push(a,meters,2)==1&&mixer_meters_take(meters,2,&snapshot));
    assert(snapshot.valid_mask==0x1ff&&snapshot.frame_end==N);
    entry_fixture();F(instruments[0],0x560,unsigned)=2;run_entry(a);assert(entry_result==MIXER_ROUTE_PASSTHROUGH);
    assert(mixer_route_meters_push(a,meters,2)==1&&mixer_meters_take(meters,2,&snapshot));
    assert(snapshot.valid_mask==0&&snapshot.frame_end==N*2&&snapshot.peak[0][0]==0);
    assert(mixer_route_render(a,NULL,NULL,N)==MIXER_ROUTE_INVALID&&mixer_route_meters_push(a,meters,2)==-1);
    mixer_meters_destroy(meters);mixer_route_destroy(a);use_voice_pcm=0;pad_eq_native_clear();
    puts("PASS actual DrumProgram render entry chunks2048 frames at unaligned native clock into33 voice calls per Pad, then real Pad EQ/routing/Program gain; unity and selected Track Level preserved");
    puts("PASS native adapter publishes actual independent Track/Direct PCM peaks, leaves global Master unavailable, rejects duplicate publications and invalidates unsupported blocks");
}
#ifndef MIXER_DSP_PROOF_MAIN
#define MIXER_DSP_PROOF_MAIN main
#endif
int MIXER_DSP_PROOF_MAIN(int argc,char**argv)
{
    assert(argc==2);setbuf(stdout,NULL);struct sigaction sa={.sa_sigaction=fault,.sa_flags=SA_SIGINFO};
    sigaction(SIGSEGV,&sa,NULL);sigaction(SIGILL,&sa,NULL);sigaction(SIGABRT,&sa,NULL);
    load(argv[1]);phase="Mixer native buffer lifecycle and DSP";
    ops=(struct mixer_route_ops){FN(0x276abc0,void*(*)(void*,unsigned long)),
        FN(0x276abe0,unsigned long(*)(void*)),FN(0x18ccbc0,void*(*)(void*,const void*)),
        FN(0x2769bf0,int(*)(void*,void*,void*,const void*,unsigned)),
        native_stage,FN(0x286be30,void(*)(void*,void*,unsigned))};
    collection_checks();
    call(0x286bf44,0x97fbf72b,0x7400000,sum_dispatch);call(0x286bfa4,0x97fbf8db,0x7400040,stage_dispatch);
    dsp_checks();entry_checks();release_collections();
    puts("PASS Mixer native allocated-buffer and Pad EQ integration (fixture voices/services; no installed live Mixer)");return 0;
}
