/* Included by step_native.c: one UI owner and the existing native event history. */
static struct piano_roll roll;
static struct pr_note *roll_snapshot;
#include "ui_list_geometry.h"
static int roll_ready,roll_editing,roll_actions,roll_action,roll_source_steps;
static int64_t roll_delta_time,roll_delta_length;
static int roll_delta_row,roll_delta_velocity;
static int roll_fader_rows[241],roll_fader_count;
static uint64_t roll_last_scan;
static void *roll_owner;
static int roll_reference_note=-1;
static uint64_t roll_tune_lights;
static int roll_pad_rows[16],roll_pad_page,roll_audition=-1;
static int roll_pattern_insert_pending;
/* Song block shown in the pattern editor: song time = song_start + (local - offset) in the first cycle. */
#ifdef MPC_PATTERN_TOOLS_TRIAL
static int roll_song_valid;static int64_t roll_song_start,roll_song_length,roll_song_offset;
static int roll_song_local(int64_t song,int64_t*local){
 song=render_from+(render_to>render_from?song%(render_to-render_from):song);
 if(!roll_song_valid||roll.length<=0||song<roll_song_start||song>=roll_song_start+roll_song_length)return 0;
 *local=(roll_song_offset+(song-roll_song_start))%roll.length;return 1;}
