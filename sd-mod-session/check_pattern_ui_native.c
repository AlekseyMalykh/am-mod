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
int main(int argc,char**argv){
 assert(pattern_legacy_roll_main(argc,argv)==0);pt_init(base);
 FN(0xf535c0,void(*)(void*,const char*))(program+0xdc8,"pattern-ui-program");
 step_native_tick();roll_open();assert(atomic_load(&piano_active));
 unsigned count=roll.count;assert(count);struct step_event source[PR_MAX_EVENTS];for(unsigned i=0;i<count;++i)source[i]=roll.notes[i].event;
 draw_file=fopen(argv[2],"w");assert(draw_file);
 roll_action=18;roll_action_run();assert(roll_patterns_state==1);
 roll_patterns_button(0x0d,0);assert(roll_patterns_state==2);roll_patterns_action=0;roll_patterns_button(0x0e,0);
 assert(pt_count()>0&&roll_patterns_state==1&&roll.count==count);roll_patterns_paint((void*)1);
 unsigned original_count=pt_count();roll_patterns_button(0x0d,0);roll_patterns_action=5;roll_patterns_button(0x0e,0);
 assert(pt_count()==original_count+1);unsigned variation=(unsigned)roll_patterns_pick;
 roll_patterns_button(0x0e,0);assert(roll_pattern_index==(int)variation&&!roll_patterns_state);pr_select_all(&roll);
 assert(pr_edit(&roll,0,0,120,-5)&&pr_apply(&roll));assert(roll_refresh(1));
 assert(roll_layer_open(PR_TUNE,roll.notes[0].sound));pr_select_all(&roll);int previous_tune=roll.notes[0].tune;
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
 pt_reset();free(saved_patterns);saved_patterns=NULL;fclose(draw_file);draw_file=NULL;
 puts("PASS pattern UI: actual existing Piano Roll adapter/encoder/fader actions, Pads split, variation edit and local Undo/Redo, range extraction, clipboard return, empty-pattern insert, packed native Tune/Chop archive roundtrip; native source tree unchanged and project reset closes editor. GUI/executor/routing/archive writer are fixtures; no full application built.");return 0;
}
