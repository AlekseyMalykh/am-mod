/* UI owner facade of the existing editor. No native Events pointers enter
 * this host; all notes/routes/commits are values in the Pattern Store. */
int step_pattern_open(uint64_t id,int64_t phase,void(*redraw)(void)){
 if(!ui_known){ui_thread=pthread_self();ui_known=1;}
 if(!on_ui()||atomic_load(&piano_active)||!redraw||phase<0)return 0;
 unsigned transport=step_native_transport_state();
 if(!(transport&2u)||(transport&5u))return 0;
 int index=pt_pattern_index(id);struct pt_entry entry;
 if(index<0||!pt_info((unsigned)index,&entry)||entry.count>PR_MAX_EVENTS||!roll_allocate())return 0;
 pr_reset(&roll);roll_actions=roll_editing=roll_action=roll_data_held=roll_data_moved=0;
 roll_erase_held=roll_erase_used=0;roll_row_confirm=-1;pattern_saved_bank=browse_bank;roll_pad_page=browse_bank=0;
 roll_patterns_state=0;roll_patterns_epoch=pt_epoch();roll_pattern_index=index;
 pattern_host_id=id;pattern_host_clip=pattern_split_result=0;pattern_separated=NULL;pattern_rows_held=pattern_rows_used=0;roll_pattern_serial=0;pattern_redraw=redraw;pattern_host=1;roll_owner=NULL;
 message[0]=0;pt_expect_epoch(roll_patterns_epoch);atomic_store(&piano_active,1);
 if(!roll_refresh(1)){roll_close();return 0;}
 pr_view_layer(&roll,PR_PADS,roll.rows[0]);pr_cursor(&roll,phase%entry.length,0);
 say(pt_pattern_assigned((unsigned)index)?"Shared pattern: edits affect all its blocks":"Unassigned instrument; editable draft only");return 1;
}
int step_pattern_open_clip(uint64_t id,int64_t phase,void(*redraw)(void),void(*separated)(uint64_t)){
 struct pt_arr_clip clip;uint64_t receipt=pt_serial();
 if(!separated||!pt_arr_clip_by_id(id,&clip)||pt_serial()!=receipt)return 0;
 if(!step_pattern_open(clip.pattern,phase,redraw))return 0;
 if(pt_serial()!=receipt){roll_close();return 0;}
 pattern_host_clip=id;pattern_separated=separated;return 1;
}
static void pattern_separate_selection(int copy){
 if(!pattern_host||!pattern_host_clip||!pattern_separated){say("Open a Song block to separate events");return;}
 if(roll_editing||roll.pending_commit){say("Apply or cancel the note edit first");return;}
 if(!pr_selected(&roll)){say("Select notes, or hold B2 + pads for rows");return;}
 uint64_t result=0;
 if(!pt_arr_separate(pattern_host_clip,pattern_host_id,roll_pattern_serial,roll.notes,roll.count,copy,&result)){say(pt_error());return;}
 /* Defer close until roll_button returns: its final repaint must still use
  * the external host callback, never a stale native Events surface. */
 pattern_split_result=result;
}
static int pattern_finish_separate(void){
 if(!pattern_split_result)return 0;
 uint64_t result=pattern_split_result;void(*done)(uint64_t)=pattern_separated;roll_close();if(done)done(result);return 1;
}
int step_pattern_active(void){return on_ui()&&pattern_host&&roll_refresh(0);}
void step_pattern_close(void){if(on_ui()&&pattern_host)roll_close();}
int step_pattern_button(unsigned code,int down,int shifted){
 if(!step_pattern_active())return 1;
 shift=shifted;
 if(!down){
  if(code==0x6f)roll_data_release(shifted);
  if(code==0x0d&&pattern_rows_held){int used=pattern_rows_used;pattern_rows_held=pattern_rows_used=0;if(!used)roll_button(code,0);repaint();}
  return 1;
 }
 if(code==BUTTON_PLAY||code==BUTTON_RECORD){say("Arrange playback/record not connected");return 1;}
 if(code==0x0d&&shifted){say("Follow needs Arrange playback");return 1;}
 if(pattern_rows_held&&code!=0x0d)pattern_rows_used=1;
 if(code==0x0e&&!shifted&&pattern_rows_held&&pattern_host_clip&&!roll_editing&&!roll_actions&&!roll_row_pending()){pattern_separate_selection(0);if(!pattern_finish_separate())repaint();return 1;}
 if(code==0x0d&&!shifted&&!roll_editing&&!roll_actions&&!roll_row_pending()){pattern_rows_held=1;pattern_rows_used=0;say("Hold B2 + pads: toggle complete rows");repaint();return 1;}
 if(code==0x00||(code==0x0c&&!shifted&&!roll_actions&&!roll_editing&&roll.layer==PR_PADS)){
  roll_close();return 1;
 }
 if(code==0x23&&roll.layer==PR_PADS){browse_bank=(browse_bank+(shifted?7:1))%8;repaint();return 1;}
 roll_button(code,shifted);
 if(pattern_finish_separate())return 1;
 if(pattern_host)repaint();
 return 1;
}
int step_pattern_control(unsigned cc,unsigned value,int shifted){
 if(!step_pattern_active()||value>127)return 1;
 shift=shifted;roll_control(cc,value,shifted);return 1;
}
int step_pattern_pad(unsigned pad,int shifted){
 if(step_pattern_active()&&pad<16){
  if(pattern_rows_held&&!roll_editing&&!roll_actions){
   pattern_rows_used=1;int value=roll.layer==PR_PADS?browse_bank*16+(int)pad:roll_pad_rows[pad];
   if(value<0){say("No event row on this pad");return 1;}
   roll_pad(pad,0);int any=0,enable=0;
   for(unsigned i=0;i<roll.count;++i)if((roll.layer==PR_PADS?roll.notes[i].sound:pr_note_row(&roll,roll.notes+i))==value){++any;if(!roll.notes[i].selected)enable=1;}
   for(unsigned i=0;i<roll.count;++i)if((roll.layer==PR_PADS?roll.notes[i].sound:pr_note_row(&roll,roll.notes+i))==value)roll.notes[i].selected=enable;
   roll.range=0;pr_cancel(&roll);say(any?"B3 Separate / release B2 to edit notes":"This row has no events");
  }else roll_pad(pad,shifted);
  repaint();
 }
 return 1;
}
int step_pattern_paint(void*g,int shifted){
 if(!step_pattern_active())return 0;
 shift=shifted;
 if(message[0]&&milliseconds()>=message_until)message[0]=0;
 roll_paint(g);return 1;
}
