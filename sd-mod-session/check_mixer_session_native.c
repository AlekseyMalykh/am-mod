#define MIXER_LIVE_PROOF_MAIN unused_session_live_main
#include "check_mixer_live_native.c"
#include "src/mixer_session.h"
#include "src/mixer_archive.h"
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
    char *payload=malloc(MIXER_PAYLOAD_BYTES);assert(payload);size_t length;
    assert(mixer_encode(&observed,payload,MIXER_PAYLOAD_BYTES,&length)==MIXER_OK);
    struct mixer_project decoded;assert(mixer_decode(payload,length,&decoded)==MIXER_OK&&decoded.track[0].mix.pan==-1&&decoded.output[0]==1);free(payload);
    assert(!mixer_control_button(c,MIXER_NATIVE_MODE,1,ticket)&&!mixer_control_active(c));
    live_fixture();run_live();struct mixer_meter_snapshot meter;assert(mixer_live_take_meter(mixer_session_live(s),&meter)&&meter.valid_mask==0x3ff);
    mixer_archive_reset();mixer_session_owner_tick(s);mixer_session_ui_tick(s);assert(mixer_session_status(s)==-1&&!mixer_control_active(c));
    assert(mixer_live_native_attach(NULL));assert(mixer_live_native_take_retired()==mixer_session_live(s));mixer_session_destroy(s);pad_eq_native_clear();
    puts("PASS native control-session bridge: confirmed edits persist only after actual audio revision, UI close leaves audio running, codec restores saved mix, external New invalidates session");
}
int main(int argc,char**argv)
{
    assert(argc==2);setbuf(stdout,NULL);struct sigaction sa={.sa_sigaction=fault,.sa_flags=SA_SIGINFO};
    sigaction(SIGSEGV,&sa,NULL);sigaction(SIGILL,&sa,NULL);sigaction(SIGABRT,&sa,NULL);load(argv[1]);phase="Mixer session persistence";
    assert(mixer_live_native_install(base));ops=mixer_live_native_ops()->route;collection_checks();session_test();release_collections();return 0;
}
