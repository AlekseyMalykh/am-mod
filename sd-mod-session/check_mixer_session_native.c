#define MIXER_LIVE_PROOF_MAIN unused_session_live_main
#include "check_mixer_live_native.c"
#include "src/mixer_session.h"
#include "src/mixer_archive.h"
#include "src/mixer_runtime.h"
#include "src/mixer_sources.h"
#include <pthread.h>
static void session_test(void)
{
    service(0x220a4a0,reset_engine);service(0x2226d10,events);service(0x18c6990,render_list);service(0x221ee10,voice_chunk);
    service(0x2760ae0,native_source);assert(pad_eq_native_install(base));use_voice_pcm=1;
    mixer_archive_init(base);struct mixer_project initial,observed;uint64_t rev;unsigned mode;
    assert(mixer_archive_snapshot(&initial,&rev,&mode));initial.output[0]=1;
    assert(mixer_archive_replace(&initial,rev)==MIXER_EDIT_OK);assert(mixer_archive_snapshot(&observed,&rev,&mode));
    live_fixture();pad_eq_native_program(program);
    struct mixer_session*s=mixer_session_create(30,0,48000,N,engine_owner,program,mixer_live_native_ops(),NULL);assert(s&&mixer_session_status(s)==0);
    struct mixer_control*c=mixer_session_control(s);assert(!mixer_control_open(c,mixer_session_status(s)==1,0));assert(mixer_live_native_attach(mixer_session_live(s)));
    live_fixture();run_live();mixer_session_ui_tick(s);assert(mixer_session_status(s)==1&&mixer_control_open(c,1,0));
    uint64_t ticket=mixer_control_ticket(c);assert(mixer_control_cc(c,0x10,0,ticket));mixer_session_ui_tick(s);
    uint64_t after;assert(mixer_archive_snapshot(&observed,&after,&mode)&&after==rev&&observed.track[0].mix.pan==0);
    mixer_session_owner_tick(s);assert(mixer_archive_snapshot(&observed,&after,&mode)&&after==rev);
    live_fixture();run_live();mixer_session_owner_tick(s);mixer_session_ui_tick(s);
    assert(mixer_archive_snapshot(&observed,&after,&mode)&&after==rev+1&&observed.track[0].mix.pan==-1);
    struct mixer_view_state v;mixer_control_view(c,1000,&v);assert(v.project->track[0].mix.pan==-1&&!v.pending);
    /* Metadata edits use the same acknowledgement/archive path and do not
     * change the PCM, gains, routes or the other channel colours. */
    live_fixture();run_live(); /* Compare with steady Pan, after its ramp block. */
    float before_colour[2][N];memcpy(before_colour,root_pcm,sizeof(before_colour));
    uint32_t old_colour=observed.track[0].colour;
    assert(mixer_control_button(c,MIXER_B1,1,ticket)&&mixer_control_button(c,MIXER_B1,0,ticket));
    assert(mixer_control_cc(c,0x12,127,ticket));mixer_session_ui_tick(s);mixer_session_owner_tick(s);
    assert(mixer_archive_snapshot(&observed,&after,&mode)&&after==rev+1&&observed.track[0].colour==old_colour);
    mixer_control_view(c,1000,&v);assert(v.pending&&v.project->track[0].colour==old_colour);
    live_fixture();run_live();compare_pcm(root_pcm[0],before_colour[0],1);compare_pcm(root_pcm[1],before_colour[1],1);
    mixer_session_owner_tick(s);mixer_session_ui_tick(s);
    assert(mixer_archive_snapshot(&observed,&after,&mode)&&after==rev+2&&observed.track[0].colour==mixer_colour_at(11));
    assert(observed.track[0].mix.pan==-1&&observed.output[0]==1&&observed.master.colour==initial.master.colour);
    char *payload=malloc(MIXER_PAYLOAD_BYTES);assert(payload);size_t length;
    assert(mixer_encode(&observed,payload,MIXER_PAYLOAD_BYTES,&length)==MIXER_OK);
    struct mixer_project decoded;assert(mixer_decode(payload,length,&decoded)==MIXER_OK&&decoded.track[0].mix.pan==-1&&decoded.output[0]==1&&decoded.track[0].colour==mixer_colour_at(11));free(payload);
    assert(!mixer_control_button(c,MIXER_NATIVE_MODE,1,ticket)&&!mixer_control_active(c));
    live_fixture();run_live();struct mixer_meter_snapshot meter;assert(mixer_live_take_meter(mixer_session_live(s),&meter)&&meter.valid_mask==0x3ff);
    mixer_archive_reset();mixer_session_owner_tick(s);mixer_session_ui_tick(s);assert(mixer_session_status(s)==-1&&!mixer_control_active(c));
    assert(mixer_live_native_attach(NULL));assert(mixer_live_native_take_retired()==mixer_session_live(s));mixer_session_destroy(s);pad_eq_native_clear();
    puts("PASS native control-session bridge: confirmed edits persist only after actual audio revision, UI close leaves audio running, codec restores saved mix, external New invalidates session");
}
static struct mixer_session *automatic;
static unsigned automatic_binds,automatic_unbinds;
int mixer_phone_bind_ui(struct mixer_session *session)
{
    if (session) ++automatic_binds;
    else ++automatic_unbinds;
    automatic = session;
    return 1;
}
static void live_caller_fixture(void)
{
    /* Recreate only the native caller's BL/return envelope at the exact
     * live callsite. The target2226fb0 and downstream DSP remain real. */
    uint32_t code[] = {0xa9bf7bfd,0x910003fd,0xd503201f,0xd503201f,
        0x94000000u | ((0x2226fb0u-0x18dc00cu)/4u),0xa8c17bfd,0xd65f03c0};
    memcpy((void*)(base+0x18dbffc),code,sizeof(code));
    __builtin___clear_cache((char*)(base+0x18dbffc),(char*)(base+0x18dc018));
}
static void run_discovery(void)
{
    in_audio=1;
    FN(0x18dbffc,void(*)(void*,void*,unsigned))(engine_owner,entry_clock,N);
    in_audio=0;
}
static void source_runtime_test(void)
{
    static _Alignas(16) unsigned char context[0x100],owner[0x260],slots[4][0xc0],things[0x100];
    static _Alignas(16) unsigned char layers[3][8][0xb30],reference[0x20];
    static void *pads[128];
    F(context,0,uintptr_t)=base+0x6e0dfd8;F(context,0x48,void*)=owner;F(context,0x78,void*)=things;
    F(things,0xb8,void*)=program;F(owner+0x1d8,0,uint64_t)=4;F(owner+0x1d8,0x30,void*)=slots;
    assert(!pthread_mutex_init((void*)(owner+0x220),NULL));
    for(unsigned i=0;i<4;++i){F(slots[i],0,uintptr_t)=base+0xb57a7c;F(slots[i],8,uintptr_t)=base+0x15265b0;}
    service(0xf40e50,pthread_mutex_lock);service(0xf40e90,pthread_mutex_unlock);
    pads[0]=instruments[0];pads[127]=instruments[2];
    F(program,0x2808,void*)=pads;F(program,0x2810,void*)=pads+128;
    for(unsigned i=0;i<3;++i){F(instruments[i],0x22e8,void*)=layers[i];F(instruments[i],0x22f0,void*)=layers[i]+8;}
    F(reference,8,unsigned)=2;F(reference,12,unsigned)=2;
    F(layers[2][7],0x4a8,void*)=(void*)1;F(layers[2][7],0x4b0,void*)=reference;
    mixer_sources_bind(base,context);
    mixer_runtime_ui_tick();assert(F(context,0x50,unsigned)==1);
    uint64_t cursor=0;FN(0x2b59200,void(*)(void*,uint64_t*))(owner+0x1d8,&cursor);
    usleep(260000);mixer_runtime_ui_tick();
    struct mixer_control *control=mixer_session_control(automatic);struct mixer_view_state view;
    mixer_control_view(control,0,&view);assert(!view.present[0]&&view.present[1]==(UINT64_C(1)<<63));
    FN(0x2b59200,void(*)(void*,uint64_t*))(owner+0x1d8,&cursor);
    F(owner+0x1d8,0x2d,unsigned char)=1;usleep(800000);mixer_runtime_ui_tick();
    mixer_control_view(control,0,&view);assert(!view.present[0]&&!view.present[1]); /* Capture age, not consumption time. */
    F(owner+0x1d8,0x2d,unsigned char)=0;mixer_control_close(control);
    mixer_runtime_ui_tick();mixer_runtime_ui_tick();assert(!F(context,0x50,unsigned));
    mixer_sources_stop(context);struct mixer_sources_snapshot out;
    assert(!mixer_sources_poll(program,1,&out));
    assert(F(reference,8,unsigned)==2&&!pthread_mutex_destroy((void*)(owner+0x220)));
    puts("PASS native source queue -> runtime -> controller mask, stale capture expires and closed Mixer stops polling");
}
static void automatic_test(void)
{
    live_caller_fixture();
    mixer_archive_reset();
    struct mixer_project model;
    uint64_t revision;
    unsigned mode;
    assert(mixer_archive_snapshot(&model,&revision,&mode));
    model.output[0]=1;
    model.master.mix.level_db=-6.0206f;
    assert(mixer_archive_replace(&model,revision)==MIXER_EDIT_OK);

    mixer_runtime_ui_tick();
    assert(!automatic);
    /* A direct/offline call cannot discover the device session. */
    live_fixture();run_live();
    mixer_runtime_ui_tick();assert(!automatic);
    live_fixture();run_discovery();
    struct mixer_native_binding stale;
    assert(mixer_live_native_binding(&stale)&&stale.program==program&&stale.rate==48000);
    mixer_runtime_ui_tick();
    assert(automatic&&automatic_binds==1&&mixer_session_status(automatic)==0);
    live_fixture();run_discovery();
    float applied[N];memcpy(applied,root_pcm[0],sizeof(applied));
    mixer_session_ui_tick(automatic);
    assert(mixer_session_status(automatic)==1);
    assert(mixer_control_open(mixer_session_control(automatic),1,0));
    live_fixture();run_live();compare_pcm(root_pcm[0],applied,2); /* offline bypass */
    source_runtime_test();

    /* A native project operation invalidates the audio attachment immediately.
     * Retirement and preparation happen on the UI/control owner afterwards. */
    mixer_live_native_invalidate();
    live_fixture();run_discovery();compare_pcm(root_pcm[0],applied,2);
    mixer_archive_reset();
    mixer_runtime_ui_tick();assert(!automatic&&automatic_unbinds==1);
    assert(!mixer_live_native_binding(&stale));
    live_fixture();run_discovery();
    mixer_runtime_ui_tick();assert(automatic&&automatic_binds==2);
    live_fixture();run_discovery();mixer_session_ui_tick(automatic);
    assert(mixer_session_status(automatic)==1);
    /* Same Program address belongs to a fresh source epoch after New. */
    struct mixer_native_binding replacement;
    assert(mixer_live_native_binding(&replacement)&&replacement.epoch>stale.epoch);

    F(rate_property,0x50,float)=44100;
    live_fixture();run_discovery();
    mixer_runtime_ui_tick();assert(automatic_binds==3&&automatic_unbinds==2);
    live_fixture();run_discovery();mixer_session_ui_tick(automatic);
    assert(mixer_session_status(automatic)==1);
    F(rate_property,0x50,float)=48000;

    /* Unsupported multi-Program graph revokes the automatic attachment. */
    live_fixture();
    F(root_mixer,0x108,void*)=program_list+2;
    run_discovery();
    mixer_runtime_ui_tick();assert(!automatic&&automatic_unbinds==3);
    live_fixture();run_discovery();
    mixer_runtime_ui_tick();assert(automatic&&automatic_binds==4);
    mixer_runtime_ui_stop();
    assert(!automatic&&automatic_unbinds==4);
    puts("PASS automatic audio-observed session creation, real PCM readiness, offline isolation, Load/New revocation, same-address epoch reuse, sample-rate replacement and retirement");
}
int main(int argc,char**argv)
{
    assert(argc==2);setbuf(stdout,NULL);struct sigaction sa={.sa_sigaction=fault,.sa_flags=SA_SIGINFO};
    sigaction(SIGSEGV,&sa,NULL);sigaction(SIGILL,&sa,NULL);sigaction(SIGABRT,&sa,NULL);load(argv[1]);phase="Mixer session persistence";
    assert(mixer_live_native_install(base));ops=mixer_live_native_ops()->route;collection_checks();session_test();automatic_test();release_collections();return 0;
}
