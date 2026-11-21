/* Included by workflow_ui.c. All state belongs to the native UI owner. */
static unsigned program_page,program_scanning,program_count,program_selected,program_bank;
static char (*program_paths)[1024];
static char program_error[96];
static void program_paint(void*g);
static void program_close(void){
 if(program_page==3)return; /* Native reader pumps UI under its progress modal. */
 if(program_scanning)browser_tree_close();
 program_import_close();free(program_paths);program_paths=NULL;
 program_page=program_scanning=0;atomic_store(&custom,0);atomic_fetch_add(&epoch,1);
}
static void program_open(void){
 char root[1024];if(!am_midi_file_root(root,sizeof root)){browser_say("SD card unavailable");return;}
 close_search();if(seq_audition_off)seq_audition_off("program import");
 program_paths=calloc(256,sizeof(*program_paths));
 if(!program_paths||!browser_tree_start(root,3)){free(program_paths);program_paths=NULL;browser_say("Could not open programs");return;}
 program_count=program_selected=0;int bank=step_native_browser_bank(-1);program_bank=bank>=0&&bank<8?(unsigned)bank:0;
 program_error[0]=0;program_page=program_scanning=1;atomic_store(&custom,1);atomic_fetch_add(&epoch,1);search_preview_stop();repaint(files);
}
static void program_tick(void){
 if(!program_page||program_page==3)return;
 if(!visible(files)){program_close();return;}
 if(program_scanning){int r=browser_tree_next(program_paths[program_count],1024,128);if(r==1)++program_count;
  if(r<0||program_count==256){program_scanning=0;browser_tree_close();}}
 repaint(files);
}
static int program_button(unsigned code,unsigned down){
 if(!down||program_page==3)return 1;
 if(code==0x0c){if(program_page==2){program_import_close();program_page=1;program_error[0]=0;}else program_close();}
 else if(code==0x23){program_bank=(program_bank+(shift?7:1))%8;step_native_browser_bank((int)program_bank);program_error[0]=0;}
 else if(code==0x36||code==0x37){if(program_page==1)program_selected=(unsigned)clamp((int)program_selected+(code==0x37?-1:1),0,program_count?(int)program_count-1:0);}
 else if(code==0x0e||code==0x6f){
  if(step_native_transport_state&&(step_native_transport_state()&5u)){snprintf(program_error,sizeof program_error,"Stop playback before importing a program");return 1;}
  if(program_page==1&&!program_scanning&&program_count){
   program_page=3;repaint(files);
   int prepared=program_import_open(base,program_paths[program_selected],(void*)atomic_load(&copy_context),program_error);
   program_page=prepared?2u:1u;
  }else if(program_page==2){
   program_page=3;int applied=program_import_commit(program_bank,program_error);program_page=2;
   if(applied){program_close();browser_say("Program imported; Undo / Redo available");}
  }
 }
 repaint(files);return 1;
}
static int program_control(unsigned cc,unsigned v){
 if(cc==0x10){program_bank=v*8/128;step_native_browser_bank((int)program_bank);program_error[0]=0;}
 if(cc==0x64&&program_page==1)program_selected=(unsigned)clamp((int)program_selected+(v>64?(int)v-128:(int)v),0,program_count?(int)program_count-1:0);
 repaint(files);return 1;
}
