/* Exercise the production UI completion gate without pad input. */
#define MPC_EARLY_STARTUP 1
#define main previous_workflow_main
#include "check_workflow.c"
#undef main
static int loading;
int startup_project_loading(void){return loading;}
void startup_project_ready(void){}
int main(int argc,char**argv){
 assert(argc==2);load_elf(argv[1],0x1800000000ULL);
 stub(0x10f87a0,top_modal);__builtin___clear_cache((char*)base,(char*)(base+0x7500000));
 for(unsigned scenario=0;scenario<3;++scenario){
  startup_repaint_done=0;unsigned before=startup_repaints;
  loading=1;autoload_sample=1;autoload_modal=NULL;
  startup_repaint_tick();assert(startup_repaints==before&&!startup_repaint_done);
  /* A load/error dialog hides the splash without completing the load. */
  autoload_modal=(void*)1;startup_repaint_tick();assert(startup_repaints==before);
  loading=0;
  if(scenario==1){startup_repaint_tick();assert(startup_repaints==before&&!startup_repaint_done);}
  autoload_modal=NULL;
  if(scenario==2){autoload_sample=0;startup_repaint_tick();assert(startup_repaints==before&&!startup_repaint_done);autoload_sample=1;}
  startup_repaint_tick();assert(startup_repaints==before+1&&startup_repaint_done);
  for(unsigned tick=0;tick<20;++tick){startup_repaint_tick();}
  assert(startup_repaints==before+1);
 }
 puts("PASS Startup final Sample repaint: waits for load completion, modal close and visible Sample; exactly once without pad input");return 0;
}
