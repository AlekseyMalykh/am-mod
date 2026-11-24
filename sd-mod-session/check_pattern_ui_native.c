#define PIANO_ROLL_PROOF_MAIN pattern_legacy_roll_main
#include "check_piano_roll_native.c"
#undef PIANO_ROLL_PROOF_MAIN
int mixer_phone_pad_output(unsigned pad){return pad<16?1:2;}
static char*saved_patterns;static size_t saved_size;
static _Alignas(8) char saved_wrapper[4][16],saved_archive[0x158],saved_frame[0x38],saved_members[48];
static void patterns_write(void*p,const char*key,const void*text){(void)p;assert(!strcmp(key,"amPatternsV1"));
 const char*bytes=FIELD(text,0,const char*);saved_size=FIELD(text,8,size_t);free(saved_patterns);saved_patterns=malloc(saved_size+1);assert(saved_patterns);memcpy(saved_patterns,bytes,saved_size);saved_patterns[saved_size]=0;
}
static void patterns_original(void*p,void*a){(void)p;(void)a;}
static int patterns_read(void*p,void*a){(void)a;pt_archive_load(p,saved_wrapper,patterns_original);return 1;}
static unsigned host_redraws;
static uint64_t separated_clip;
static void host_separated(uint64_t id){assert(!pattern_host&&!atomic_load(&piano_active));separated_clip=id;++host_redraws;}
extern int check_arrangement_native_choices(uintptr_t);
static void host_redraw(void){++host_redraws;}
static void host_press(unsigned code,int shifted){assert(step_pattern_button(code,1,shifted));assert(step_pattern_button(code,0,shifted));}
static void host_action(int action){host_press(0x0e,1);int delta=action-roll_action;assert(step_pattern_control(0x64,(unsigned)(delta<0?128+delta:delta),0));host_press(0x0e,0);}
int main(int argc,char**argv){
 assert(pattern_legacy_roll_main(argc,argv)==0);pt_init(base);assert(check_arrangement_native_choices(base));
 FN(0xf535c0,void(*)(void*,const char*))(program+0xdc8,"pattern-ui-program");
 step_native_tick();roll_open();assert(atomic_load(&piano_active));
 unsigned count=roll.count;assert(count);struct step_event source[PR_MAX_EVENTS];for(unsigned i=0;i<count;++i)source[i]=roll.notes[i].event;
 draw_file=fopen(argv[2],"w");assert(draw_file);
 roll_action=18;roll_action_run();assert(roll_patterns_state==1);
 roll_patterns_button(0x0d,0);assert(roll_patterns_state==2);roll_patterns_action=0;roll_patterns_button(0x0e,0);
 assert(pt_count()>0&&roll_patterns_state==1&&roll.count==count);roll_patterns_paint((void*)1);
 roll_patterns_button(0x0d,0);assert(roll_patterns_state==2);
 roll_patterns_control(0x14,127,0);assert(roll_patterns_action==0);roll_patterns_control(0x14,0,0);assert(roll_patterns_action==7);
 roll_patterns_control(0x10,0,0);assert(roll_patterns_action==0);roll_patterns_button(0x0d,0);
 roll_patterns_control(0x14,127,0);assert(roll_patterns_pick==0);roll_patterns_control(0x14,0,0);assert(roll_patterns_pick==(int)pt_count()-1);
 unsigned original_count=pt_count();roll_patterns_button(0x0d,0);roll_patterns_action=5;roll_patterns_button(0x0e,0);
 assert(pt_count()==original_count+1);unsigned variation=(unsigned)roll_patterns_pick;
 roll_patterns_button(0x0e,0);assert(roll_pattern_index==(int)variation&&!roll_patterns_state);pr_select_all(&roll);
 assert(pr_edit(&roll,0,0,120,-5)&&pr_apply(&roll));assert(roll_refresh(1));
 assert(roll_layer_open(PR_TUNE,roll.notes[0].sound));pr_select_all(&roll);int previous_tune=roll.notes[0].tune;
 assert(roll_control(0x14,0,0));int bottom_row=roll.cursor_row;assert(roll_control(0x14,127,0)&&roll.cursor_row>=bottom_row); // Pitch increases upward on screen.
 assert(pr_edit(&roll,0,1,0,0)&&pr_apply(&roll)&&roll_refresh(1));assert(roll.notes[0].tune==previous_tune+1);
 assert(roll_layer_open(PR_PADS,roll.notes[0].sound));
 assert(tree()->size==count);unsigned ordinal=0;for(void*n=tree()->left;n!=&tree()->colour;n=FN(0xb51738,void*(*)(void*))(n))assert(!memcmp((char*)n+0x20,source+ordinal++,sizeof(struct step_event)));
 roll_paint((void*)1);
 assert(roll_button(BUTTON_MINUS,1)==1&&roll_refresh(1));assert(roll_button(BUTTON_PLUS,1)==1&&roll_refresh(1));
 roll_button(0x0c,0);assert(roll_pattern_index<0&&roll_patterns_state==1);
 roll_patterns_button(0x0d,0);roll_patterns_action=4;roll_patterns_button(0x0e,0);assert(roll_patterns_state==3);
 roll_patterns_control(0x11,64,0);assert(roll_patterns_end>roll_patterns_begin);roll_patterns_paint((void*)1);roll_patterns_button(0x0e,0);assert(pt_count()==original_count+2);
 roll_patterns_button(0x0d,0);roll_patterns_action=7;roll_patterns_button(0x0e,0);assert(!roll_patterns_state&&roll_pattern_index<0&&roll.clip_count);
 assert(tree()->size==count&&roll.count==count);
 roll_patterns_open();roll_patterns_button(0x0d,0);roll_patterns_action=3;roll_patterns_button(0x0e,0);assert(pt_count()==original_count+3);
 roll_patterns_button(0x0e,0);assert(roll_pattern_index>=0&&!roll.count);roll_action=12;roll_action_run();assert(roll_refresh(1)&&roll.count==1&&tree()->size==count);roll_paint((void*)1);
 step_native_project_changed();step_native_tick();assert(!atomic_load(&piano_active)&&!roll_patterns_state&&roll_pattern_index<0);
 unsigned entries=pt_count();struct pr_note packed_before[PR_MAX_EVENTS],packed_after[PR_MAX_EVENTS];int packed_count=pt_notes(variation,packed_before,PR_MAX_EVENTS,NULL);assert(packed_count>0);
 stub(0x1d27d90,patterns_write);pt_archive_save(NULL,NULL,patterns_original);assert(saved_size);
 for(unsigned i=0;i<4;++i)FIELD(saved_wrapper[i],8,void*)=i==3?saved_archive:saved_wrapper[i+1];
 FIELD(saved_archive,0x150,uintptr_t)=(uintptr_t)(saved_frame+0x38);FIELD(saved_frame,0x30,unsigned)=1;FIELD(saved_frame,0,void*)=saved_members;FIELD(saved_frame,8,void*)=saved_members+48;
 FIELD(saved_members,0,uint32_t)=12;FIELD(saved_members,8,const char*)="amPatternsV1";FIELD(saved_members,22,unsigned short)=1u<<10;
 FIELD(saved_members+24,0,uint32_t)=(uint32_t)saved_size;FIELD(saved_members+24,8,const char*)=saved_patterns;FIELD(saved_members+24,22,unsigned short)=1u<<10;
 pt_reset();assert(!pt_count());assert(pt_archive_read(NULL,NULL,patterns_read));assert(pt_count()==entries&&pt_notes(variation,packed_after,PR_MAX_EVENTS,NULL)==packed_count);
 for(int i=0;i<packed_count;++i)assert(packed_before[i].tune==packed_after[i].tune&&packed_before[i].slice==packed_after[i].slice&&!memcmp(&packed_before[i].event,&packed_after[i].event,sizeof(struct step_event)));
 fclose(draw_file);char host_path[1024];snprintf(host_path,sizeof host_path,"%s-host",argv[2]);draw_file=fopen(host_path,"w");assert(draw_file);
 phase="same Piano Roll with poisoned native Events pointers";
 void*old_root=root,*old_controller=controller,*old_sequence=sequence,*old_list=event_list;
 root=controller=sequence=event_list=(void*)1;showing=0;
 int old_bank=browse_bank;browse_bank=5;
 atomic_store(&clock_valid,1);atomic_store(&clock_running,0);atomic_store(&clock_recording,0);
 struct pt_entry host_entry;assert(pt_info(variation,&host_entry));
 assert(step_pattern_open(host_entry.id,240,host_redraw)&&step_pattern_active()&&roll.cursor_tick==240);
 int host_sound=roll.notes[0].sound;for(int i=0;i<host_sound/16;++i)host_press(0x23,0);
 assert(step_pattern_pad((unsigned)host_sound%16,0)&&roll.rows[roll.cursor_row]==host_sound&&!roll_input_held&&roll_audition<0);
 host_press(0x6f,0);assert(!roll_data_held); /* both physical DATA edges reach the host */
 for(unsigned i=0;i<roll.count;++i)roll.notes[i].selected=0;
 assert(!step_native_tick()&&step_pattern_active());assert(step_pattern_paint((void*)1,0));
 host_press(BUTTON_LEVELS,0);assert(roll.layer==PR_TUNE);assert(step_pattern_paint((void*)1,0));
 struct pr_note host_before[PR_MAX_EVENTS],host_after[PR_MAX_EVENTS];int host_count=pt_notes(variation,host_before,PR_MAX_EVENTS,NULL);assert(host_count>0);
 host_action(4);assert(pr_selected(&roll)==(unsigned)host_count);host_press(0x0e,0);assert(roll_editing);
 assert(step_pattern_control(0x64,1,0)&&step_pattern_control(0x11,65,0)&&step_pattern_control(0x12,65,0));
 assert(step_pattern_paint((void*)1,0));host_press(0x0e,0);
 assert(pt_notes(variation,host_after,PR_MAX_EVENTS,NULL)==host_count);
 for(int i=0;i<host_count;++i){assert(host_after[i].event.time==host_before[i].event.time+pr_grid_ticks(roll.grid));assert(host_after[i].tune==host_before[i].tune+1);}
 host_press(BUTTON_MINUS,1);assert(pt_notes(variation,host_after,PR_MAX_EVENTS,NULL)==host_count);assert(!memcmp(&host_after[0].event,&host_before[0].event,sizeof(struct step_event)));host_press(BUTTON_PLUS,1);
 host_action(12);assert(roll.count==(unsigned)host_count+1);assert(step_pattern_paint((void*)1,0));
 host_action(18);assert(!roll_patterns_state&&step_pattern_active());
 host_press(BUTTON_PLAY,0);host_press(BUTTON_RECORD,0);assert(step_pattern_active());
 host_press(0x0c,0);assert(roll.layer==PR_PADS);host_press(0x0c,0);assert(!step_pattern_active()&&host_redraws&&browse_bank==5);
 assert(step_pattern_open(host_entry.id,0,host_redraw));assert(variation>0&&pt_remove(0));assert(step_pattern_active()&&roll_pattern_index==(int)variation-1);
 host_press(BUTTON_MINUS,1);assert(step_pattern_active()&&roll_pattern_index==(int)variation);
 pt_reset();assert(!step_pattern_active()&&!pattern_host);
 FILE*previous_trace=draw_file;char fresh_path[1024];snprintf(fresh_path,sizeof fresh_path,"%s-new-pattern",argv[2]);draw_file=fopen(fresh_path,"w");assert(draw_file);
 pt_expect_epoch(pt_epoch());uint64_t fresh_clip=0;assert(pt_arr_new_pattern(0,0,0,3840,&fresh_clip));
 struct pt_arr_clip fresh_block;assert(pt_arr_clip_at(0,&fresh_block)&&step_pattern_open(fresh_block.pattern,0,host_redraw));
 assert(!roll.count&&!pt_pattern_assigned(0)&&!strcmp(message,"Unassigned instrument; editable draft only"));
 assert(step_pattern_paint((void*)1,0));host_press(0x0e,0);assert(roll.count==1);
 assert(step_pattern_paint((void*)1,0));host_press(BUTTON_LEVELS,0);assert(roll.layer==PR_TUNE);
 assert(step_pattern_control(0x64,1,0));for(unsigned i=0;i<roll.count;++i)roll.notes[i].selected=0;
 host_press(0x0e,0);assert(roll.count==2);host_press(BUTTON_MINUS,1);assert(roll.count==1);host_press(BUTTON_PLUS,1);assert(roll.count==2);
 assert(step_pattern_paint((void*)1,0));
 host_press(0x0c,0);host_press(0x0c,0);assert(!step_pattern_active());pt_reset();
 puts("PASS direct unassigned pattern: actual reused Piano Roll with poisoned Events pointers, B3 New manual Pad/Tune insertion and Undo/Redo; no native source/instrument pointer needed");
 fclose(draw_file);draw_file=previous_trace;
 FILE*saved_trace=draw_file;char separate_path[1024];snprintf(separate_path,sizeof separate_path,"%s-separate",argv[2]);draw_file=fopen(separate_path,"w");assert(draw_file);
 pt_expect_epoch(pt_epoch());int identity[128],direct[128];for(int i=0;i<128;++i){identity[i]=i;direct[i]=0;}
 struct pr_note row_notes[4]={0};
 for(unsigned i=0;i<4;++i){row_notes[i].sound=i<2?0:(int)i-1;row_notes[i].selected=1;row_notes[i].event=FN(0x27e54d0,struct step_event(*)(unsigned,int64_t,int64_t,unsigned,unsigned,float))((unsigned)row_notes[i].sound,(int64_t)i*960,10000,100,0,.8f);}
 FN(0x27e4400,void(*)(void*,int,int,int))(row_notes[1].event.data+4,0,0,4);
 assert(pt_capture(row_notes,4,7680,1,"row-ui",identity,direct,2)&&pt_count()==1);
 uint64_t row_id=0,block_id=0,other_row=0,other_block=0;assert(pt_arr_add_track("Song",&row_id)&&pt_arr_add_clip(row_id,0,0,15360,&block_id));
 assert(pt_arr_add_track("Other",&other_row)&&pt_arr_copy(block_id,other_row,0,&other_block));struct pt_entry row_source;assert(pt_info(0,&row_source));
 assert(step_pattern_open_clip(block_id,0,host_redraw,host_separated));
 host_press(0x0d,0);assert(roll.range);host_press(0x0d,0);assert(!roll.range);
 assert(step_pattern_button(0x0d,1,0)&&pattern_rows_held);assert(step_pattern_pad(0,0)&&pr_selected(&roll)==2);
 assert(step_pattern_pad(1,0)&&pr_selected(&roll)==3);assert(step_pattern_pad(0,0)&&pr_selected(&roll)==1);assert(step_pattern_pad(0,0)&&pr_selected(&roll)==3);
 assert(step_pattern_paint((void*)1,0));assert(step_pattern_button(0x0d,0,0)&&!roll.range&&!pattern_rows_held&&!roll_input_held&&roll_audition<0);
 host_action(19);assert(separated_clip&&!step_pattern_active());struct pt_arr_clip moved,remainder,other;
 assert(pt_arr_clip_by_id(separated_clip,&moved)&&pt_arr_clip_by_id(block_id,&remainder)&&pt_arr_clip_by_id(other_block,&other));
 assert(moved.pattern!=row_source.id&&remainder.pattern!=row_source.id&&other.pattern==row_source.id&&moved.length==15360);
 struct pt_entry entry;assert(pt_info((unsigned)pt_pattern_index(moved.pattern),&entry)&&entry.count==3&&entry.length==7680);
 assert(pt_history(-1)&&step_pattern_open_clip(block_id,0,host_redraw,host_separated));host_press(BUTTON_LEVELS,0);assert(roll.layer==PR_TUNE);
 host_press(0x23,0); /* Pattern Tune page 0 is -12..+3; +4 is on page 1. */
 int pitch_pad=-1;for(int i=0;i<16;++i)if(roll_pad_rows[i]==124)pitch_pad=i;assert(pitch_pad>=0);
 assert(step_pattern_button(0x0d,1,0));int saved_pitch=roll_pad_rows[15];roll_pad_rows[15]=-1;
 assert(step_pattern_pad(15,0)&&!pr_selected(&roll));roll_pad_rows[15]=saved_pitch;
 assert(step_pattern_pad((unsigned)pitch_pad,0)&&pr_selected(&roll)==1);assert(step_pattern_paint((void*)1,0));assert(step_pattern_button(0x0d,0,0)&&!roll.range);
 separated_clip=0;host_action(20);assert(separated_clip&&!step_pattern_active()&&pt_arr_clip_by_id(block_id,&other)&&other.pattern==row_source.id);
 assert(pt_arr_clip_by_id(separated_clip,&moved)&&pt_info((unsigned)pt_pattern_index(moved.pattern),&entry)&&entry.count==1&&entry.length==7680);
 assert(pt_history(-1)&&step_pattern_open_clip(block_id,0,host_redraw,host_separated));separated_clip=0;
 assert(step_pattern_button(0x0d,1,0)&&step_pattern_pad(0,0)&&pr_selected(&roll)==2);host_press(0x0e,0);
 assert(separated_clip&&!step_pattern_active()&&step_pattern_button(0x0d,0,0)&&!pattern_rows_held);
 assert(pt_history(-1)&&step_pattern_open_clip(block_id,0,host_redraw,host_separated));assert(pt_arr_variation(block_id)&&!step_pattern_active());
 assert(step_pattern_pad(0,0));assert(step_pattern_open_clip(block_id,0,host_redraw,host_separated));assert(step_pattern_button(0x0d,1,0));pt_reset();assert(!step_pattern_active());assert(step_pattern_button(0x0d,0,0)&&!pattern_rows_held);
 fclose(draw_file);draw_file=saved_trace;
 puts("PASS row extraction host: hold B2 + Pads toggles full Pad/Tune rows, no audition, existing range tap, invalid row ignored, Separate/Copy actions close only after safe external repaint and return numeric clip ID, other references unchanged, stale rebind/reset rejected with invalid native Events pointers");
 assert(browse_bank==5);browse_bank=old_bank;
 root=old_root;controller=old_controller;sequence=old_sequence;event_list=old_list;
 unsigned native_ordinal=0;for(void*n=tree()->left;n!=&tree()->colour;n=FN(0xb51738,void*(*)(void*))(n))assert(!memcmp((char*)n+0x20,source+native_ordinal++,sizeof(struct step_event)));assert(native_ordinal==count);
 free(saved_patterns);saved_patterns=NULL;fclose(draw_file);draw_file=NULL;
 puts("PASS pattern host: same Piano Roll with invalid native Events/Project context, pad/tune paint, selected-note move/transpose/length preview and commit, Undo/Redo, inline insertion, blocked native transport/library escape, return and project reset; native source unchanged.");
 puts("PASS pattern UI: actual existing Piano Roll adapter/encoder/fader actions, Pads split, variation edit and local Undo/Redo, range extraction, clipboard return, empty-pattern insert, packed native Tune/Chop archive roundtrip; native source tree unchanged and project reset closes editor. GUI/executor/routing/archive writer are fixtures; no full application built.");return 0;
}
