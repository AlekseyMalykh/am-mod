/* Real pinned Pad loop + installed production hooks, with fixture voices/DSP. */
#define MIXER_LIVE_PROOF_MAIN unused_policy_live_main
#include "check_mixer_live_native.c"
#include "src/music_settings.h"
static void policy_fixture(unsigned selection,int missing){
 live_fixture();
 F(program,0xc18,void*)=root_output;F(program,0xd50,unsigned)=missing?0:1;
 for(unsigned i=0;i<3;++i)F(instruments[i],0x560,unsigned)=selection==3?(i?2:1):selection;
}
int main(int argc,char**argv){
 assert(argc==3);setbuf(stdout,NULL);
 char dir[]="/tmp/am-fx-policy-XXXXXX",name[128];assert(mkdtemp(dir));
 snprintf(name,sizeof(name),"%s/.am-mod-fx-mode",dir);FILE*f=fopen(name,"w");assert(f);fprintf(f,"AM_FXMODE %s\n",argv[2]);fclose(f);
 assert(music_lighting_preferences_open(dir));unsigned mode=argv[2][0]-'0';assert(music_fx_mode_active()==mode);
 load(argv[1]);phase="FX mode isolation";assert(mixer_live_native_install(base));ops=mixer_live_native_ops()->route;
 collection_checks();service(0x220a4a0,reset_engine);service(0x2226d10,events);service(0x18c6990,render_list);service(0x221ee10,voice_chunk);service(0x2760ae0,native_source);assert(pad_eq_native_install(base));use_voice_pcm=1;
 float baseline[2][N];policy_fixture(2,0);run_live();memcpy(baseline,root_pcm,sizeof baseline);
 policy_fixture(1,0);run_live();
 if(mode){
  compare_pcm(root_pcm[0],baseline[0],1);compare_pcm(root_pcm[1],baseline[1],1);
  assert(energy(main_pcm[0])==0); /* no PCM reaches stock FX input, no session */
  policy_fixture(3,0);run_live();compare_pcm(root_pcm[0],baseline[0],1);assert(energy(main_pcm[0])==0);
  policy_fixture(1,1);run_live();assert(energy(main_pcm[0])==0);for(unsigned i=0;i<N;++i)assert(root_pcm[0][i]==.01f);
  struct mixer_project p;mixer_defaults(&p);p.output[0]=1;p.master.mix.level_db=-6.0206f;enum mixer_bus_result error;
  struct mixer_live*s=mixer_live_prepare(&p,0,48000,N,11,engine_owner,program,mixer_live_native_ops(),NULL,&error);assert(s&&error==MIXER_BUS_OK);assert(mixer_live_native_attach(s));
  policy_fixture(1,0);run_live();compare_pcm(root_pcm[0],baseline[0],.5f);assert(energy(main_pcm[0])==0);
  /* Duplicate active Instrument makes Mixer preflight reject the block,
   * while native rendering still produces Pad audio through its own loop. */
  policy_fixture(1,0);F(active[1],0,void*)=instruments[0];
  run_live();assert(mixer_route_last_reject()==6);assert(energy(main_pcm[0])==0);
  assert(energy(root_pcm[0])>.001);
  assert(mixer_live_native_attach(NULL));assert(mixer_live_native_take_retired()==s);mixer_live_destroy(s);
  policy_fixture(1,0);run_live();compare_pcm(root_pcm[0],baseline[0],1);assert(energy(main_pcm[0])==0);
  puts("PASS Am MOD FX isolation: all/some selected equal Direct, no session, attached session, native fallback, detach and missing direct target never feed FX Select");
 }else{assert(energy(main_pcm[0])>0);puts("PASS Stock preserves the native selected-Pad FX bus with no Mixer session");}
 /* Preference edits do not switch the running audio/UI mode. */
 assert(music_set_fx_mode(!mode));assert(music_fx_mode()==!mode&&music_fx_mode_active()==mode);
 release_collections();unlink(name);rmdir(dir);return 0;
}
