/* Frontend contract: legacy EventAudition entries poisoned; ordinary Sample
 * destination is an adapter. Its real native input has a separate test. */
static char test_probe_root[]="/tmp/mpc-picker-probe-XXXXXX";
#define PICKER_PROBE_ROOT test_probe_root
#define main original_steps_main
#include "check_steps.c"
#undef main
static void forbidden(void){fprintf(stderr,"Unexpected audition/queue call\n");abort();}
int main(int argc,char**argv){
 assert(argc==2);assert(mkdtemp(test_probe_root));load_elf(argv[1],0x1800000000ULL);setup();assert(step_native_event_preview_enabled());
 unsigned char selected_track[8]={0};FIELD(things,0x140,void*)=selected_track;FIELD(things,0x368,void*)=(void*)1;
 struct sigaction a={0};a.sa_sigaction=fault;a.sa_flags=SA_SIGINFO;sigaction(SIGSEGV,&a,NULL);sigaction(SIGBUS,&a,NULL);
 stub(0x18b0510,forbidden);stub(0x18b0c00,forbidden);stub(0x2119d00,forbidden);stub(0xf40e50,forbidden);stub(0x2b59300,forbidden);
 __builtin___clear_cache((char*)base,(char*)(base+0x7500000));
 for(int context=0;context<2;++context){
  if(context){button(0x0d,1);button(0x0d,0);sound=7;grid=2;hold(1);step_native_tick();assert(steps);}
  for(int mode=0;mode<10;++mode){
   struct step_event source=make_note(7,240,.8f);event_choice_set(&source,0,5);insert_note(&source);assert(choose_exact(&source));step_native_tick();
   unsigned ink;assert(step_native_button_light(BUTTON_LEVELS,&ink)&&ink==2);
   button(BUTTON_LEVELS,1);button(BUTTON_LEVELS,0);assert(menu==3);
   assert(step_native_preview_suppressed(selected_track,&source,0x7ffffffe));
   picker_mode=mode;picker_base=mode==1?-3:mode==8?1:0;picker_slice_count=128;
   step_native_tick();assert(!picker_has_choice);
   if(mode==1){uint64_t light;assert(step_native_light(8,&light)&&(uint32_t)light==0xffc247);}
   for(unsigned pad=0;pad<16;++pad){
    unsigned starts=audition_starts,stops=audition_stops;
    step_native_pad(pad,step_native_ticket());assert(picker_audition_active&&audition_starts==starts+1);
    assert(!(audition_event.type&0x300)&&audition_event.time==0);
    step_native_pad_release(pad,step_native_ticket());assert(menu==3&&!picker_audition_active&&audition_stops==stops+1);
    assert(picker_has_choice);
    if(mode==1&&pad==0){uint64_t light;assert(step_native_light(0,&light)&&(uint32_t)light==0xffffff);assert(step_native_light(8,&light)&&(uint32_t)light==0xffc247);}
    struct step_event check;assert(selected_event(&check)&&(FN(0x19722e0,int(*)(const void*,const void*))(&check,&source)&255));
   }
   struct step_event applied=detail_edit;button(0x0e,1);button(0x0e,0);
   struct step_event check;assert(!menu&&steps==context&&selected_event(&check)&&(FN(0x19722e0,int(*)(const void*,const void*))(&check,&applied)&255));assert(erase_note(&check));step_native_tick();
   assert(!step_native_preview_suppressed(selected_track,&source,0x7ffffffe));
  }
 }
 char path[512],line[160];snprintf(path,sizeof(path),"%s/picker-phase.txt",test_probe_root);
 step_native_tick();FILE*f=fopen(path,"r");assert(f&&fgets(line,sizeof(line),f)&&!strncmp(line,"PAD_DONE ",9));fclose(f);assert(!unlink(path));
 snprintf(path,sizeof(path),"%s/picker-trace.txt",test_probe_root);f=fopen(path,"r");assert(f);unsigned lines=0;while(fgets(line,sizeof(line),f))++lines;fclose(f);assert(lines>=64*3&&lines<=64*8);assert(!unlink(path)&&!rmdir(test_probe_root));
 puts("PASS r16d frontend all16 pads/all10 modes Events/Steps -> ordinary Sample input adapter; original amber/choice cyan, staged event unchanged before Apply, paired release and bounded RAM trace; audio worker tested separately");
}