static int64_t roll_song_time(int64_t local){return roll_song_valid?roll_song_start-roll_song_offset+local:local;}
#endif /* pattern editor: pad chose a row, click/B3 adds the event */
static unsigned roll_input_held;
static int roll_data_held,roll_data_moved; /* DATA held: encoder + fader draw a selection area */
static void roll_data_begin_move(void){if(roll_data_held&&!roll_data_moved){roll.anchor_tick=roll.cursor_tick;roll.anchor_row=roll.cursor_row;roll.range=1;roll_data_moved=1;}}
static void roll_data_release(int shift){
 if(!roll_data_held)return;
 roll_data_held=0;
 if(!atomic_load(&piano_active)){roll_data_moved=0;return;}
 if(roll_data_moved){pr_range_apply(&roll);roll.range=0;roll_data_moved=0;repaint();}
 else if(!roll_editing&&!roll_actions){pr_select(&roll,shift);repaint();}
}
static int roll_row_pending(void);static void roll_row_execute(int);
#ifdef MPC_PATTERN_TOOLS_TRIAL
#include "pattern_tools.h"
static int roll_pattern_index=-1,roll_patterns_state,roll_patterns_pick,roll_patterns_action;
static uint64_t roll_patterns_epoch;
static uint64_t roll_pattern_serial;
static void pattern_separate_selection(int);
static int64_t roll_patterns_begin,roll_patterns_end;
static void roll_patterns_open(void),roll_patterns_paint(void*);
static int roll_patterns_button(unsigned,int),roll_patterns_control(unsigned,unsigned,int),roll_pattern_refresh(void);
#endif
static int roll_erase_held,roll_erase_used;static int roll_row_confirm=-1,roll_row_layer,roll_row_sound;static uint64_t roll_row_until;
static void roll_layout_update(void);
#ifdef MPC_MIDI_TOOLS
static int roll_midi_state;
static void roll_midi_close(void),roll_midi_open(void),roll_midi_tick(void),roll_midi_paint(void*),roll_midi_pad(unsigned);
static int roll_midi_button(unsigned),roll_midi_control(unsigned,unsigned);
#endif
static const char *const roll_action_names[]={"Copy","Cut","Paste","Delete","Select all","Select none","Snap starts","Snap ends","Snap both","Grid","Tune / Chop","Show target pad","Insert event","Undo: Shift -","Redo: Shift +","Move / Length / Velocity","Import MIDI","Export MIDI"
#ifdef MPC_PATTERN_TOOLS_TRIAL
,"Parts / Patterns"
,"Separate selection","Copy selection"
#endif
};
#define ROLL_ACTIONS_COUNT ((int)(sizeof(roll_action_names)/sizeof(roll_action_names[0])))
static int roll_actions_count(void){
#ifdef MPC_PATTERN_TOOLS_TRIAL
 if(!pattern_host||!pattern_host_clip)return ROLL_ACTIONS_COUNT-2;
#endif
 return ROLL_ACTIONS_COUNT;
}
static const char *roll_grid_name(void){return roll.grid==10?"Off / 1 tick":grid_names[roll.grid];}
static int roll_sequence_index(void){
 void *things=controller?FIELD(controller,0x248,void*):NULL;
 void *sequence=things?FIELD(things,0x1c8,void*):NULL;
 int index=sequence?FIELD(sequence,0x480,int):-1;
 return index>=0&&index<128?index:-1;
}
static int roll_sequence_mutes(uint64_t muted[2]){
#ifdef MPC_PATTERN_TOOLS_TRIAL
 if(roll_pattern_index>=0){muted[0]=muted[1]=0;return 0;}
#endif
 int index=roll_sequence_index();muted[0]=muted[1]=0;
 return index>=0&&music_sequence_mutes_get((unsigned)index,muted);
}
static int roll_sample_settings(int pad,int *chops,struct music_sample *settings,int *tuning){
 if(chops)*chops=0;
 if(!controller||pad<0||pad>=128)return -1;
 void *things=FIELD(controller,0x248,void*),*program=things?FIELD(things,0xb8,void*):NULL;
 void *instrument=program?FN(0x286bbe0,void*(*)(void*,int))(program,pad):NULL;
 if(!instrument||!FIELD(instrument,0x22e8,void*))return -1;
 uintptr_t sample[2]={0};step_layer_sample(instrument,0,sample,base+0x2803b90);int root_note=-1;
 if(sample[0]){const char *name=FIELD((void*)sample[0],0xa8,const char*);
  if(name&&strnlen(name,513)>0&&strnlen(name,513)<513){struct music_sample s;music_get_sample(name,&s);root_note=s.root;if(settings)*settings=s;}
 }
 if(tuning)*tuning=FIELD(instrument,0xb98,int8_t)+FIELD(FIELD(instrument,0x22e8,void*),0x60,int8_t);
 if(sample[1])FN(0x123bca0,void(*)(void*))((void*)sample[1]);
 if(chops){struct step_event e={0};e.type=3;FIELD(&e,0x10,float)=1.f;e.data[32]=(unsigned char)FN(0x157f800,int(*)(void*,int))(controller,pad);*chops=event_slices(&e);}
 return root_note;
}
static int roll_sample_meta(int pad,int *chops){return roll_sample_settings(pad,chops,NULL,NULL);}
static int roll_layer_open(int layer,int pad){
#ifdef MPC_PATTERN_TOOLS_TRIAL
 if(roll_pattern_index>=0){roll_reference_note=60;roll.chop_count=128;roll_pad_page=0;pr_view_layer(&roll,layer,pad);roll_layout_update();return 1;}
#endif
 int chops=0,original=roll_sample_meta(pad,&chops);
 if(layer==PR_TUNE&&original<0){say("Set Orig Note: Sample > Tune > Shift K1");return 0;}
 if(layer==PR_CHOP&&chops<2){say("Split this sample into chops first");return 0;}
 if(roll_audition>=0){workflow_event_preview_stop(0);roll_audition=-1;}
 roll_reference_note=original;roll.chop_count=chops>0?chops:128;roll_pad_page=0;pr_view_layer(&roll,layer,pad);roll_layout_update();return 1;
}
static void roll_layout_update(void){
 if(roll.layer==PR_PADS)return;
#ifdef MPC_PATTERN_TOOLS_TRIAL
 if(roll_pattern_index>=0){int lo=roll.layer==PR_TUNE?108:0;lo+=roll_pad_page*16;for(int i=0;i<16;++i)roll_pad_rows[i]=lo+i;pr_layout_range(&roll,lo,lo+15);return;}
#endif
 struct music_sample settings={0};int tuning=0;
 roll_sample_settings(roll.sound,NULL,&settings,&tuning);
 roll_tune_lights=music_light_snapshot(&settings,tuning);
 int lo=241,hi=-1,origin=(int)atomic_load(&native_levels_origin);
 for(int i=0;i<16;++i){int v;
  if(roll.layer==PR_CHOP)v=roll_pad_page*16+i;
  else {int pitch=i-origin+settings.transpose+roll_pad_page*16;
   if(settings.mode==MUSIC_DEGREES){pitch=music_layout_semitones(roll_tune_lights,pitch);if(!roll_pad_page){int native_pitch;if(music_layout_pad_pitch(roll_tune_lights,(unsigned)i,origin,&native_pitch))pitch=native_pitch;}}
   v=pitch+120;
  }
  int max=roll.layer==PR_CHOP?roll.chop_count-1:240;
  roll_pad_rows[i]=v>=0&&v<=max?v:-1;
  if(roll_pad_rows[i]>=0){if(v<lo)lo=v;if(v>hi)hi=v;}
 }
 if(hi<lo){lo=roll.layer==PR_TUNE?120:0;hi=lo;}
 for(unsigned i=0;roll_editing&&i<roll.plan.added_count;++i){const struct step_event *e=roll.plan.added+i;
  if(event_pad(e)!=roll.sound)continue;
  int v=roll.layer==PR_TUNE?event_choice_value(e,0)+120:event_choice_value(e,15)-1;
  if(v>=0){if(v<lo)lo=v;if(v>hi)hi=v;}
 }
 pr_layout_range(&roll,lo,hi);
}
static int roll_read(unsigned *out_count,uint64_t *revision,unsigned char loaded[128]){
 if(!controller||!event_list||!roll_snapshot)return 0;
 void *end=(char*)event_list+0x68,*node=FIELD(event_list,0x78,void*);
 unsigned budget=65536,count=0;uint64_t h=UINT64_C(1469598103934665603);
 while(node!=end&&budget--){
  if(!node)return 0;
  const struct step_event *e=(void*)((char*)node+0x20);
  if(musical(e)){
   int pad=event_pad(e);if(pad<0||pad>127||count>=PR_MAX_EVENTS)return 0;
   struct pr_note *n=roll_snapshot+count++;memset(n,0,sizeof(*n));n->event=*e;n->sound=pad;
   unsigned mask=FIELD(e,0x14,uint16_t);
   n->tune=(mask&1)?event_choice_value(e,0):0;n->slice=(mask&0x8000)?event_choice_value(e,15):0;
   n->flags=event_categories(e);
   h=(h^(uint64_t)pad)*UINT64_C(1099511628211);h=(h^(uint64_t)e->time)*UINT64_C(1099511628211);h=(h^(e->type&~0x100u))*UINT64_C(1099511628211);
   for(unsigned i=0;i<37;++i)h=(h^e->data[i])*UINT64_C(1099511628211);
  }
  node=FN(0xb51738,void*(*)(void*))(node);
 }
 if(node!=end)return 0;
 if(loaded){
  memset(loaded,0,128);void *things=FIELD(controller,0x248,void*),*program=things?FIELD(things,0xb8,void*):NULL;
  if(program)for(int pad=0;pad<128;++pad){void *instrument=FN(0x286bbe0,void*(*)(void*,int))(program,pad);
   loaded[pad]=(unsigned char)(instrument&&FN(0x2804760,int(*)(void*))(instrument));
  }
 }
 *out_count=count;*revision=h^(uint64_t)length;return 1;
}
static int roll_encode(void *user,const struct pr_note*n,struct step_event*out){
#ifdef MPC_PATTERN_TOOLS_TRIAL
 if(roll_pattern_index>=0)return pt_encode((unsigned)roll_pattern_index,n,out);
#endif
 (void)user;if(n->sound<0||n->sound>127||n->slice<0||n->slice>128||n->tune< -120||n->tune>120)return 0;
 struct step_event e=n->event;
 int old_sound=event_pad(&e);e.data[32]=(unsigned char)FN(0x157f800,int(*)(void*,int))(controller,n->sound);
 if(e.data[32]>127)return 0;
 if(n->slice>0&&old_sound!=n->sound&&event_slices(&e)<n->slice)return 0;
 unsigned mask=FIELD(&e,0x14,uint16_t);
 int old_tune=(mask&1)?event_choice_value(&e,0):0,old_slice=(mask&0x8000)?event_choice_value(&e,15):0;
 if(old_tune!=n->tune)event_choice_set(&e,0,n->tune);
 if(old_slice!=n->slice){if(!n->slice)return 0;event_choice_set(&e,15,n->slice);}
 *out=e;return 1;
}
static int roll_commit(void *user,const struct pr_plan *plan){
#ifdef MPC_PATTERN_TOOLS_TRIAL
 if(roll_pattern_index>=0){(void)user;
  int link=pt_link_seq((unsigned)roll_pattern_index);
  if(link>=0){ /* Seq block: write into the sequence, then refresh the block from it */
   char msg[80];if(!step_native_edit_seq(link,plan,msg,sizeof msg)){snprintf(roll.error,sizeof roll.error,"%s",msg);return 0;}
   static struct step_event fresh[8192];unsigned n=0;int64_t len=0;const char*key=NULL;int map[128];
   if(step_native_read_seq(arrange_sequencer,link,fresh,8192,&n,&len,&key,map)==link)pt_link_sync((unsigned)roll_pattern_index,fresh,n,len,key,map);
   return 1;}
  return pt_commit((unsigned)roll_pattern_index,plan);}
#endif
 (void)user;
 if(!atomic_load(&piano_active)||roll_owner!=event_list||(step_native_transport_state()&4))return 0;
 unsigned count;uint64_t revision;
 if(!roll_read(&count,&revision,NULL)||revision!=plan->revision)return 0;
 struct add_context c={0};c.bulk=1;c.bulk_removed=plan->removed;c.bulk_added=plan->added;
 c.bulk_removed_count=plan->removed_count;c.bulk_added_count=plan->added_count;c.bulk_title=plan->title;
 if(!run_set_command(&c))return 0;
 roll_last_scan=0;return 1;
}
static void roll_close(void){
#ifdef MPC_PATTERN_TOOLS_TRIAL
 atomic_store(&pattern_lights_active,0);
 int external=pattern_host;
#else
 int external=0;
#endif
#ifdef MPC_PATTERN_TOOLS_TRIAL
 roll_pattern_index=-1;roll_patterns_state=0;
#endif
 if(atomic_load(&piano_active)&&!external)workflow_piano_input_stop();
 roll_input_held=0;roll_data_held=roll_data_moved=roll_erase_held=roll_erase_used=0;roll_row_confirm=-1;
#ifdef MPC_MIDI_TOOLS
 roll_midi_close();
#endif
 if(roll_audition>=0){workflow_event_preview_stop(0);roll_audition=-1;}
 if(roll_ready)pr_cancel(&roll);
 atomic_store(&piano_active,0);roll_actions=roll_editing=0;roll_owner=NULL;
#ifdef MPC_PATTERN_TOOLS_TRIAL
 if(external)browse_bank=pattern_saved_bank;
 pattern_host=0;pattern_host_id=pattern_host_clip=pattern_split_result=0;pattern_redraw=NULL;pattern_separated=NULL;pattern_rows_held=pattern_rows_used=0;
#endif
}
static void roll_project_reset(void){roll_close();if(roll_ready)pr_reset(&roll);}
static int roll_refresh(int force){
 if(!atomic_load(&piano_active))return 0;
#ifdef MPC_PATTERN_TOOLS_TRIAL
 if(pattern_host){
  if(roll_patterns_epoch!=pt_epoch()){roll_close();return 0;}
  int index=pt_pattern_index(pattern_host_id);if(index<0){roll_close();return 0;}
  if(pattern_host_clip&&roll_pattern_serial!=pt_serial()){struct pt_arr_clip c;if(!pt_arr_clip_by_id(pattern_host_clip,&c)||c.pattern!=pattern_host_id){roll_close();return 0;}}
  pt_expect_epoch(roll_patterns_epoch);roll_pattern_index=index;return roll_pattern_refresh();
 }
#endif
 if(!showing||roll_owner!=event_list){roll_close();return 0;}
#ifdef MPC_PATTERN_TOOLS_TRIAL
 if((roll_pattern_index>=0||roll_patterns_state)&&roll_patterns_epoch!=pt_epoch()){roll_close();say("Project changed: reopen Patterns");return 0;}
 if(roll_pattern_index>=0)return roll_pattern_refresh();
 if(roll_patterns_state)return 1;
#endif
#ifdef MPC_MIDI_TOOLS
 roll_midi_tick();
#endif
 pr_follow(&roll,play_tick,was_playing);
 uint64_t now=milliseconds();if(!force&&now-roll_last_scan<100)return 1;
 roll_last_scan=now;unsigned n;uint64_t revision;unsigned char loaded[128];
 if(step_native_transport_state()&4u){pr_cancel(&roll);roll_editing=roll_actions=0;}
 if(!roll_read(&n,&revision,loaded)){roll_close();say("Piano Roll: event limit or unreadable data");return 0;}
 if(!pr_load(&roll,roll_snapshot,n,length,revision,loaded)){roll_close();say("Piano Roll: unsupported event data");return 0;}
 if(roll.layer!=PR_PADS){int chops=0;int original=roll_sample_meta(roll.sound,&chops);
  if((roll.layer==PR_TUNE&&original<0)||(roll.layer==PR_CHOP&&chops<2)){
   pr_view_layer(&roll,PR_PADS,roll.sound);roll_editing=0;say(original<0?"Orig Note unavailable: Pad view":"Chops unavailable: Pad view");
  }else {roll_reference_note=original;if(roll.layer==PR_CHOP&&roll.chop_count!=chops){roll.chop_count=chops;pr_rows(&roll);}}
 }
 roll_layout_update();
 return 1;
}
static int roll_allocate(void){
 if(!roll_ready){struct pr_ops ops={NULL,roll_encode,roll_commit};
  if(!pr_init(&roll,ops))return 0;
  roll_snapshot=calloc(PR_MAX_EVENTS,sizeof(*roll_snapshot));
  if(!roll_snapshot){pr_destroy(&roll);return 0;}
  roll_ready=1;
 }
 return 1;
}
static void roll_open(void){
 if(menu)return;
 if(!roll_allocate()){say("Piano Roll allocation failed");return;}
 pr_reset_view(&roll);roll_source_steps=steps;roll_owner=event_list;roll_editing=roll_actions=0;
 atomic_store(&piano_active,1);
 if(!roll_refresh(1))return;
 roll.grid=grid;pr_view_layer(&roll,PR_PADS,sound);pr_cursor(&roll,edit_tick,pr_row_index(&roll,sound));
 reset_mute();atomic_store(&write_armed,0);atomic_store(&toggle_armed,0);follow=0;
 invalidate();publish();repaint();report("PIANO_ROLL OPEN shared_native_events");
}
static void roll_error(int result,const char *ok){
 if(!result)say(roll.error[0]?roll.error:"Piano Roll action unavailable");else if(ok)say(ok);
 roll_refresh(1);repaint();
}
static unsigned roll_categories(int pad){unsigned flags=0;for(unsigned i=0;i<roll.count;++i)if(roll.notes[i].sound==pad)flags|=roll.notes[i].flags;return flags;}
static void roll_enter(void){
 int pad=roll.layer==PR_PADS?roll.rows[roll.cursor_row]:roll.sound;
#ifdef MPC_PATTERN_TOOLS_TRIAL
 if(roll_pattern_index>=0){roll_layer_open((roll_categories(pad)&2)?PR_CHOP:PR_TUNE,pad);roll_editing=0;repaint();return;}
#endif
 unsigned flags=roll_categories(pad);int chops=0,original=roll_sample_meta(pad,&chops);
 int layer=original>=0&&(!(flags&1)||(flags&2))?PR_TUNE:chops>=2?PR_CHOP:PR_TUNE;
 if(roll_layer_open(layer,pad))roll_editing=0;
 repaint();
}
static void roll_edit_start(void){
 if(!pr_selected(&roll))pr_select(&roll,0);
 if(!pr_selected(&roll)){say("Select an event or Insert from Actions");return;}
 roll_fader_count=roll.row_count;memcpy(roll_fader_rows,roll.rows,(size_t)roll.row_count*sizeof(int));
 roll_editing=1;roll_delta_time=roll_delta_length=0;roll_delta_row=roll_delta_velocity=0;pr_cancel(&roll);
}
#ifdef MPC_PATTERN_TOOLS_TRIAL
#include "pattern_roll.inc"
#endif
static void roll_action_run(void){
 int action=roll_action;roll_actions=0;
 if(!roll_refresh(1))return;
 if(roll.pending_commit){say("Waiting for native history");return;}
 if(action==0)roll_error(pr_copy(&roll),"Selection copied");
 else if(action==1)roll_error(pr_delete(&roll,1),"Selection cut; Undo / Redo available");
 else if(action==2)roll_error(pr_paste(&roll),"Selection pasted; Undo / Redo available");
 else if(action==3)roll_error(pr_delete(&roll,0),"Selection deleted; Undo / Redo available");
 else if(action==4){pr_select_all(&roll);repaint();}
 else if(action==5){for(unsigned i=0;i<roll.count;++i)roll.notes[i].selected=0;roll.range=0;pr_cancel(&roll);say("Selection cleared");}
 else if(action>=6&&action<=8)roll_error(pr_quantize(&roll,(enum pr_snap)(action-6)),"Selection quantized; Undo / Redo available");
 else if(action==9){roll.grid=(roll.grid+1)%11;char msg[64];snprintf(msg,sizeof(msg),"Grid: %s",roll_grid_name());say(msg);}
 else if(action==10){if(roll.layer==PR_PADS)roll_enter();else roll_layer_open(roll.layer==PR_TUNE?PR_CHOP:PR_TUNE,roll.sound);}
 else if(action==11){
  if(roll.layer!=PR_PADS){say("Show target pad is available on Pad rows");return;}
  int pad=roll.rows[roll.cursor_row];pad=(pad+1)%128;roll.pinned[pad]=1;pr_rows(&roll);pr_cursor(&roll,roll.cursor_tick,pr_row_index(&roll,pad));
 }else if(action==12){
  int pad=roll.layer==PR_PADS?roll.rows[roll.cursor_row]:roll.sound;
  unsigned note;
#ifdef MPC_PATTERN_TOOLS_TRIAL
  if(roll_pattern_index>=0)note=(unsigned)pt_note_number((unsigned)roll_pattern_index,pad);else
#endif
  note=(unsigned)FN(0x157f800,int(*)(void*,int))(controller,pad);
  if(note>127){say("Destination pad has no native note");return;}
  struct step_event e=FN(0x27e54d0,struct step_event(*)(unsigned,int64_t,int64_t,unsigned,unsigned,float))(note,roll.cursor_tick,pr_grid_ticks(roll.grid),100,0,100.f/127.f);
  if(roll.layer==PR_TUNE)event_choice_set(&e,0,roll.rows[roll.cursor_row]-120);
  if(roll.layer==PR_CHOP){int slice=roll.rows[roll.cursor_row]+1;int available;
#ifdef MPC_PATTERN_TOOLS_TRIAL
   if(roll_pattern_index>=0)available=128;else
#endif
   available=event_slices(&e);
   if(available<slice){say("This Chop is unavailable");return;}event_choice_set(&e,15,slice);}
  pr_cancel(&roll);roll.plan.title="Piano Roll Insert";roll.plan.revision=roll.revision;roll.plan.added[0]=e;roll.plan.added_count=1;
  roll_error(pr_apply(&roll),"Event inserted; Undo / Redo available");
 }else if(action==15)roll_edit_start();
#ifdef MPC_PATTERN_TOOLS_TRIAL
 else if(action==18){if(pattern_host)say("Return to Events for Parts / Patterns");else roll_patterns_open();}
 else if(action==19||action==20)pattern_separate_selection(action==20);
#endif
#ifdef MPC_MIDI_TOOLS
 else if(action==16){
#ifdef MPC_PATTERN_TOOLS_TRIAL
  if(roll_pattern_index>=0){say("Return to sequence for MIDI import");return;}
#endif
  roll_midi_open();
 }
 else if(action==17){char path[512],error[96];
#ifdef MPC_PATTERN_TOOLS_TRIAL
  if(roll_pattern_index>=0){say("Copy pattern to sequence for MIDI export");return;}
#endif
  void *things=FIELD(controller,0x248,void*);
  if(step_native_transport_state()&4){say("Stop recording before MIDI export");return;}
  if(am_midi_file_export(base,am_midi_file_project(base),things,sequence,NULL,path,error)){char text[160];const char*name=strrchr(path,'/');snprintf(text,sizeof(text),"Saved %.60s",name?name+1:path);say(text);}
  else say(error);
 }
#endif
 else say("Hold Shift and press - Undo or + Redo");
}
#ifdef MPC_PATTERN_TOOLS_TRIAL
static int roll_empty_cell(void){
 if(roll.cursor_row<0||roll.cursor_row>=roll.row_count)return 0;
 for(unsigned i=0;i<roll.count;++i){const struct pr_note*n=roll.notes+i;int64_t gate;memcpy(&gate,n->event.data+24,8);
  if(pr_note_row(&roll,n)==roll.rows[roll.cursor_row]&&roll.cursor_tick>=n->event.time&&roll.cursor_tick-n->event.time<(gate>0?gate:1))return 0;
 }
 return 1;
}
#endif
static int roll_button(unsigned code,int event_shift){
 if(!atomic_load(&piano_active))return 0;
 int insert_pending=roll_pattern_insert_pending;roll_pattern_insert_pending=0;(void)insert_pending;
#ifdef MPC_PATTERN_TOOLS_TRIAL
 if(roll_patterns_state)return roll_patterns_button(code,event_shift);
 if(roll_pattern_index>=0){
  if((code==BUTTON_MINUS||code==BUTTON_PLUS)&&event_shift){pr_cancel(&roll);roll_editing=roll_actions=0;if(!pt_history(code==BUTTON_MINUS?-1:1))say(pt_error());roll_refresh(1);return 1;}
  if(code==0x0c&&!event_shift&&!roll_actions&&!roll_editing&&roll.layer==PR_PADS){roll_pattern_index=-1;pr_reset_view(&roll);roll_patterns_open();return 1;}
  if(code==BUTTON_RECORD||code==BUTTON_MUTE||code==BUTTON_ERASE){say("Use Actions to edit pattern events");return 1;}
 }
#endif
 if(roll_row_pending()&&(code==0x0e||code==0x0c)){
  int value=roll_row_confirm;
  if(code==0x0e)roll_row_execute(value);else{roll_row_confirm=-1;say("Row delete cancelled");}
  repaint();return 1;
 }
 if(code!=0x31&&code!=BUTTON_ERASE)roll_row_confirm=-1; /* Erase itself must not cancel Erase + pad confirmation. */
#ifdef MPC_MIDI_TOOLS
 if(roll_midi_state)return roll_midi_button(code);
#endif
 if((code==BUTTON_MINUS||code==BUTTON_PLUS)&&event_shift){
  pr_cancel(&roll);roll_editing=roll_actions=0;roll_last_scan=0;
  return 2; /* Native Shift - / Shift + owns the common Undo / Redo history. */
 }
 if(code==BUTTON_MUTE){
  if(!roll_actions&&roll.cursor_row>=0&&roll.cursor_row<roll.row_count){
   int sequence=roll_sequence_index(),pad=roll.layer==PR_PADS?roll.rows[roll.cursor_row]:roll.sound;
   if(sequence>=0&&pad>=0&&pad<128){
    int muted=music_sequence_pad_toggle((unsigned)sequence,(unsigned)pad);
    void *things=FIELD(controller,0x248,void*);
    if(seq_mutes_refresh)seq_mutes_refresh(FIELD(things,0xb8,void*),FIELD(things,0x1c8,void*));
    char name[8],msg[64];pad_name(pad,name);snprintf(msg,sizeof(msg),"%s Seq mute: %s",name,muted?"On":"Off");say(msg);
   }else say("Sequence mute unavailable");
  }
  repaint();return 1;
 }
 if(code==BUTTON_STOP||code==BUTTON_PLAY||code==BUTTON_RECORD||code==0x31){
  if(code==BUTTON_STOP){workflow_piano_input_stop();roll_input_held=0;}
  if(code==BUTTON_RECORD){pr_cancel(&roll);roll_editing=roll_actions=0;}
  return 2;
 }
 if(code==0x00){roll_close();steps=roll_source_steps;invalidate();return 1;}
 if(code==0x0c){
  if(event_shift){roll_editing=roll_actions=0;pr_cancel(&roll);roll.note_focus=!roll.note_focus;roll.focused=-1;say(roll.note_focus?"Notes: DATA browse; Shift click adds":"Cursor: DATA time; B2 range");}
  else if(roll_actions)roll_actions=0;
  else if(roll_editing){roll_editing=0;pr_cancel(&roll);}
  else if(roll.layer!=PR_PADS){roll_pad_up((unsigned)roll_audition);pr_view_layer(&roll,PR_PADS,roll.sound);}
  else{int64_t tick=roll.cursor_tick;roll_close();steps=roll_source_steps;FN(0x15814a0,void(*)(void*,int64_t))(controller,tick);edit_tick=tick;cursor=(int)(tick/grid_ticks[grid]);page=cursor/16;invalidate();}
 }else if(code==0x0d){if(event_shift){roll.follow=!roll.follow;pr_follow(&roll,play_tick,was_playing);say(roll.follow?"Follow on":"Follow off");}else if(!roll_editing&&!roll_actions)pr_range(&roll);}
 else if(code==0x0e){
  if(event_shift){roll_actions=!roll_actions;roll_action=clamp(roll_action,0,roll_actions_count()-1);roll_editing=0;pr_cancel(&roll);}
  else if(roll_actions)roll_action_run();
  else if(roll_editing){int result=roll.plan.removed_count?pr_apply(&roll):1;roll_editing=0;roll_error(result,"Event edit applied; Undo / Redo available");}
#ifdef MPC_PATTERN_TOOLS_TRIAL
  else if(pattern_host&&!pr_selected(&roll)&&roll_empty_cell()){roll_action=12;roll_action_run();}
#endif
  else if(roll.layer==PR_PADS&&!pr_selected(&roll))roll_enter();else roll_edit_start();
 }else if(code==0x6f){if(roll_actions)roll_action_run();
#ifdef MPC_PATTERN_TOOLS_TRIAL
  else if(insert_pending&&pattern_host&&!roll_editing&&!pr_selected(&roll)&&roll_empty_cell()){roll_action=12;roll_action_run();}
#endif
  else if(!roll_editing){roll_data_held=1;roll_data_moved=0;}} /* selects on release unless the area was drawn */
 else if(code==BUTTON_MINUS||code==BUTTON_PLUS){if(!roll_editing&&!roll_actions)pr_cursor(&roll,roll.cursor_tick,roll.cursor_row+(code==BUTTON_PLUS?1:-1));}
 else if(code==BUTTON_ERASE){roll_editing=roll_actions=0;roll_erase_held=1;roll_erase_used=0;} /* Erase alone deletes the selection on release; Erase + pad clears a row. */
 else if(code==0x23){
  if(roll.layer==PR_PADS)set_sound_bank((browse_bank+(event_shift?7:1))%8);
  else {if(roll_audition>=0){workflow_event_preview_stop(0);roll_audition=-1;}int low=roll.layer==PR_CHOP?0:-7,high=roll.layer==PR_CHOP?(roll.chop_count-1)/16:7;
   roll_pad_page+=event_shift?-1:1;if(roll_pad_page>high)roll_pad_page=low;if(roll_pad_page<low)roll_pad_page=high;roll_layout_update();}
 }
 else if(code==BUTTON_LEVELS||code==BUTTON_CHOP){
  int pad=roll.layer==PR_PADS?roll.rows[roll.cursor_row]:roll.sound;
  if(roll_layer_open(code==BUTTON_CHOP?PR_CHOP:PR_TUNE,pad))roll_editing=roll_actions=0;
 }else if(code==6||code==7){roll_close();return 2;}
 else return 1;
#ifdef MPC_PATTERN_TOOLS_TRIAL
 if(pattern_host&&pattern_split_result)return 1; /* Host finalizes without repainting the retired clip. */
#endif
 repaint();return 1;
}
static int roll_control(unsigned cc,unsigned v,int event_shift){
 if(!atomic_load(&piano_active))return 0;
#ifdef MPC_PATTERN_TOOLS_TRIAL
 if(roll_patterns_state)return roll_patterns_control(cc,v,event_shift);
#endif
#ifdef MPC_MIDI_TOOLS
 if(roll_midi_state)return roll_midi_control(cc,v);
#endif
 int delta=v>64?(int)v-128:(int)v;
 if(roll_actions){int count=roll_actions_count();if(cc==0x64)roll_action=clamp(roll_action+delta,0,count-1);else if(cc==0x14)roll_action=ui_list_fader(v,count-1);else if(cc==0x10)roll_action=(int)(v*(unsigned)count/128);repaint();return 1;}
 if(cc==0x64){if(event_shift){pr_zoom(&roll,1,roll.zoom-delta);repaint();return 1;}roll.follow=0;if(!roll_editing){if(roll.note_focus&&!roll.range&&!event_shift&&!roll_data_held)pr_browse(&roll,delta);else{roll_data_begin_move();pr_cursor(&roll,roll.cursor_tick+(int64_t)delta*(event_shift?1:pr_grid_ticks(roll.grid)),roll.cursor_row);if(roll_data_moved)pr_range_apply(&roll);}}
  else {roll_delta_time+=(int64_t)delta*(event_shift?1:pr_grid_ticks(roll.grid));roll_error(pr_edit(&roll,roll_delta_time,roll_delta_row,roll_delta_length,roll_delta_velocity),NULL);}
  repaint();return 1;}
 if(cc==0x14){if(event_shift){pr_zoom(&roll,0,12+(int)v*27/128);repaint();return 1;}if(!roll_editing){roll_data_begin_move();pr_cursor(&roll,roll.cursor_tick,(int)v*(roll.row_count-1)/127);if(roll_data_moved)pr_range_apply(&roll);}
  else {for(unsigned i=0;i<roll.count;++i)if(roll.notes[i].selected){
    int row=(int)v*(roll_fader_count-1)/127;roll_delta_row=roll_fader_rows[row]-pr_note_row(&roll,roll.notes+i);
    break;}
   roll_error(pr_edit(&roll,roll_delta_time,roll_delta_row,roll_delta_length,roll_delta_velocity),NULL);}
  repaint();return 1;}
 if(cc<0x10||cc>0x12)return 1;
 int k=(int)cc-0x10;
 if(roll_editing){
  if(k==0){roll.follow=0;roll_delta_time=((int64_t)v-64)*(event_shift?1:pr_grid_ticks(roll.grid));}
  if(k==1){if(event_shift){for(unsigned i=0;i<roll.count;++i)if(roll.notes[i].selected){float velocity;memcpy(&velocity,roll.notes[i].event.data,4);roll_delta_velocity=clamp((int)v,1,127)-(int)lroundf(velocity*127.f);break;}}
   else roll_delta_row=(int)v-64;}
  if(k==2)roll_delta_length=((int64_t)v-64)*(event_shift?1:pr_grid_ticks(roll.grid));
  roll_error(pr_edit(&roll,roll_delta_time,roll_delta_row,roll_delta_length,roll_delta_velocity),NULL);
 }else{
  if(k==2){roll.follow=0;roll.left=(int64_t)v*(roll.length>roll.span?roll.length-roll.span:0)/127;}
  if(k==0)roll.top=(int)v*(roll.row_count>roll.rows_visible?roll.row_count-roll.rows_visible:0)/127;
  if(k==1)roll.grid=(int)v*11/128;
 }repaint();return 1;
}
static void roll_erase_release(void){
 if(!roll_erase_held)return;
 roll_erase_held=0;
 if(!roll_erase_used&&atomic_load(&piano_active))roll_error(pr_delete(&roll,0),"Events deleted; Undo / Redo available");
 roll_erase_used=0;
}
/* Erase + pad: delete every event of that row; the same Erase + pad again confirms. */
static void roll_row_execute(int value){
 roll_row_confirm=-1;
 if(!roll_refresh(1))return;
 if(roll.pending_commit){say("Waiting for native history");return;}
 char name[24]="";
 if(roll.layer==PR_PADS){char tmp[8];pad_name(value,tmp);snprintf(name,sizeof(name),"pad %s",tmp);}else snprintf(name,sizeof(name),"row %d",roll.layer==PR_TUNE?value-120:value+1);
 for(unsigned i=0;i<roll.count;++i)roll.notes[i].selected=pr_note_row(&roll,roll.notes+i)==value;
 char msg[64];snprintf(msg,sizeof(msg),"%s cleared; Undo / Redo available",name);
 roll_error(pr_delete(&roll,0),msg);
}
static int roll_row_pending(void){
 return roll_row_confirm>=0&&roll_row_layer==roll.layer&&roll_row_sound==roll.sound&&milliseconds()<roll_row_until;
}
static void roll_row_clear(int value){
 unsigned count=0;
 for(unsigned i=0;i<roll.count;++i)if(pr_note_row(&roll,roll.notes+i)==value)++count;
 char name[24]="";
 if(roll.layer==PR_PADS){char tmp[8];pad_name(value,tmp);snprintf(name,sizeof(name),"pad %s",tmp);}else snprintf(name,sizeof(name),"row %d",roll.layer==PR_TUNE?value-120:value+1);
 uint64_t now=milliseconds();
 if(!count){roll_row_confirm=-1;char msg[48];snprintf(msg,sizeof(msg),"%s has no events",name);say(msg);return;}
 roll_row_confirm=value;roll_row_layer=roll.layer;roll_row_sound=roll.sound;roll_row_until=now+15000;
 char msg[96];snprintf(msg,sizeof(msg),"Erase %u events on %s? B1 Cancel, B3 Do It!",count,name);say(msg);message_until=milliseconds()+15000;
}
/* Event of this row (pad / tune / chop value) the cursor stands on or next to (half a grid step). */
static int roll_near_note(int value){
 int64_t tol=pr_grid_ticks(roll.grid)/2,best_d=0;int best=-1;
 for(unsigned i=0;i<roll.count;++i){const struct pr_note*n=roll.notes+i;
  int row=roll.layer==PR_PADS?n->sound:pr_note_row(&roll,n);if(row!=value)continue;
  int64_t t=n->event.time,d=FIELD(&n->event,0x28,int64_t);if(d<=0)d=1;
  int64_t dist=roll.cursor_tick<t?t-roll.cursor_tick:roll.cursor_tick>=t+d?roll.cursor_tick-(t+d)+1:0;
  if(dist>tol)continue;
  if(best<0||dist<best_d){best=(int)i;best_d=dist;}
 }return best;
}
static void roll_pad(unsigned pad,int pad_shift){
#ifdef MPC_PATTERN_TOOLS_TRIAL
 if(roll_patterns_state)return;
 if(roll_pattern_index>=0){
  if(pad<16){int value=roll.layer==PR_PADS?browse_bank*16+(int)pad:roll_pad_rows[pad];if(roll.layer==PR_PADS&&value>=0&&value<128){roll.pinned[value]=1;pr_rows(&roll);}int row=pr_row_index(&roll,value);if(row>=0){pr_cursor(&roll,roll.cursor_tick,row);if(pad_shift)pr_select(&roll,1);
   else if(!roll_editing&&!roll_actions&&!pr_selected(&roll)&&roll_empty_cell()){char name[16],msg[64];if(roll.layer==PR_PADS)pad_name(value,name);else snprintf(name,sizeof name,"Row");
    snprintf(msg,sizeof msg,"%s: click or B3 adds the event here",name);say(msg);roll_pattern_insert_pending=1;}}}
  repaint();return;
 }
#endif
#ifdef MPC_MIDI_TOOLS
 if(roll_midi_state){roll_midi_pad(pad);return;}
#endif
 if(!atomic_load(&piano_active)||pad>=16||roll_actions)return;
 int value=roll.layer==PR_PADS?browse_bank*16+(int)pad:roll_pad_rows[pad];
 if(value<0)return;
 if((roll_erase_held||atomic_load(&requested_erase))&&!roll_editing){roll_erase_used=1;roll_row_clear(value);repaint();return;}
 roll_row_confirm=-1;
 if(pad_shift&&!roll_editing){ /* Shift + pad: add / remove this pad's event at the cursor to the selection. */
  int idx=roll_near_note(value);char name[16],msg[64];
  if(roll.layer==PR_PADS)pad_name(value,name);else snprintf(name,sizeof(name),"Row");
  if(idx>=0){roll.notes[idx].selected=!roll.notes[idx].selected;roll.range=0;pr_cancel(&roll);
   snprintf(msg,sizeof(msg),"%s %s (%u selected)",name,roll.notes[idx].selected?"added":"removed",pr_selected(&roll));}
  else snprintf(msg,sizeof(msg),"%s: no event at cursor",name);
  say(msg);repaint();return;
 }
 if(roll_editing){if(roll.layer==PR_PADS)return;
  for(unsigned i=0;i<roll.count;++i)if(roll.notes[i].selected){roll_delta_row=value-pr_note_row(&roll,roll.notes+i);break;}
  roll_error(pr_edit(&roll,roll_delta_time,roll_delta_row,roll_delta_length,roll_delta_velocity),NULL);
 }
 if(roll.layer==PR_PADS){roll.pinned[value]=1;pr_rows(&roll);value=pr_row_index(&roll,value);}
 else value=pr_row_index(&roll,value);
 pr_cursor(&roll,roll.cursor_tick,value);
 if(!roll_editing){
  unsigned sound=roll.layer==PR_PADS?(unsigned)(browse_bank*16+(int)pad):(unsigned)roll.sound;
  unsigned note=(unsigned)FN(0x157f800,int(*)(void*,int))(controller,(int)sound);
  struct step_event e=FN(0x27e54d0,struct step_event(*)(unsigned,int64_t,int64_t,unsigned,unsigned,float))(note,0,0,100,0,roll_input_velocity);
  if(roll.layer!=PR_PADS)event_choice_set(&e,roll.layer==PR_TUNE?0:15,roll.layer==PR_TUNE?roll_pad_rows[pad]-120:roll_pad_rows[pad]+1);
  void *things=FIELD(controller,0x248,void*),*program=things?FIELD(things,0xb8,void*):NULL;
  if(program&&workflow_piano_input(program,&e,pad))roll_input_held|=1u<<pad;
  else say("Pad input unavailable");
 }else if(roll.layer!=PR_PADS){
  roll_pad_up((unsigned)roll_audition);
  unsigned note=(unsigned)FN(0x157f800,int(*)(void*,int))(controller,roll.sound);
  struct step_event e=FN(0x27e54d0,struct step_event(*)(unsigned,int64_t,int64_t,unsigned,unsigned,float))(note,0,pr_grid_ticks(roll.grid),100,0,roll_input_velocity);
  event_choice_set(&e,roll.layer==PR_TUNE?0:15,roll.layer==PR_TUNE?roll_pad_rows[pad]-120:roll_pad_rows[pad]+1);
  void *things=FIELD(controller,0x248,void*),*program=things?FIELD(things,0xb8,void*):NULL;
  if(program&&workflow_event_preview(program,&e,pad,roll.layer==PR_TUNE?1:8)==1)roll_audition=(int)pad;
 }repaint();
}
static void roll_pad_up(unsigned pad){if(pad<16&&(roll_input_held&(1u<<pad))){workflow_piano_input(NULL,NULL,pad);roll_input_held&=~(1u<<pad);}if(roll_audition>=0&&(unsigned)roll_audition==pad){workflow_event_preview_stop(0);roll_audition=-1;}}
#define ROLL_DIM .22f /* pads with events elsewhere; pads at the cursor are brighter, selected ones white */
static int roll_show_transport(void){
#ifdef MPC_PATTERN_TOOLS_TRIAL
 if(roll_pattern_index>=0){int64_t l;return was_playing&&roll_song_local(play_tick,&l);}
#endif
 return was_playing;
}
static int64_t roll_transport_tick(void){
#ifdef MPC_PATTERN_TOOLS_TRIAL
 int64_t l;if(roll_pattern_index>=0&&roll_song_local(play_tick,&l))return l;
#endif
 return play_tick;
}
static void roll_palette(void){
 if(!atomic_load(&piano_active))return;
 atomic_store(&event_edit_mask,roll.layer==PR_TUNE?1u:roll.layer==PR_CHOP?2u:category_buttons(roll_categories(roll.rows[roll.cursor_row])));
 uint64_t phase=milliseconds();
 for(unsigned pad=0;pad<16;++pad){
  int value=roll.layer==PR_PADS?browse_bank*16+(int)pad:roll_pad_rows[pad];
  uint32_t hue=0;float brightness=0.f;int selected=0,playing=0,near_chosen=0;
  if(value>=0){
   int near_note=roll_near_note(value);
   if(roll.layer==PR_PADS){int present=roll.loaded[value];for(unsigned i=0;i<roll.count;++i)if(roll.notes[i].sound==value)present=1;
    if(present){hue=music_pad_rgb((unsigned)value);brightness=ROLL_DIM;}
    selected=roll.rows[roll.cursor_row]==value;
   }else {hue=roll.layer==PR_CHOP?music_pad_rgb((unsigned)roll.sound):0xffffffu;brightness=ROLL_DIM;
    if(roll.layer==PR_TUNE){int replace=0;uint64_t light=music_pitch_light(roll_tune_lights,value-120,&replace);if(replace){hue=(uint32_t)light;uint32_t bits=(uint32_t)(light>>32);memcpy(&brightness,&bits,4);}}
    selected=roll.rows[roll.cursor_row]==value;
   }
   if(near_note>=0){brightness=.85f;near_chosen=roll.notes[near_note].selected;}
#ifdef MPC_PATTERN_TOOLS_TRIAL
   /* Row selection spans the entire pattern and survives bank changes. A
    * selected row must be visible even when its notes are away from cursor. */
   if(pattern_host&&pattern_rows_held)for(unsigned i=0;i<roll.count;++i)
    if(roll.notes[i].selected&&(roll.layer==PR_PADS?roll.notes[i].sound:pr_note_row(&roll,roll.notes+i))==value){near_chosen=1;break;}
#endif
   if(roll_show_transport())for(unsigned i=0;i<roll.count;++i){const struct pr_note *n=roll.notes+i;int row=roll.layer==PR_PADS?n->sound:pr_note_row(&roll,n);
    if(row==value&&!(n->event.type&0x200)&&roll_transport_tick()>=n->event.time&&roll_transport_tick()<n->event.time+FIELD(&n->event,0x28,int64_t)){playing=1;break;}}
  }
  atomic_store(&lights[pad],playing?pad_light(PAD_PLAYHEAD,.95f):near_chosen?pad_light(0xffffffu,1.f):selected?pad_selection_light(hue?hue:PAD_SELECT,brightness,phase):pad_light(hue,brightness));
 }
#ifdef MPC_PATTERN_TOOLS_TRIAL
 if(pattern_host)atomic_store(&pattern_lights_active,1u+(unsigned)(roll.layer==PR_PADS?browse_bank:roll.sound/16));
#endif
}
static int roll_x(int64_t tick){return 70+(int)((tick-roll.left)*240/roll.span);}
static int roll_y(int row){return 190-(row-roll.top+1)*roll.row_height;}
static void roll_rect(void*g,int x,int y,int w,int h,uint32_t ink){
 int right=x+w,bottom=y+h;if(x<70)x=70;if(right>310)right=310;if(y<70)y=70;if(bottom>190)bottom=190;
 if(right>x&&bottom>y){colour(g,ink);rect(g,x,y,right-x,bottom-y);}
}
static void roll_paint(void*g){
#ifdef MPC_PATTERN_TOOLS_TRIAL
 if(roll_patterns_state){roll_patterns_paint(g);return;}
#endif
#ifdef MPC_MIDI_TOOLS
 if(roll_midi_state){roll_midi_paint(g);return;}
#endif
 colour(g,0xff000000);rect(g,0,0,320,240);
 const char*back_label=roll_row_pending()?"Cancel":shift?(roll.note_focus?"Cursor":"Notes"):roll_actions?"Close":roll_editing?"Cancel":roll.layer==PR_PADS?(roll_source_steps?"Steps":"Events"):"Pad view";
#ifdef MPC_PATTERN_TOOLS_TRIAL
 if(roll_pattern_index>=0&&!shift&&!roll_editing&&!roll_actions&&roll.layer==PR_PADS)back_label=pattern_host?"Arrange":"Patterns";
#endif
 colour(g,WHITE);text(g,back_label,0,6,105,23,14);
 const char*range_label=shift?(roll.follow?"Follow On":"Follow Off"):roll.range?"End range":"Select range";
#ifdef MPC_PATTERN_TOOLS_TRIAL
 if(pattern_host&&!shift&&!roll_editing&&!roll_actions)range_label=pattern_rows_held?"Pads: rows":"Range / rows";
#endif
 colour(g,shift&&roll.follow?AMBER:WHITE);text(g,range_label,107,6,105,23,14);
 const char*primary=roll_row_pending()?"Do It!":shift?"Actions":roll_actions?"Choose":roll_editing?"Apply":roll.layer==PR_PADS&&!pr_selected(&roll)?"Open":"Edit";
#ifdef MPC_PATTERN_TOOLS_TRIAL
 if(pattern_host&&!roll_row_pending()&&!shift&&!roll_actions&&!roll_editing&&!pr_selected(&roll)&&roll_empty_cell())primary="New";
 if(pattern_host_clip&&pattern_rows_held&&!shift&&!roll_actions&&!roll_editing&&!roll_row_pending())primary="Separate";
#endif
 colour(g,AMBER);text(g,primary,214,6,105,23,14);
 char title[80],pad[8];pad_name(roll.sound,pad);
 uint64_t sequence_muted[2];int has_seq_mutes=roll_sequence_mutes(sequence_muted);
#ifdef MPC_PATTERN_TOOLS_TRIAL
 if(roll_pattern_index>=0){sequence_muted[0]=sequence_muted[1]=0;has_seq_mutes=0;}
#endif
 /* Title: pad + its sample name (as in Sample/Steps), grid and selection on the right.
  * Row mutes are shown by the red "M" on the row itself, no extra SM badge here. */
 {int title_pad=roll.layer==PR_PADS?(roll.cursor_row>=0&&roll.cursor_row<roll.row_count?roll.rows[roll.cursor_row]:-1):roll.sound;
  char caption[64],sample[513];
#ifdef MPC_PATTERN_TOOLS_TRIAL
  if(roll_pattern_index>=0){struct pt_entry e;pt_info((unsigned)roll_pattern_index,&e);if(pattern_host&&roll.layer==PR_PADS)snprintf(pad,sizeof pad,"P:%c",'A'+browse_bank);else strcpy(pad,"PAT");snprintf(sample,sizeof sample,"%s",e.name);}
  else
#endif
  if(title_pad>=0&&title_pad<128){pad_name(title_pad,pad);read_sample_for_pad(title_pad,sample);}
  else{strcpy(pad,"--");strcpy(sample,"Empty");}
  (void)has_seq_mutes;
  int pad_colour=title_pad>=0&&title_pad<128?music_pad_colour((unsigned)title_pad):0;
  colour(g,pad_colour?0xff000000u|music_pad_rgb((unsigned)title_pad):AMBER);left_text(g,pad,6,34,40,20,15);
  ui_caption(caption,sizeof(caption),sample,18);colour(g,!strcmp(sample,"Empty")?GREY:WHITE);left_text(g,caption,46,34,150,20,14);
  /* Right: which sequence is edited (grid is on K2). Patterns show their name on the left already. */
  {unsigned nsel=pr_selected(&roll);char cnt[24],seq[48]="";snprintf(cnt,sizeof(cnt),nsel?"  %u sel":"",nsel);
   int seq_index=-1;{uintptr_t w;if(sequence&&!((uintptr_t)sequence&7)&&safe_word((uintptr_t)sequence+0x480,&w))seq_index=(int)(uint32_t)w;}
#ifdef MPC_PATTERN_TOOLS_TRIAL
   if(roll_pattern_index>=0)seq_index=-1;
   if(roll_pattern_index>=0&&roll_song_valid){int64_t a=roll_song_start,b=roll_song_start+roll_song_length;
    snprintf(seq,sizeof(seq),"%d.%d-%d.%d",(int)(a/3840+1),(int)(a%3840/960+1),(int)(b/3840+1),(int)(b%3840/960+1));}
#endif
   if(seq_index>=0&&seq_index<128)snprintf(seq,sizeof(seq),"Seq %02d",seq_index+1);
   snprintf(title,sizeof(title),"%s%s%s",roll.layer==PR_PADS?"":roll.layer==PR_TUNE?"Tune ":"Chop ",seq[0]?seq:roll_grid_name(),cnt);}
  colour(g,WHITE);right_text(g,title,196,34,118,20,12);}
 if(roll_actions){int count=roll_actions_count(),first=ui_list_first(roll_action,count,6);
  for(int i=first;i<count&&i<first+6;++i){int y=61+(i-first)*22;if(i==roll_action){colour(g,0xff303030);rect(g,6,y,302,22);}
   colour(g,i==roll_action?AMBER:WHITE);left_text(g,roll_action_names[i],12,y,292,22,14);
  }
  int offset,thumb=ui_list_thumb(first,count,6,132,&offset);
  colour(g,0xff303030);rect(g,314,61,2,132);colour(g,AMBER);rect(g,314,61+offset,2,thumb);
  char position[40];snprintf(position,sizeof position,"Action %d / %d",roll_action+1,count);colour(g,GREY);left_text(g,position,6,200,308,16,11);
  colour(g,AMBER);left_text(g,"DATA / Fader: choose | B3: apply",6,218,308,17,11);return;
 }
 int64_t unit=roll.grid==10?60:pr_grid_ticks(roll.grid);while(roll.span/unit>48)unit*=2;
 for(int64_t t=(roll.left/unit)*unit;t<=roll.left+roll.span;t+=unit){int x=roll_x(t);if(x>=70&&x<310){
  colour(g,0xff292929);rect(g,x,70,1,120);
 }}
 /* Musical beat/bar accents stay anchored to sequence time while scrolling. */
 int64_t beat=960;while(roll.span/beat>48)beat*=2;int last_label=-100;
 for(int64_t t=(roll.left/beat)*beat;t<=roll.left+roll.span;t+=beat){int x=roll_x(t);if(x<70||x>=310)continue;
  struct bbt pos;
#ifdef MPC_PATTERN_TOOLS_TRIAL
  if(roll_pattern_index>=0){int64_t s=roll_song_time(t);if(s<0)s=0;pos.bar=(int)(s/3840);pos.beat=(int)(s%3840/960);pos.tick=(int)(s%960);}else
#endif
  pos=position(t);
  colour(g,pos.beat==0&&!pos.tick?0xffa0a0a0:0xff505050);rect(g,x,70,1,120);
  if(!pos.tick&&x-last_label>=45&&x<285){char stamp[24];snprintf(stamp,sizeof(stamp),"%d.%d",pos.bar+1,pos.beat+1);colour(g,WHITE);left_text(g,stamp,x+2,57,42,12,10);last_label=x;}
 }
 {   /* Cursor position (bar.beat.tick) above the row names, same musical counting as the ruler. */
  struct bbt cur;int64_t t=roll.cursor_tick<0?0:roll.cursor_tick;
#ifdef MPC_PATTERN_TOOLS_TRIAL
  if(roll_pattern_index>=0){int64_t s=roll_song_time(t);if(s<0)s=0;cur.bar=(int)(s/3840);cur.beat=(int)(s%3840/960);cur.tick=(int)(s%960);}else
#endif
  cur=position(t);
  char stamp[48];snprintf(stamp,sizeof(stamp),"%d.%d.%03d",cur.bar+1,cur.beat+1,cur.tick);
  colour(g,0xff000000);rect(g,0,55,69,14);colour(g,AMBER);left_text(g,stamp,3,57,66,12,10);
 }
 for(int row=roll.top;row<roll.row_count&&row<roll.top+roll.rows_visible;++row){int y=roll_y(row);if(y<70)continue;
  colour(g,0xff303030);rect(g,0,y,310,1);char label[64];int value=roll.rows[row];
  if(roll.layer==PR_PADS){unsigned flags=roll_categories(value);pad_name(value,pad);
   /* Pad + the start of its sample name (cached: the program is read at most once a second per pad). */
   static char sample_cache[128][12];static uint64_t sample_at[128];const char*sn="";
   if(value>=0&&value<128){uint64_t now=milliseconds();if(!sample_at[value]||now-sample_at[value]>1000){char full[513];strcpy(full,"Empty");
     /* Pattern host has no own native screen: read the program only through readable, aligned pointers. */
     uintptr_t things=0,program=0;
     if(controller&&!((uintptr_t)controller&7)&&safe_word((uintptr_t)controller+0x248,&things)&&things&&!(things&7)&&safe_word(things+0xb8,&program)&&program&&!(program&7)&&safe_word(program,&program))read_sample_for_pad(value,full);
     snprintf(sample_cache[value],sizeof sample_cache[value],"%.11s",strcmp(full,"Empty")?full:"");sample_at[value]=now?now:1;}sn=sample_cache[value];}
   snprintf(label,sizeof(label),"%s%s%.7s%s%s",pad,sn[0]?" ":"",sn,flags&2?" L":"",flags&1?" C":"");}
  else if(roll.layer==PR_TUNE){int pc=(roll_reference_note+value-120)%12;if(pc<0)pc+=12;snprintf(label,sizeof(label),"%s %+d",music_note_names[pc],value-120);}
  else snprintf(label,sizeof(label),"C%03d",value+1);
  int muted_pad=roll.layer==PR_PADS?value:roll.sound;
  int muted=muted_pad>=0&&muted_pad<128&&((sequence_muted[muted_pad/64]>>(muted_pad%64))&1u);
  colour(g,row==roll.cursor_row?AMBER:WHITE);left_text(g,label,3,y+1,muted?50:65,roll.row_height-2,11);
  if(muted){colour(g,0xffff4747u);text(g,"M",56,y+1,11,roll.row_height-2,11);}
 }
 for(unsigned i=0;i<roll.count;++i){const struct pr_note*n=roll.notes+i;int row=pr_row_index(&roll,pr_note_row(&roll,n));
  if(row<roll.top||row>=roll.top+roll.rows_visible)continue;
  int64_t d=FIELD(&n->event,0x28,int64_t);if(d<=0)d=1;
  if(n->event.time+d<=roll.left||n->event.time>=roll.left+roll.span)continue;
  int x=roll_x(n->event.time),right=roll_x(n->event.time+d),y=roll_y(row)+3;
  uint32_t assigned=music_pad_rgb((unsigned)n->sound);
  uint32_t ink=music_piano_roll_colour()?0xff000000u|assigned:n->selected?AMBER:WHITE;
  if(n->event.type&0x200)ink=0xff666666;
  roll_rect(g,x,y,right-x>2?right-x:2,roll.row_height-5,ink);
  if(n->selected){roll_rect(g,x,y-1,right-x>2?right-x:2,1,AMBER);roll_rect(g,x,y+roll.row_height-5,right-x>2?right-x:2,1,AMBER);roll_rect(g,x,y-1,1,roll.row_height-3,AMBER);roll_rect(g,right-1,y-1,1,roll.row_height-3,AMBER);}
  if(roll.note_focus&&roll.focused==(int)i){roll_rect(g,x,y-2,right-x>2?right-x:2,1,WHITE);roll_rect(g,x,y+roll.row_height-4,right-x>2?right-x:2,1,WHITE);}
 }
 for(unsigned i=0;i<roll.plan.added_count&&roll_editing;++i){const struct step_event*e=roll.plan.added+i;
  int row=-1;
  int event_sound;
#ifdef MPC_PATTERN_TOOLS_TRIAL
  if(roll_pattern_index>=0)event_sound=pt_pad_number((unsigned)roll_pattern_index,e->data[32]);else
#endif
  event_sound=event_pad(e);
  if(roll.layer==PR_PADS)row=pr_row_index(&roll,event_sound);
  else if(event_sound==roll.sound){int value=roll.layer==PR_TUNE?event_choice_value(e,0)+120:event_choice_value(e,15)-1;row=pr_row_index(&roll,value);}
  if(row<roll.top||row>=roll.top+roll.rows_visible)continue;
  int x=roll_x(e->time),right=roll_x(e->time+FIELD(e,0x28,int64_t)),y=roll_y(row)+2;
  roll_rect(g,x,y,right-x>2?right-x:2,2,AMBER);roll_rect(g,x,y+roll.row_height-5,right-x>2?right-x:2,2,AMBER);
 }
 int x=roll_x(roll.cursor_tick);roll_rect(g,x,70,1,120,AMBER);
 if(roll_show_transport()){x=roll_x(roll_transport_tick());roll_rect(g,x,70,1,120,WHITE);}
 if(roll.range){int xa=roll_x(roll.anchor_tick),xb=roll_x(roll.cursor_tick);if(xa>xb){int t=xa;xa=xb;xb=t;}
  int ya=roll_y(roll.anchor_row),yb=roll_y(roll.cursor_row);if(ya>yb){int t=ya;ya=yb;yb=t;}
  roll_rect(g,xa,ya,xb-xa+1,1,AMBER);roll_rect(g,xa,yb+roll.row_height-1,xb-xa+1,1,AMBER);
  roll_rect(g,xa,ya,1,yb-ya+roll.row_height,AMBER);roll_rect(g,xb,ya,1,yb-ya+roll.row_height,AMBER);
 }
 /* Separate names from the moving timeline; mark the real sequence extent. */
 colour(g,WHITE);rect(g,68,70,1,120);
 int begin=roll_x(0),end=roll_x(roll.length);
 colour(g,AMBER);rect(g,clamp(begin,70,309),193,clamp(end,70,310)-clamp(begin,70,309),1);
 if(begin>=70&&begin<=310)rect(g,clamp(begin,70,309),189,1,5);
 if(end>=70&&end<=310)rect(g,clamp(end,70,309),189,1,5);
 colour(g,0xff555555);rect(g,70,196,240,1);rect(g,314,70,2,120);
 int width=(int)(roll.span*240/roll.length);width=clamp(width,3,240);
 colour(g,WHITE);rect(g,70+clamp((int)(roll.left*240/roll.length),0,240-width),196,width,1);
 int thumb=clamp(120*roll.rows_visible/roll.row_count,3,120);rect(g,314,70+120-thumb-(roll.top*(120-thumb)/(roll.row_count>1?roll.row_count-1:1)),2,thumb);
 const char *labels[3]={roll_editing?(shift?"Fine move":"Move"):shift?"Zoom rows":"Scroll rows",roll_editing?(shift?"Velocity":"Pad / Pitch"):"Grid",roll_editing?(shift?"Fine length":"Length"):shift?"Zoom time":"Scroll time"};
 for(int k=0;k<3;++k){colour(g,0xff242424);rect(g,k*106,209,105,31);colour(g,WHITE);
  char value[40];value[0]=0;
  if(k==1&&!roll_editing)snprintf(value,sizeof(value),"%s",roll_grid_name());
  else if(!roll_editing&&k==0){if(shift)snprintf(value,sizeof(value),"%d rows",roll.rows_visible);else snprintf(value,sizeof(value),"%d / %d",roll.top+1,roll.row_count);}
  else if(!roll_editing&&k==2){if(shift){int64_t b=roll.span/960;if(b>=4&&b%4==0)snprintf(value,sizeof(value),"%lld bars",(long long)(b/4));else snprintf(value,sizeof(value),"%lld beats",(long long)(b>0?b:1));}
   else snprintf(value,sizeof(value),"%lld.%lld",(long long)(roll.left/3840+1),(long long)(roll.left%3840/960+1));}
  else if(roll_editing){int64_t t=k==0?roll_delta_time:k==2?roll_delta_length:0;
   if(k==1)snprintf(value,sizeof(value),shift?"%+d":"%+d rows",shift?roll_delta_velocity:roll_delta_row);
   else if(t%960==0)snprintf(value,sizeof(value),"%+lld beat",(long long)(t/960));else snprintf(value,sizeof(value),"%+lld tick",(long long)t);}
  if(value[0]){text(g,labels[k],k*106,210,105,11,10);colour(g,AMBER);text(g,value,k*106,222,105,16,13);}
  else text(g,labels[k],k*106,212,105,20,12);}
 const char*footer=roll.range?"Range: DATA = time, Fader = rows":message[0]?message:roll.error[0]?roll.error:roll_editing?"DATA: Move | Fader: Rows | Shift: Zoom":roll.note_focus?"MUTE: Row | DATA: Notes | Click: Select":roll.follow?"MUTE: Row | Follow On | Shift B3: Actions":"MUTE: Row | B2: Range | Shift B3: Actions";
#ifdef MPC_PATTERN_TOOLS_TRIAL
 if(roll_pattern_index>=0&&!message[0]&&!roll.error[0]&&!roll.range)footer=pattern_host?"Pattern edit | Shift B3: Actions | B1: Arrange":"Pattern edit | Play: sequence | Shift B3: Actions";
#endif
 colour(g,AMBER);left_text(g,footer,4,199,308,9,9);
}
#ifdef MPC_MIDI_TOOLS
#include "piano_roll_midi.inc"
#endif
#ifdef MPC_PATTERN_TOOLS_TRIAL
#include "pattern_host.inc"
#endif
