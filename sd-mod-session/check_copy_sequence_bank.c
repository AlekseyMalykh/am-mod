/* Copy navigation must not depend on the sound-pad colour overlay. */
#define main previous_workflow_main
#include "check_workflow.c"
#undef main
int main(int argc,char**argv){
 assert(argc==2);load_elf(argv[1],0x1800000000ULL);
 stub(0x112fd50,component_visible);stub(0x112f6b0,unblocked);stub(0x1149880,invalidate);
 __builtin___clear_cache((char*)base,(char*)(base+0x7500000));
 copy_root=copy_screen;is_visible=copy_root;
 for(unsigned owner=0;owner<=2;owner+=2){
  atomic_store(&native_colours,owner);atomic_store(&button_routes,1);atomic_store(&active,1);
  step_native_browser_bank(0);
  button_event(0x31,1);button_event(0x31,0);assert(visible(copy_root));
  button_event(0x31,1);assert(workflow_owns(2,0x23));
  unsigned char bank[16]={[8]=0x23,[9]=1};
  assert(workflow_button(bank,workflow_ticket())&&step_native_browser_bank(-1)==7);
  assert(workflow_button(bank,workflow_ticket())&&step_native_browser_bank(-1)==7);
  bank[9]=0;assert(workflow_button(bank,workflow_ticket()));
  bank[9]=1;assert(workflow_button(bank,workflow_ticket())&&step_native_browser_bank(-1)==6);
  bank[9]=0;assert(workflow_button(bank,workflow_ticket()));button_event(0x31,0);
  bank[9]=1;assert(!workflow_button(bank,workflow_ticket())&&step_native_browser_bank(-1)==6);
 }
 puts("PASS Copy Sequence/Pad: Shift release keeps Copy; Shift+Pad Bank A-H-G once per press; plain Pad Bank remains native");return 0;
}
