#define main legacy_steps_main
#include "check_steps.c"
#undef main
#include "src/usb_drive_lights.h"
#include "src/seq_mutes.h"
static void*roll_instrument(void *p,int pad){assert(p==program);return instruments[pad];}
static unsigned rendered_events;static int a01_y=-1,a02_y=-1,h16_y=-1;
static int names_separator,playback_end,shift_actions,shift_follow,bar_x;
static unsigned row_mute_marks,roll_sm_marks,grid_footer;
static void roll_draw_rect(void*g,int x,int y,int w,int h){
 (void)g;assert(x>=0&&y>=0&&w>=0&&h>=0&&x+w<=320&&y+h<=240);
 if(x>=70&&x<310&&y>=57&&y<190&&h>2)++rendered_events;
 if(x==68&&y==70&&w==1&&h==120&&draw_ink==WHITE)names_separator=1;
 if(x==309&&y==189&&w==1&&h==5&&draw_ink==AMBER)playback_end=1;
 if(y==70&&w==1&&h==120&&(draw_ink==0xffa0a0a0u||draw_ink==0xff505050u)&&x>70&&bar_x<0)bar_x=x;
 fprintf(draw_file,"R %d %d %d %d %08x\n",x,y,w,h,draw_ink);
}
static void roll_draw_text(void*g,const uint64_t*s,int x,int y,int w,int h,int flags,int lines,float scale){
 const char*t=(const char*)(uintptr_t)*s;
 if(!strcmp(t,"M")){assert(x==56&&draw_ink==0xffff4747u);++row_mute_marks;}
 if(!strcmp(t,"SM")){assert(x==284&&draw_ink==0xffff4747u);++roll_sm_marks;}
 if(x==106&&y==222){assert(!strcmp(t,roll_grid_name())&&draw_ink==AMBER);++grid_footer;}
 if(!strncmp(t,"A01",3))a01_y=y;
 if(!strncmp(t,"A02",3))a02_y=y;
 if(!strncmp(t,"H16",3))h16_y=y;
 if(!strcmp(t,"Actions"))shift_actions=1;
 if(!strcmp(t,"Follow On"))shift_follow=1;
 draw_text(g,s,x,y,w,h,flags,lines,scale);
}
int main(int argc,char**argv){
 assert(argc==3);struct sigaction sa={.sa_sigaction=fault,.sa_flags=SA_SIGINFO};sigaction(SIGSEGV,&sa,NULL);sigaction(SIGILL,&sa,NULL);sigaction(SIGABRT,&sa,NULL);
 load_elf(argv[1],0x1800000000ULL);setup();phase="piano roll real native event tree and modifiers";
 unsigned char amber[192],native_rgb[12];usb_drive_amber_frame(amber);
 for(unsigned pad=0;pad<16;++pad){FN(0x23b0070,void(*)(void*,unsigned,unsigned,unsigned))(native_rgb,0x5c,0xffc247,pad);assert(!memcmp(native_rgb,amber+pad*12,12));}
 stub(0x286bbe0,roll_instrument);instruments[0]=instruments[1]=instruments[127]=instrument;
 struct step_event a=make_note(0,119,.7f),b=make_note(1,480,.8f);event_choice_set(&b,0,7);event_choice_set(&b,15,3);
 b.data[34]=37;b.data[36]=2;insert_note(&a);insert_note(&b);insert_note(&b);step_native_tick();
 button(0x00,1);button(0x00,0);assert(atomic_load(&piano_active)&&roll.count==3&&roll.row_count==3);
 assert(roll.rows[0]==0&&roll.rows[1]==1&&roll.rows[2]==127);
 pr_select_all(&roll);int before=group_mute_calls;
 assert(pr_edit(&roll,120,0,120,0)&&pr_apply(&roll));assert(group_mute_calls==before+1);
 assert(roll_refresh(1)&&roll.count==3&&pr_selected(&roll)==3);
 for(unsigned i=0;i<roll.count;++i){assert(FIELD(&roll.notes[i].event,0x28,int64_t)==360);if(roll.notes[i].sound==1){assert(roll.notes[i].tune==7&&roll.notes[i].slice==3);assert(roll.notes[i].event.data[34]==37&&roll.notes[i].event.data[36]==2);}}
 /* Both history shortcuts reach stock; the roll does not implement a second history. */
 assert(roll_button(BUTTON_MINUS,1)==2&&roll_button(BUTTON_PLUS,1)==2);
 uint64_t record_ticket=atomic_load(&ticket);atomic_store(&ticket,record_ticket|T_STEPS|T_PIANO);unsigned record_light=99;
 assert(!step_native_record_light(&record_light));atomic_store(&ticket,record_ticket);
 /* Exercise the complete ownership path: repeated native downs require ups. */
 for(unsigned repeat=0;repeat<3;++repeat)for(unsigned c=0;c<5;++c){unsigned code=(unsigned[]){BUTTON_PLUS,BUTTON_MINUS,BUTTON_STOP,BUTTON_PLAY,BUTTON_RECORD}[c];
  unsigned char event[16]={0};event[8]=(unsigned char)code;event[9]=1;
  uint64_t t=step_native_ticket()|T_SHIFT;
  assert(step_native_button_at(event,t,milliseconds())==2);
  assert(step_native_button_owns(code)&&atomic_load(&piano_forwarded_buttons[code]));
  event[9]=0;assert(step_native_button_at(event,0,milliseconds())==2);
  assert(!atomic_load(&piano_forwarded_buttons[code]));
 }
 roll_action=14;roll_action_run();assert(roll_editing&&roll.layer==PR_PADS);
 assert(roll_button(0x0c,0)==1&&!roll_editing); /* Pad-level length needs neither Orig Note nor chops. */
 assert(roll_button(0x0e,0)==1&&roll_editing); /* Selection exposes Edit directly on B3. */
 roll_button(0x0c,0);roll_button(0x0c,1);assert(roll.note_focus);
 roll_control(0x64,1,0);assert(roll.focused>=0);roll_button(0x0c,1);assert(!roll.note_focus);
 assert(!roll_layer_open(PR_TUNE,1)&&roll.layer==PR_PADS);
 assert(!roll_layer_open(PR_CHOP,1)&&roll.layer==PR_PADS); /* C flag alone is insufficient. */
 FIELD(instrument,0x22e8,void*)=layer;FIELD(instrument,0x22f0,void*)=layer+sizeof(layer);
 FIELD(layer,0x4a8,void*)=sample_data;FIELD(layer,0x4b0,void*)=sample_refs;FIELD(sample_refs,8,int)=1;
 FN(0xf535c0,void(*)(uint64_t*,const char*))((uint64_t*)(sample_data+0xa8),"Roll keys");
 assert(music_set_sample("Roll keys",MUSIC_ROOT,0)&&roll_layer_open(PR_TUNE,1));
 assert(music_set_sample("Roll keys",MUSIC_MODE,MUSIC_PIANO));
 assert(music_set_sample("Roll keys",MUSIC_TRANSPOSE,2));
 step_native_levels_state(1,1,3);roll_layout_update();
 assert(roll_pad_rows[0]==119&&roll_pad_rows[15]==134&&roll.row_count==22);
 pr_select_all(&roll);roll_edit_start();assert(roll_editing);
 assert(roll_control(0x14,127,0)&&roll.plan.added_count);int delta=roll_delta_row;
 struct step_event fader_plan=roll.plan.added[0];
 assert(roll_control(0x14,127,0)&&roll_delta_row==delta&&!memcmp(&fader_plan,roll.plan.added,sizeof(fader_plan)));
 int moved=event_choice_value(roll.plan.added,0)+120,visible_row=pr_row_index(&roll,moved);
 assert(visible_row>=roll.top&&visible_row<roll.top+roll.rows_visible);
 int64_t edit_span=roll.span;assert(roll_control(0x64,1,1)&&roll_editing&&roll.span<=edit_span);
 assert(roll_control(0x14,127,1)&&roll.row_height==38&&roll.plan.added_count);
 roll_button(0x0c,0);assert(!roll_editing);for(unsigned i=0;i<roll.count;++i)roll.notes[i].selected=0;
 unsigned auditions_before=audition_starts;roll_pad(0);
 assert(audition_starts==auditions_before+1&&event_choice_value(&audition_event,0)==-1&&event_pad(&audition_event)==1);
 step_native_pad_release(0,step_native_ticket());assert(roll_audition==-1);
 roll_palette();uint64_t actual;assert(step_native_light(8,&actual));
 int replace=0;uint64_t expected=music_pitch_light(roll_tune_lights,roll_pad_rows[8]-120,&replace);
 uint32_t bright_bits=(uint32_t)(expected>>32);float bright;memcpy(&bright,&bright_bits,4);
 assert(replace&&actual==pad_light((uint32_t)expected,bright));
 assert(music_set_sample("Roll keys",MUSIC_MODE,MUSIC_DEGREES));roll_layout_update();
 for(unsigned p=0;p<16;++p){int pitch=0;assert(music_layout_pad_pitch(roll_tune_lights,p,3,&pitch)&&roll_pad_rows[p]==pitch+120);}
 assert(music_set_sample("Roll keys",MUSIC_MODE,MUSIC_MONO)&&music_set_sample("Roll keys",MUSIC_TRANSPOSE,0));
 step_native_levels_state(0,0,0);roll_layout_update();
 assert(!roll_layer_open(PR_CHOP,1)&&roll.layer==PR_TUNE);
 FIELD(sample_data,0x130,void*)=slice_data;FIELD(slice_data,0x70,void*)=slice_objects;FIELD(slice_data,0x78,void*)=slice_objects+8;
 assert(music_set_sample("Roll keys",MUSIC_ROOT,-1));pr_view_layer(&roll,PR_PADS,1);
 assert(!roll_layer_open(PR_TUNE,1)&&roll_layer_open(PR_CHOP,1)&&roll.row_count==8);
 assert(music_set_sample("Roll keys",MUSIC_ROOT,0)&&roll_layer_open(PR_TUNE,1));
 pr_select_all(&roll);assert(pr_edit(&roll,0,2,0,0)&&pr_apply(&roll));assert(roll_refresh(1));
 for(unsigned i=0;i<roll.count;++i)if(roll.notes[i].sound==1)assert(roll.notes[i].tune==9&&roll.notes[i].slice==3);
 assert(roll_layer_open(PR_CHOP,1));pr_select_all(&roll);assert(pr_edit(&roll,0,1,0,0)&&pr_apply(&roll));assert(roll_refresh(1));
 roll_palette();assert(step_native_light(8,&actual)&&!actual);
 roll_pad(3);assert(event_choice_value(&audition_event,15)==4&&event_pad(&audition_event)==1);roll_pad_up(3);
 for(unsigned i=0;i<roll.count;++i)if(roll.notes[i].sound==1)assert(roll.notes[i].tune==9&&roll.notes[i].slice==4);
 pr_view_layer(&roll,PR_PADS,0);pr_select_all(&roll);assert(pr_quantize(&roll,PR_SNAP_START));assert(roll_refresh(1));
 assert(pr_copy(&roll));pr_cursor(&roll,1920,0);assert(pr_paste(&roll));assert(roll_refresh(1)&&roll.count==6);
 assert(pr_delete(&roll,1));assert(roll_refresh(1)&&roll.count==3);
 draw_file=fopen(argv[2],"w");assert(draw_file);stub(0x102d5e0,draw_colour);stub(0x102e000,roll_draw_rect);
 stub(0x1082810,draw_font_make);stub(0x102dca0,draw_font_use);stub(0x1070d60,draw_font_end);stub(0x10817b0,roll_draw_text);
 assert(!music_piano_roll_colour());pr_cursor(&roll,480,0);roll_paint((void*)1);fclose(draw_file);draw_file=NULL;
 assert(rendered_events&&a01_y>a02_y&&a02_y>h16_y&&h16_y>=57);
 draw_file=fopen("/dev/null","w");assert(draw_file);
 /* Existing sequence state is visible; Mute changes only the cursor's pad. */
 FIELD(seq,0x480,int)=3;assert(music_sequence_pad_toggle(3,1)==1&&music_sequence_pad_toggle(9,0)==1);
 row_mute_marks=roll_sm_marks=0;roll_paint((void*)1);assert(row_mute_marks==1&&roll_sm_marks==1);
 unsigned preserved_events=(unsigned)tree()->size;int native_edits=group_mute_calls;
 button(BUTTON_MUTE,1);button(BUTTON_MUTE,0);
 assert(music_sequence_pad_muted(3,0)&&music_sequence_pad_muted(3,1)&&music_sequence_pad_muted(9,0));
 assert(seq_mutes_current()==3&&seq_mutes_event_muted(&roll.notes[0].event));
 row_mute_marks=roll_sm_marks=0;roll_paint((void*)1);assert(row_mute_marks==2&&roll_sm_marks==1);
 fclose(draw_file);char muted_trace[1024];snprintf(muted_trace,sizeof(muted_trace),"%s.muted.txt",argv[2]);
 draw_file=fopen(muted_trace,"w");assert(draw_file);roll_paint((void*)1);fclose(draw_file);
 draw_file=fopen("/dev/null","w");assert(draw_file);
 button(BUTTON_MUTE,1);button(BUTTON_MUTE,0);assert(!music_sequence_pad_muted(3,0));
 FIELD(seq,0x480,int)=9;row_mute_marks=roll_sm_marks=0;roll_paint((void*)1);assert(row_mute_marks==1&&roll_sm_marks==1);
 FIELD(seq,0x480,int)=3;assert(music_sequence_mutes_clear(3));
 row_mute_marks=roll_sm_marks=0;roll_paint((void*)1);assert(!row_mute_marks&&!roll_sm_marks);
 assert(tree()->size==preserved_events&&group_mute_calls==native_edits);
 /* Tune and Chop rows share the source pad's sequence mute, not note mutes. */
 for(int mode=PR_TUNE;mode<=PR_CHOP;++mode){
  assert(roll_layer_open(mode,1));button(BUTTON_MUTE,1);button(BUTTON_MUTE,0);
  assert(music_sequence_pad_muted(3,1)&&!music_sequence_pad_muted(3,0));
  row_mute_marks=roll_sm_marks=0;roll_paint((void*)1);assert(row_mute_marks>0&&roll_sm_marks==1);
  button(BUTTON_MUTE,1);button(BUTTON_MUTE,0);assert(!music_sequence_pad_muted(3,1));
 }
 pr_view_layer(&roll,PR_PADS,0);pr_cursor(&roll,480,0);
 /* Every K2 grid value is drawn at the knob and leaves zoom/history alone. */
 for(unsigned value=0;value<11;++value){
  int64_t span=roll.span;assert(roll_control(0x11,(value*128+10)/11,0)&&roll.grid==(int)value);
  grid_footer=0;roll_paint((void*)1);assert(grid_footer==1&&roll.span==span&&group_mute_calls==native_edits);
 }
 FIELD(seq,0x480,int)=128;button(BUTTON_MUTE,1);button(BUTTON_MUTE,0);assert(!music_sequence_pad_muted(3,0));
 FIELD(seq,0x480,int)=3;
 shift=1;roll.follow=1;pr_zoom(&roll,1,9);roll_paint((void*)1);
 assert(names_separator&&playback_end&&shift_actions&&shift_follow);
 shift=0;roll.follow=0;pr_zoom(&roll,1,2);roll.left=0;bar_x=-1;roll_paint((void*)1);int before_bar=bar_x;
 roll.left=240;bar_x=-1;roll_paint((void*)1);assert(before_bar>=0&&bar_x<before_bar);
 pr_select_all(&roll);roll_edit_start();assert(pr_edit(&roll,10,0,0,0));struct step_event planned=roll.plan.added[0];
 int64_t preview_delta=roll_delta_length;assert(roll_control(0x64,1,1)&&roll_control(0x14,127,1));
 assert(roll_editing&&roll.plan.added_count&&!memcmp(&planned,roll.plan.added,sizeof(planned))&&roll_delta_length==preview_delta);
 roll_button(0x0c,0);assert(!roll_editing);roll_button(0x0d,1);assert(roll.follow&&!roll.range);roll_button(0x0d,1);assert(!roll.follow);
 fclose(draw_file);draw_file=NULL;
 /* Sample-style Shift zoom stays on the same axis; fader and DATA span a range. */
 pr_zoom(&roll,0,19);
 pr_zoom(&roll,1,3);int calls_before=group_mute_calls;int64_t old_span=roll.span;int old_height=roll.row_height;
 assert(roll_control(0x64,1,1)&&roll.span!=old_span&&roll.row_height==old_height);
 old_span=roll.span;assert(roll_control(0x14,127,1)&&roll.row_height!=old_height&&roll.span==old_span);
 assert(roll_control(0x11,127,0)&&roll.grid==10);
 pr_cursor(&roll,0,0);pr_range(&roll);assert(roll.range);
 assert(roll_control(0x14,127,0)&&roll.cursor_row==roll.row_count-1&&roll.anchor_row==0);
 assert(roll_control(0x64,1,0)&&roll.cursor_tick==1&&roll.anchor_tick==0);
 pr_range(&roll);assert(!roll.range&&group_mute_calls==calls_before);
 button(0x0c,1);button(0x0c,0);assert(!atomic_load(&piano_active)&&!steps);
 steps=want_steps=1;step_native_tick();button(0x00,1);button(0x00,0);assert(atomic_load(&piano_active));
 assert(roll.clip_count>0); /* Same-project sequence/view transitions retain copied notes. */
 button(0x0c,1);button(0x0c,0);assert(!atomic_load(&piano_active)&&steps);
 step_native_project_changed();step_native_tick();assert(!atomic_load(&piano_active)&&!roll.clip_count);
 char preferences[]="/tmp/am-roll-preferences-XXXXXX";assert(mkdtemp(preferences)&&music_lighting_preferences_open(preferences));
 assert(!music_piano_roll_colour()&&!music_set_piano_roll_colour(2));
 assert(music_set_piano_roll_colour(1)&&music_lighting_preferences_open(preferences)&&music_piano_roll_colour());
 assert(music_set_piano_roll_colour(0)&&music_lighting_preferences_open(preferences)&&!music_piano_roll_colour());
 puts("PASS Piano Roll ARM native adapter: actual event containers/factory and packed Tune/Chop, one command per group, duplicate preservation, cut/paste, stock Undo+Redo forwarding, Events+Steps entry/return, bottom-up monochrome rendering and project lifecycle (executor/GUI fixtures)");
 return 0;
}
