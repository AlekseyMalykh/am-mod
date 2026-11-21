/* UI-owned MIDI file selection, value-only review and one native Undo command. */
#define ROLL_MIDI_FILES 256u
static char (*roll_midi_paths)[1024];
static unsigned roll_midi_files,roll_midi_selected,roll_midi_scanning;
static struct am_smf roll_midi_document;
static struct am_smf_pad_note *roll_midi_notes;
static struct am_smf_import_options roll_midi_options;
static struct am_smf_import_info roll_midi_info;
static unsigned roll_midi_count;
static int roll_midi_valid;
static char roll_midi_error[96];
static int roll_midi_path_order(const void*a,const void*b){return strcmp(a,b);}
static void roll_midi_close(void){
 if(roll_midi_scanning)browser_tree_close();
 roll_midi_scanning=roll_midi_state=0;free(roll_midi_paths);roll_midi_paths=NULL;
 free(roll_midi_notes);roll_midi_notes=NULL;am_smf_free(&roll_midi_document);
}
static void roll_midi_open(void){
 roll_midi_close();char root[1024];
 if(!am_midi_file_root(root,sizeof(root))){say("SD card unavailable");return;}
 roll_midi_paths=calloc(ROLL_MIDI_FILES,sizeof(*roll_midi_paths));
 roll_midi_notes=calloc(PR_MAX_EVENTS,sizeof(*roll_midi_notes));
 if(!roll_midi_paths||!roll_midi_notes||!browser_tree_start(root,2)){roll_midi_close();say("Could not open MIDI files");return;}
 roll_midi_files=roll_midi_selected=0;roll_midi_scanning=roll_midi_state=1;roll_midi_error[0]=0;
}
static void roll_midi_tick(void){
 if(!roll_midi_scanning)return;
 int result=browser_tree_next(roll_midi_paths[roll_midi_files],sizeof(*roll_midi_paths),128);
 if(result==1)++roll_midi_files;
 if(result<0||roll_midi_files==ROLL_MIDI_FILES){roll_midi_scanning=0;browser_tree_close();qsort(roll_midi_paths,roll_midi_files,sizeof(*roll_midi_paths),roll_midi_path_order);}
}
static void roll_midi_preview(void){
 int native[128];for(int pad=0;pad<128;++pad)native[pad]=FN(0x157f800,int(*)(void*,int))(controller,pad);
 int chops=0;roll_midi_options.original_note=roll_sample_meta((int)roll_midi_options.sound,&chops);roll_midi_options.chops=(unsigned)chops;
 roll_midi_options.length=(uint32_t)roll.length;roll_midi_count=0;
 memset(&roll_midi_info,0,sizeof(roll_midi_info));
 roll_midi_valid=am_smf_import(&roll_midi_document,&roll_midi_options,native,roll_midi_notes,PR_MAX_EVENTS-roll.count,&roll_midi_count,&roll_midi_info,roll_midi_error);
}
static void roll_midi_load(void){
 if(roll_midi_scanning||!roll_midi_files)return;
 if(!am_midi_file_load(roll_midi_paths[roll_midi_selected],&roll_midi_document,roll_midi_error))return;
 roll_midi_options=(struct am_smf_import_options){.track=-1,.channel=-1,.mode=roll.layer==PR_TUNE?AM_SMF_TUNE:roll.layer==PR_CHOP?AM_SMF_CHOP:AM_SMF_PAD_MAP,
  .sound=(unsigned)(roll.layer==PR_PADS?roll.rows[roll.cursor_row]:roll.sound)};
 roll_midi_state=2;roll_midi_preview();
}
static void roll_midi_import(void){
 if(step_native_transport_state()&4){snprintf(roll_midi_error,sizeof(roll_midi_error),"Stop recording before MIDI import");return;}
 if(!roll_refresh(1))return;
 roll_midi_preview();if(!roll_midi_valid||roll.pending_commit)return;
 pr_cancel(&roll);roll.plan.title="Import MIDI";roll.plan.revision=roll.revision;
 for(unsigned i=0;i<roll_midi_count;++i){const struct am_smf_pad_note*n=roll_midi_notes+i;
  unsigned note=(unsigned)FN(0x157f800,int(*)(void*,int))(controller,n->sound);
  if(note>127){pr_cancel(&roll);snprintf(roll_midi_error,sizeof(roll_midi_error),"Destination pad has no native note");return;}
  struct step_event e=FN(0x27e54d0,struct step_event(*)(unsigned,int64_t,int64_t,unsigned,unsigned,float))(note,n->tick,n->duration,100,n->channel,n->velocity/127.f);
  if(roll_midi_options.mode==AM_SMF_TUNE)event_choice_set(&e,0,n->tune);
  if(roll_midi_options.mode==AM_SMF_CHOP)event_choice_set(&e,15,n->slice);
  roll.plan.added[i]=e;++roll.plan.added_count;
 }
 if(!pr_apply(&roll)){snprintf(roll_midi_error,sizeof(roll_midi_error),"%s",roll.error);return;}
 unsigned count=roll_midi_count;roll_midi_close();roll_refresh(1);char message[96];snprintf(message,sizeof(message),"Imported %u notes; Undo / Redo available",count);say(message);
}
static int roll_midi_button(unsigned code){
 if(code==0x31)return 2;
 if(code==0x51||code==0x52||code==0x50)return 2;
 if(code==0x0c){if(roll_midi_state==2){roll_midi_state=1;am_smf_free(&roll_midi_document);roll_midi_error[0]=0;}else roll_midi_close();}
 else if(code==0x0d&&roll_midi_state==2){roll_midi_options.skip_unmapped=!roll_midi_options.skip_unmapped;roll_midi_preview();}
 else if(code==0x23&&roll_midi_state==2)set_sound_bank((browse_bank+(shift?7:1))%8);
 else if(code==0x0e||code==0x6f){if(roll_midi_state==1)roll_midi_load();else roll_midi_import();}
 else if(code==6||code==7){roll_close();return 2;}
 repaint();return 1;
}
static int roll_midi_control(unsigned cc,unsigned v){
 int delta=v>64?(int)v-128:(int)v;
 if(roll_midi_state==1){if(cc==0x64&&!roll_midi_scanning&&roll_midi_files)roll_midi_selected=(unsigned)clamp((int)roll_midi_selected+delta,0,(int)roll_midi_files-1);}
 else {
  if(cc==0x64)roll_midi_options.track=clamp(roll_midi_options.track+delta,-1,(int)roll_midi_document.tracks-1);
  if(cc==0x10)roll_midi_options.mode=v*3/128;
  if(cc==0x11)roll_midi_options.channel=(int)(v*17/128)-1;
  if(cc==0x12)roll_midi_options.fit=v>=64;
  if(cc==0x14)roll_midi_options.port=v*3/128;
  roll_midi_preview();
 }repaint();return 1;
}
static void roll_midi_pad(unsigned pad){
 if(roll_midi_state!=2||pad>=16||roll_midi_options.mode==AM_SMF_PAD_MAP)return;
 roll_midi_options.sound=(unsigned)browse_bank*16+pad;roll_midi_preview();repaint();
}
static void roll_midi_paint(void*g){
 colour(g,0xff000000u);rect(g,0,0,320,240);colour(g,WHITE);text(g,"Back",0,6,105,23,14);
 text(g,roll_midi_state==2?(roll_midi_options.skip_unmapped?"Skip On":"Skip Off"):"",107,6,105,23,14);
 colour(g,roll_midi_state==2&&!roll_midi_valid?GREY:AMBER);text(g,roll_midi_state==2?"Import":"Open",214,6,105,23,14);
 colour(g,WHITE);left_text(g,"Import MIDI",8,35,304,21,17);char line[160];
 if(roll_midi_state==1){
  unsigned first=roll_midi_selected<5?0:roll_midi_selected-4;
  for(unsigned i=first;i<roll_midi_files&&i<first+6;++i){const char *name=strrchr(roll_midi_paths[i],'/');int y=64+(int)(i-first)*21;
   if(i==roll_midi_selected){colour(g,0xff302412u);rect(g,6,y,308,21);}colour(g,i==roll_midi_selected?AMBER:WHITE);left_text(g,name?name+1:roll_midi_paths[i],10,y,300,21,12);}
  colour(g,AMBER);left_text(g,roll_midi_error[0]?roll_midi_error:roll_midi_scanning?"Finding .mid / .midi files on this card":roll_midi_files?"DATA: select file   B3: review":"No MIDI files in MPC-Sample on this card",8,204,304,24,11);return;
 }
 const char *name=strrchr(roll_midi_paths[roll_midi_selected],'/');colour(g,GREY);left_text(g,name?name+1:roll_midi_paths[roll_midi_selected],8,58,304,17,11);
 static const char*const modes[]={"Pad map","Tune","Chop"};char pad[8];pad_name((int)roll_midi_options.sound,pad);
 snprintf(line,sizeof(line),"%s%s%s | %u / %u notes",modes[roll_midi_options.mode],roll_midi_options.mode?" ":"",roll_midi_options.mode?pad:"",roll_midi_info.accepted,roll_midi_document.count);colour(g,WHITE);left_text(g,line,8,83,304,20,14);
 if(roll_midi_options.track<0)snprintf(line,sizeof(line),"All %u MIDI tracks",roll_midi_document.tracks);else snprintf(line,sizeof(line),"MIDI track %d",roll_midi_options.track+1);left_text(g,line,8,108,304,18,12);
 snprintf(line,sizeof(line),"Unmapped %u | Ambiguous %u | Outside %u",roll_midi_info.unmapped,roll_midi_info.ambiguous,roll_midi_info.out_of_range);left_text(g,line,8,131,304,18,10);
 snprintf(line,sizeof(line),"%s | Port: %s",roll_midi_options.fit?"Fit to sequence":"Original timing",roll_midi_options.port==1?"USB":roll_midi_options.port==2?"External":"Any");left_text(g,line,8,153,304,18,11);
 colour(g,roll_midi_valid?AMBER:RED);left_text(g,roll_midi_error[0]?roll_midi_error:"B3: append notes as one Undo operation",8,175,304,18,11);
 colour(g,GREY);left_text(g,"Notes only | DATA: track | Fader: port",8,191,304,7,8);
 cell(g,0,"Mode",modes[roll_midi_options.mode],0,(int)roll_midi_options.mode,3,0);
 char channel[24];if(roll_midi_options.channel<0)snprintf(channel,sizeof(channel),"All");else snprintf(channel,sizeof(channel),"%d",roll_midi_options.channel+1);
 cell(g,106,"Channel",channel,0,roll_midi_options.channel+1,17,0);cell(g,213,"Timing",roll_midi_options.fit?"Fit":"Original",0,(int)roll_midi_options.fit,2,0);
}
