/* Reuse the pinned engine/router fixtures, without running the historical
 * session UI tests (their B2 expectation predates the current Mixer). */
#define MIXER_SESSION_PROOF_MAIN unused_usb_session_main
#include "check_mixer_session_native.c"
#include "src/usb_audio.h"
int main(int argc,char**argv){
 assert(argc==2);setbuf(stdout,NULL);struct sigaction sa={.sa_sigaction=fault,.sa_flags=SA_SIGINFO};
 sigaction(SIGSEGV,&sa,NULL);sigaction(SIGILL,&sa,NULL);sigaction(SIGABRT,&sa,NULL);
 load(argv[1]);phase="USB production live tap";
 assert(mixer_live_native_install(base));ops=mixer_live_native_ops()->route;collection_checks();
 service(0x220a4a0,reset_engine);service(0x2226d10,events);service(0x18c6990,render_list);service(0x221ee10,voice_chunk);
 service(0x2760ae0,native_source);assert(pad_eq_native_install(base));use_voice_pcm=1;
 live_caller_fixture();live_fixture();pad_eq_native_program(program);
 struct mixer_project model;mixer_defaults(&model);model.output[0]=1;model.output[1]=2;model.master.mix.level_db=-6.0206f;
 enum mixer_bus_result error;
 struct mixer_live*s=mixer_live_prepare(&model,0,48000,N,1,engine_owner,program,mixer_live_native_ops(),NULL,&error);
 assert(s&&error==MIXER_BUS_OK&&mixer_live_native_attach(s));
 float usb[N*AM_USB_CHANNELS];usb_audio_enable(1);
 live_fixture();run_discovery();assert(!usb_audio_peek(usb,N));
 live_fixture();run_discovery();assert(usb_audio_peek(usb,N)==N);
 for(unsigned frame=0;frame<N;++frame)for(unsigned c=0;c<2;++c)assert(usb[frame*18+c]==root_pcm[c][frame]);
 for(unsigned bus=0;bus<8;++bus){struct mixer_pcm_view v;assert(mixer_live_bus_view(s,bus,&v)&&v.frames==N);
  for(unsigned frame=0;frame<N;++frame)for(unsigned c=0;c<2;++c)assert(usb[frame*18+2+2*bus+c]==v.channel[c][frame]);}
 usb_audio_consume(N);live_fixture();run_live();assert(!usb_audio_peek(usb,N));
 live_fixture();run_discovery();mixer_live_native_invalidate();assert(!usb_audio_peek(usb,N));
 usb_audio_enable(0);assert(mixer_live_native_attach(NULL));assert(mixer_live_native_take_retired()==s);mixer_live_destroy(s);pad_eq_native_clear();release_collections();
 puts("PASS production USB tap: pinned live engine output and8 stereo buses match all18 queued channels; offline excluded and project reset discards old PCM");
 return 0;
}
