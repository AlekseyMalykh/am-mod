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
 pattern_host_id=id;pattern_redraw=redraw;pattern_host=1;roll_owner=NULL;
 message[0]=0;pt_expect_epoch(roll_patterns_epoch);atomic_store(&piano_active,1);
 if(!roll_refresh(1)){roll_close();return 0;}
 pr_view_layer(&roll,PR_PADS,roll.rows[0]);pr_cursor(&roll,phase%entry.length,0);
 say("Shared pattern: edits affect all its blocks");return 1;
}
int step_pattern_active(void){return on_ui()&&pattern_host&&roll_refresh(0);}
void step_pattern_close(void){if(on_ui()&&pattern_host)roll_close();}
int step_pattern_button(unsigned code,int down,int shifted){
 if(!step_pattern_active())return 1;
 shift=shifted;
 if(!down){if(code==0x6f)roll_data_release(shifted);return 1;}
 if(code==BUTTON_PLAY||code==BUTTON_RECORD){say("Arrange playback/record not connected");return 1;}
 if(code==0x0d&&shifted){say("Follow needs Arrange playback");return 1;}
 if(code==0x00||(code==0x0c&&!shifted&&!roll_actions&&!roll_editing&&roll.layer==PR_PADS)){
  roll_close();return 1;
 }
 if(code==0x23&&roll.layer==PR_PADS){browse_bank=(browse_bank+(shifted?7:1))%8;repaint();return 1;}
 roll_button(code,shifted);repaint();return 1;
}
int step_pattern_control(unsigned cc,unsigned value,int shifted){
 if(!step_pattern_active()||value>127)return 1;
 shift=shifted;roll_control(cc,value,shifted);return 1;
}
int step_pattern_pad(unsigned pad,int shifted){
 if(step_pattern_active()&&pad<16)roll_pad(pad,shifted);
 return 1;
}
int step_pattern_paint(void*g,int shifted){
 if(!step_pattern_active())return 0;
 shift=shifted;
 if(message[0]&&milliseconds()>=message_until)message[0]=0;
 roll_paint(g);return 1;
}
