/* UI owner only. Uses the existing Piano Roll's copied notes and editor. */
extern int mixer_phone_pad_output(unsigned) __attribute__((weak));
static const char*const roll_pattern_actions[]={"Split by Pads","Split by Mixer","Manual: selected + rest","Empty pattern","Extract range","Create variation","Delete pattern","Copy to sequence"};
static void roll_patterns_open(void){
 if(step_native_transport_state()&4u){say("Stop recording before Patterns");return;}
 workflow_piano_input_stop();roll_input_held=0;if(roll_audition>=0){workflow_event_preview_stop(0);roll_audition=-1;}
 roll_erase_held=roll_erase_used=0;roll_row_confirm=-1;
 roll_patterns_epoch=pt_epoch();pt_expect_epoch(roll_patterns_epoch);roll_patterns_state=1;roll_actions=roll_editing=0;pr_cancel(&roll);
 roll_patterns_pick=clamp(roll_patterns_pick,0,pt_count()?(int)pt_count()-1:0);repaint();
}
static int roll_pattern_refresh(void){
 struct pt_entry entry;unsigned char loaded[128];
 if(!pt_info((unsigned)roll_pattern_index,&entry)){roll_pattern_index=-1;pr_reset_view(&roll);roll_patterns_open();return 1;}
 if(roll.revision==entry.revision&&roll.length==entry.length&&!roll.pending_commit)return 1;
 int count=pt_notes((unsigned)roll_pattern_index,roll_snapshot,PR_MAX_EVENTS,loaded);
 if(count<0||!pr_load(&roll,roll_snapshot,(unsigned)count,entry.length,entry.revision,loaded)){say("Pattern exceeds editor capacity");return 0;}
 return 1;
}
static int roll_patterns_capture(unsigned mode){
 if(roll_pattern_index>=0){say("Return to sequence before capture");return 0;}
 if(step_native_transport_state()&4u||roll.pending_commit){say("Finish recording/edit before capture");return 0;}
 int mapping[128],outputs[128];
 void*things=FIELD(controller,0x248,void*),*program=things?FIELD(things,0xb8,void*):NULL;
 const char*key=program?FIELD(program,0xdc8,const char*):NULL;
 if(!key||!key[0]||strnlen(key,128)>=128){say("Program identity unavailable");return 0;}
 char identity[128];snprintf(identity,sizeof identity,"%s",key);
 for(unsigned pad=0;pad<128;++pad){mapping[pad]=FN(0x157f800,int(*)(void*,int))(controller,(int)pad);outputs[pad]=mixer_phone_pad_output?mixer_phone_pad_output(pad):-1;}
 return pt_capture(roll.notes,roll.count,roll.length,roll.revision,identity,mapping,outputs,mode);
}
static void roll_patterns_execute(void){
 int ok=0;unsigned old=pt_count();
 if(roll_patterns_action<4)ok=roll_patterns_capture((unsigned)roll_patterns_action);
 else if(roll_patterns_action==4){struct pt_entry entry;if(!pt_info((unsigned)roll_patterns_pick,&entry)){say("Choose a part or pattern");return;}
  roll_patterns_begin=0;roll_patterns_end=entry.length;roll_patterns_state=3;repaint();return;
 }else if(roll_patterns_action==5)ok=pt_variation((unsigned)roll_patterns_pick);
 else if(roll_patterns_action==6){roll_patterns_state=4;repaint();return;}
 else {
  int count=pt_notes((unsigned)roll_patterns_pick,roll.clipboard,PR_MAX_EVENTS,NULL);
  if(count<0){say("Choose a compatible pattern");return;}
  roll.clip_count=(unsigned)count;roll.clip_tick=0;roll.clip_layer=PR_PADS;
  roll_pattern_index=-1;roll_patterns_state=0;pr_reset_view(&roll);roll_refresh(1);roll.clip_row=roll.rows[roll.cursor_row];
  say("Copied: position cursor, Actions > Paste");repaint();return;
 }
 if(ok){roll_patterns_pick=pt_count()>old?(int)pt_count()-1:clamp(roll_patterns_pick,0,pt_count()?(int)pt_count()-1:0);roll_patterns_state=1;say("Pattern library updated; save project");}
 else if(pt_error()[0])say(pt_error());
 repaint();
}
static int roll_patterns_button(unsigned code,int shifted){
 if((code==BUTTON_MINUS||code==BUTTON_PLUS)&&shifted){if(!pt_history(code==BUTTON_MINUS?-1:1))say(pt_error());roll_patterns_pick=clamp(roll_patterns_pick,0,pt_count()?(int)pt_count()-1:0);repaint();return 1;}
 if(code==BUTTON_STOP||code==BUTTON_PLAY)return 2;
 if(code==0x0c||code==0x00){if(roll_patterns_state!=1)roll_patterns_state=1;else{roll_patterns_state=0;if(roll_pattern_index<0)roll_refresh(1);}repaint();return 1;}
 if(code==0x0d){if(roll_patterns_state==3){struct pt_entry e;if(pt_info((unsigned)roll_patterns_pick,&e)){roll_patterns_begin=0;roll_patterns_end=e.length;}}
  else roll_patterns_state=roll_patterns_state==2?1:2;
  repaint();return 1;
 }
 if(code==0x0e||code==0x6f){
  if(roll_patterns_state==2)roll_patterns_execute();
  else if(roll_patterns_state==3){if(pt_extract((unsigned)roll_patterns_pick,roll_patterns_begin,roll_patterns_end)){roll_patterns_pick=(int)pt_count()-1;roll_patterns_state=1;say("Extracted; tails retained; save project");}else say(pt_error());}
  else if(roll_patterns_state==4){if(pt_remove((unsigned)roll_patterns_pick)){roll_patterns_pick=clamp(roll_patterns_pick,0,pt_count()?(int)pt_count()-1:0);roll_patterns_state=1;}else say(pt_error());}
  else {struct pt_entry entry;if(pt_info((unsigned)roll_patterns_pick,&entry)){roll_pattern_index=roll_patterns_pick;roll_patterns_state=0;pr_reset_view(&roll);roll_pattern_refresh();say("Pattern edit; transport plays native sequence");}}
 }
 repaint();return 1;
}
static int roll_patterns_control(unsigned cc,unsigned value,int shifted){
 int delta=value>64?(int)value-128:(int)value;
 if(roll_patterns_state==3){struct pt_entry entry;if(!pt_info((unsigned)roll_patterns_pick,&entry))return 1;
  int64_t grid=pr_grid_ticks(roll.grid);
  if(cc==0x64){if(shifted)roll_patterns_end+=delta*grid;else roll_patterns_begin+=delta*grid;}
  else if(cc==0x10)roll_patterns_begin=(entry.length*(int64_t)value/127/grid)*grid;
  else if(cc==0x11)roll_patterns_end=(entry.length*(int64_t)value/127/grid)*grid;
  if(roll_patterns_begin<0)roll_patterns_begin=0;
  if(roll_patterns_begin>=entry.length)roll_patterns_begin=entry.length-1;
  if(roll_patterns_end<=roll_patterns_begin)roll_patterns_end=roll_patterns_begin+1;
  if(roll_patterns_end>entry.length)roll_patterns_end=entry.length;
 }else if(roll_patterns_state==1||roll_patterns_state==2){int limit=roll_patterns_state==2?7:pt_count()?(int)pt_count()-1:0;
  int*pick=roll_patterns_state==2?&roll_patterns_action:&roll_patterns_pick;
  if(cc==0x64)*pick=clamp(*pick+delta,0,limit);else if(cc==0x14||cc==0x10)*pick=(int)value*limit/127;
 }
 repaint();return 1;
}
static void roll_patterns_paint(void*g){
 colour(g,0xff000000);rect(g,0,0,320,240);colour(g,WHITE);text(g,"Back",0,6,105,23,14);
 text(g,roll_patterns_state==3?"Full range":"Actions",107,6,105,23,14);colour(g,AMBER);text(g,roll_patterns_state==1?"Open":"Do It",214,6,105,23,14);
 colour(g,AMBER);left_text(g,roll_patterns_state==3?"Extract Pattern":roll_patterns_state==4?"Delete Pattern?":"Parts / Patterns",6,35,308,24,17);
 if(roll_patterns_state==3){char text_value[96];snprintf(text_value,sizeof text_value,"Start %lld   End %lld",(long long)roll_patterns_begin,(long long)roll_patterns_end);colour(g,WHITE);left_text(g,text_value,10,90,300,25,14);colour(g,GREY);left_text(g,"DATA: start | Shift DATA: end",10,126,300,20,13);left_text(g,"K1 Start / K2 End; notes keep tails",10,153,300,20,12);}
 else if(roll_patterns_state==4){struct pt_entry entry;if(pt_info((unsigned)roll_patterns_pick,&entry)){colour(g,WHITE);left_text(g,entry.name,10,92,300,25,16);}colour(g,GREY);left_text(g,"Source sequence stays unchanged",10,130,300,20,13);}
 else {int pick=roll_patterns_state==2?roll_patterns_action:roll_patterns_pick;int count=roll_patterns_state==2?8:(int)pt_count(),first=pick>4?pick-4:0;
  for(int i=first;i<count&&i<first+6;++i){char label[112];if(roll_patterns_state==2)snprintf(label,sizeof label,"%s",roll_pattern_actions[i]);else{struct pt_entry e;if(!pt_info((unsigned)i,&e))continue;snprintf(label,sizeof label,"%s %.26s  %u ev",e.part?"P":" ",e.name,e.count);}
   int y=66+(i-first)*21;if(i==pick){colour(g,0xff303030);rect(g,6,y,308,21);}colour(g,i==pick?AMBER:WHITE);left_text(g,label,11,y,300,21,14);
  }
  if(!count){colour(g,GREY);left_text(g,"B2 Actions > Split or Empty",10,100,300,24,14);}
 }
 colour(g,AMBER);left_text(g,message[0]?message:"DATA / Fader: choose | Shift +/-: pattern history",5,201,310,24,11);
}
