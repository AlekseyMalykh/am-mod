/* Real SongScreen hook + production Arrange overlay, native graphics/model
 * fixtures. Runs the entire existing Sequential/Export regression first. */
#define MPC_PATTERN_TOOLS_TRIAL 1
#define SONG_UI_PROOF_MAIN sequential_main
#include "check_song_ui.c"
#undef SONG_UI_PROOF_MAIN
static unsigned transport_state=2;
unsigned step_native_transport_state(void){return transport_state;}
/* Song handoff fixture. The real reused editor is tested with poisoned
 * Events pointers in check_pattern_ui_native.c, not duplicated here. */
static int bridge_active;static uint64_t bridge_id;static int64_t bridge_phase;
static unsigned bridge_controls,bridge_down,bridge_up,bridge_paints,bridge_pads;
static void(*bridge_redraw)(void);
static uint64_t bridge_clip;static void(*bridge_separated)(uint64_t);
int step_pattern_open(uint64_t id,int64_t phase,void(*redraw)(void)){bridge_active=1;bridge_id=id;bridge_phase=phase;bridge_redraw=redraw;return 1;}
int step_pattern_open_clip(uint64_t id,int64_t phase,void(*redraw)(void),void(*separated)(uint64_t)){struct pt_arr_clip c;if(!pt_arr_clip_by_id(id,&c))return 0;bridge_clip=id;bridge_separated=separated;return step_pattern_open(c.pattern,phase,redraw);}
int step_pattern_active(void){return bridge_active;}
void step_pattern_close(void){bridge_active=0;bridge_redraw=NULL;}
int step_pattern_button(unsigned code,int down,int shifted){(void)shifted;if(down)++bridge_down;else ++bridge_up;if(code==B1&&down)bridge_active=0;return 1;}
int step_pattern_control(unsigned cc,unsigned value,int shifted){(void)cc;(void)value;(void)shifted;++bridge_controls;if(bridge_redraw)bridge_redraw();return 1;}
int step_pattern_pad(unsigned pad,int shifted){assert(pad==3&&shifted);++bridge_pads;return 1;}
int step_pattern_paint(void*g,int shifted){(void)g;(void)shifted;++bridge_paints;return bridge_active;}
static FILE*draw_trace;
static uint32_t trace_ink;static float trace_size;
static int drawn_menu_rows,thumb_y,thumb_h;
static void ink(void*g,uint32_t c){(void)g;trace_ink=c;}
static void box(void*g,int x,int y,int w,int h){(void)g;assert(x>=0&&y>=0&&w>=0&&h>=0&&x+w<=320&&y+h<=240);if(x==314&&trace_ink==0xffffc247u){thumb_y=y;thumb_h=h;}if(draw_trace)fprintf(draw_trace,"R %d %d %d %d %08x\n",x,y,w,h,trace_ink);}
static void font_make(uint64_t*f,int face,float size){(void)face;*f=0;trace_size=size;}
static void label(void*g,const uint64_t*s,int x,int y,int w,int h,int flags,int lines,float scale){
    (void)g;(void)s;(void)lines;(void)scale;assert(x>=0&&y>=0&&w>=0&&h>=0&&x+w<=320&&y+h<=240);
    if(x==11&&y>=62&&y<194&&h==22)++drawn_menu_rows;
    if(draw_trace)fprintf(draw_trace,"T %d %d %d %d %08x %.1f @%x %s\n",x,y,w,h,trace_ink,trace_size,flags,drawn);
}
static void press(unsigned code,int shifted){
    unsigned char e[16]={0};e[8]=(unsigned char)code;e[9]=1;
    assert(song_ui_owns(PHONE_BUTTON,code,shifted,1));assert(song_ui_button(e,shifted));
    assert(song_ui_owns(PHONE_BUTTON,code,0,0));e[9]=0;assert(song_ui_button(e,0));
    assert(!song_ui_owns(PHONE_BUTTON,code,0,0));
}
static void turn(unsigned cc,unsigned value,int shifted){unsigned char e[16]={0};e[8]=(unsigned char)cc;e[9]=(unsigned char)value;assert(song_ui_owns(PHONE_CONTROLLER,cc,shifted,1));assert(song_ui_control_at(e,shifted));}
static void action(unsigned id){press(B3,1);assert(arrange_menu==1);unsigned pick=0;while(pick<(unsigned)ARRANGE_ORDER_COUNT&&arrange_order[pick]!=(int)id)++pick;assert(pick<(unsigned)ARRANGE_ORDER_COUNT);if(pick)turn(DATA_ENCODER,pick,0);press(B3,0);}
static void shot(void){paint(screen_obj,(void*)0x10);}
int main(int argc,char**argv){
    assert(argc==3);assert(!sequential_main(2,argv));
    screen=screen_obj;atomic_store(&shown,1);pt_init(elf_image);
    int mapping[128],outputs[128];for(int i=0;i<128;++i){mapping[i]=i;outputs[i]=0;}
    struct pr_note notes[2]={0};
    for(unsigned i=0;i<2;++i){notes[i].sound=36;notes[i].event.type=3;notes[i].event.time=i*3840;float v=.8f;memcpy(notes[i].event.data,&v,4);int64_t gate=10000;memcpy(notes[i].event.data+24,&gate,8);notes[i].event.data[32]=36;notes[i].event.data[34]=100;}
    assert(pt_capture(notes,2,7680,1,"song-ui-program",mapping,outputs,0)&&pt_count()==1);
    int original_ops=ops,original_steps=step_count;unsigned char source[sizeof steps];memcpy(source,steps,sizeof steps);
    phase="Arrange entry uses local menu, refuses running transport";
    press(B1,0);turn(DATA_ENCODER,63,0);assert(atomic_load(&export_choice)==export_items());
    transport_state=3;press(B3,0);assert(!atomic_load(&arrangement_active)&&atomic_load(&export_menu_active));
    transport_state=2;press(B3,0);assert(atomic_load(&arrangement_active)&&!atomic_load(&export_menu_active));
    stub(0x102d5e0,ink);stub(0x102e000,box);stub(0x1082810,font_make);stub(0x10817b0,label);
    draw_trace=fopen(argv[2],"w");assert(draw_trace);shot();
    action(0);struct pt_arrangement a;assert(pt_arr_info(&a)&&a.tracks==1);
    action(1);assert(arrange_menu==2);press(B3,0);assert(pt_arr_info(&a)&&a.clips==1&&arrange_selected);shot();
    phase="Chop inside pinned block, paired edges, shared history";
    turn(DATA_ENCODER,3,0);assert(arrange_cursor==2880);shot();press(0x4b,0);assert(pt_arr_info(&a)&&a.clips==2);shot();
    struct pt_arr_clip right,left;assert(arrange_clip(arrange_selected,&right)&&right.start==2880&&right.offset==2880&&right.length==4800);
    assert(pt_arr_clip_at(0,&left)&&left.start==0&&left.length==2880&&left.offset==0&&left.length+right.length==7680&&left.pattern==right.pattern);
    struct pt_entry source_pattern;struct pr_note unchanged_notes[2];
    assert(pt_info(0,&source_pattern)&&source_pattern.length==7680&&source_pattern.count==2&&pt_notes(0,unchanged_notes,2,NULL)==2);
    for(unsigned i=0;i<2;++i)assert(!memcmp(&unchanged_notes[i].event,&notes[i].event,sizeof(struct step_event)));
    puts("CHOP_LENGTHS source=7680 left=2880 right=4800 right_offset=2880; source pattern unchanged");
    phase="Song handoff uses source phase and restores the same cursor/block";
    int64_t held_cursor=arrange_cursor,held_left=arrange_left;int held_row=arrange_row;uint64_t held_block=arrange_selected,held_ticket=song_ui_ticket();
    action(13);
    assert(atomic_load(&arrangement_editor)&&bridge_id==right.pattern&&bridge_phase==2880);
    assert(song_ui_pattern_pads()&&!song_ui_pads_active());song_ui_pattern_pad(3,1,held_ticket);assert(!bridge_pads);song_ui_pattern_pad(3,1,song_ui_ticket());assert(bridge_pads==1);
    unsigned char old_movement[16]={0};old_movement[8]=DATA_ENCODER;old_movement[9]=1;
    assert(song_ui_control_ticket(old_movement,0,held_ticket)&&!bridge_controls);
    turn(DATA_ENCODER,1,0);press(0x6f,0);press(B2,0);assert(bridge_controls==1&&bridge_down==2&&bridge_up==3&&!arrange_menu);
    paint(screen_obj,(void*)0x10);assert(bridge_paints==1);
    press(B1,0);assert(!atomic_load(&arrangement_editor)&&!bridge_active);
    assert(!song_ui_pattern_pads()&&song_ui_pads_active());song_ui_pattern_pad(3,1,song_ui_ticket());assert(bridge_pads==1);
    assert(arrange_cursor==held_cursor&&arrange_left==held_left&&arrange_row==held_row&&arrange_selected==held_block);
    press(MINUS,1);assert(pt_arr_info(&a)&&a.clips==1);press(PLUS,1);assert(pt_arr_info(&a)&&a.clips==2);
    press(0x6f,0);turn(DATA_ENCODER,125,0);assert(arrange_cursor==0);press(0x6f,0);action(7);assert(pt_arr_info(&a)&&a.clips==1);shot();
    phase="Copy reference to second track and non-destructive rejected overlap";
    action(2);action(0);assert(arrange_row==1);turn(DATA_ENCODER,8,0);action(3);assert(pt_arr_info(&a)&&a.clips==2&&pt_count()==1);shot();
    uint64_t revision=a.revision;action(3);assert(pt_arr_info(&a)&&a.revision==revision&&a.clips==2);press(B1,0);
    action(8);struct pt_arr_track t;assert(pt_arr_track_at(1,&t)&&t.muted);action(9);assert(pt_arr_track_at(1,&t)&&t.solo);
    phase="Zoom/scroll, boundary Chop refused, delete requires confirmation";
    int64_t before_span=arrange_span;turn(DATA_ENCODER,1,1);assert(arrange_span<before_span);turn(0x14,127,1);assert(arrange_rows==3);
    turn(0x12,127,0);shot();press(0x4b,0);assert(pt_arr_info(&a)&&a.clips==2);
    action(10);assert(arrange_menu==3);press(B1,0);assert(pt_arr_info(&a)&&a.clips==2);
    action(10);press(B3,0);assert(pt_arr_info(&a)&&a.clips==1);press(MINUS,1);assert(pt_arr_info(&a)&&a.clips==2);
    phase="Fader follows screen direction; six filled menu rows and visible scrollbar endpoints";
    turn(0x14,127,0);assert(arrange_row==0);turn(0x14,0,0);assert(arrange_row==1);
    turn(0x14,0,1);assert(arrange_rows==6);turn(0x14,127,1);assert(arrange_rows==3);
    FILE*timeline_trace=draw_trace;char menu_path[4096];snprintf(menu_path,sizeof menu_path,"%s-menus.txt",argv[2]);draw_trace=fopen(menu_path,"w");assert(draw_trace);
    press(B3,1);turn(0x14,127,0);assert(arrange_pick==0);drawn_menu_rows=0;shot();assert(drawn_menu_rows==6&&thumb_y==62&&thumb_h>0&&thumb_h<132);
    int previous_pick=0;for(unsigned value=127;value>0;--value){turn(0x14,value,0);assert(arrange_pick>=previous_pick);previous_pick=arrange_pick;}
    turn(0x14,0,0);assert(arrange_pick==ARRANGE_ACTIONS-1);drawn_menu_rows=0;shot();assert(drawn_menu_rows==6&&thumb_y+thumb_h==194);
    turn(K1,0,0);assert(arrange_pick==0);turn(K1,127,0);assert(arrange_pick==ARRANGE_ACTIONS-1);
    press(B1,0);action(1);assert(arrange_menu==2);shot();press(B1,0);
    fclose(draw_trace);draw_trace=timeline_trace;
    puts("PASS screen navigation: fader up selects top, down selects bottom; zoom up increases row size; full menu viewport and scrollbar ends, knobs retain direction");
    phase="Play/Record held locally; Stop stays native; Shift B1 toggles Arrange";
    press(0x52,0);press(0x50,0);assert(!song_ui_owns(PHONE_BUTTON,0x51,0,1));
    assert(ops==original_ops&&step_count==original_steps&&!memcmp(source,steps,sizeof steps));
    uint64_t old_ticket=song_ui_ticket();press(B1,1);assert(!atomic_load(&arrangement_active));
    transport_state=3;press(B1,1);assert(!atomic_load(&arrangement_active)&&!atomic_load(&export_menu_active));transport_state=2;
    press(B1,1);assert(atomic_load(&arrangement_active));press(B1,1);assert(!atomic_load(&arrangement_active));
    unsigned char stale[16]={0};stale[8]=K1;stale[9]=127;
    assert(song_ui_control_ticket(stale,0,old_ticket)&&ops==original_ops);
    stale[8]=B1;stale[9]=1;assert(song_ui_button_ticket(stale,0,old_ticket)&&!atomic_load(&export_menu_active));
    phase="New/Load invalidates editor, including held Chop up";
    press(B1,0);turn(DATA_ENCODER,63,0);press(B3,0);assert(atomic_load(&arrangement_active));
    /* Native reset must close the host before any new-project input. */
    action(13);assert(atomic_load(&arrangement_editor));
    assert(song_ui_owns(PHONE_BUTTON,0x4b,0,1));old_ticket=song_ui_ticket();pt_reset();arrange_tick();assert(!atomic_load(&arrangement_active));
    assert(song_ui_owns(PHONE_BUTTON,0x4b,0,0));unsigned char up[16]={0};up[8]=0x4b;assert(song_ui_button_ticket(up,0,old_ticket));assert(!song_ui_owns(PHONE_BUTTON,0x4b,0,0));
    assert(pt_arr_info(&a)&&!a.tracks&&!a.clips&&!pt_count()&&!bridge_active&&!atomic_load(&arrangement_editor));
    FILE*legacy_trace=draw_trace;char workflow_path[4096];snprintf(workflow_path,sizeof workflow_path,"%s-new.txt",argv[2]);draw_trace=fopen(workflow_path,"w");assert(draw_trace);
    press(B1,0);turn(DATA_ENCODER,63,0);press(B3,0);assert(atomic_load(&arrangement_active));shot();
    press(B3,0);assert(atomic_load(&arrangement_editor)&&bridge_active&&pt_count()==1);
    assert(pt_arr_info(&a)&&a.tracks==1&&a.clips==1);struct pt_arr_clip fresh;assert(pt_arr_clip_at(0,&fresh)&&fresh.start==0&&fresh.length==3840);
    assert(!pt_pattern_assigned(0));press(B1,0);assert(!arrange_selected);shot();
    press(B3,0);assert(atomic_load(&arrangement_editor));press(B1,0);
    press(0x6f,0);assert(arrange_selected==fresh.id);press(B3,0);assert(arrange_editing);uint64_t unchanged_revision=a.revision;
    turn(DATA_ENCODER,2,0);assert(arrange_draft.start==1920);shot();assert(pt_arr_info(&a)&&a.revision==unchanged_revision&&pt_arr_clip_at(0,&fresh)&&fresh.start==0);
    press(B1,0);assert(!arrange_editing&&arrange_selected==fresh.id);press(B3,0);turn(DATA_ENCODER,2,0);turn(0x12,0,0);press(B3,0);
    assert(!arrange_editing&&pt_arr_clip_at(0,&fresh)&&fresh.start==1920&&fresh.length==960);shot();
    press(MINUS,1);assert(pt_arr_clip_at(0,&fresh)&&fresh.start==0&&fresh.length==3840);press(PLUS,1);
    press(0x6f,0);turn(DATA_ENCODER,6,0);press(B2,0);turn(DATA_ENCODER,1,0);assert(arrange_range);shot();
    press(B3,0);assert(atomic_load(&arrangement_editor)&&pt_count()==2);press(B1,0);
    assert(pt_arr_clip_at(1,&fresh)&&fresh.start==7680&&fresh.length==1920);shot();
    action(2);turn(DATA_ENCODER,4,0);action(3);assert(pt_arr_info(&a)&&a.clips==3);struct pt_arr_clip copied;assert(arrange_clip(arrange_selected,&copied)&&copied.pattern==fresh.pattern);
    action(15);assert(pt_count()==3&&arrange_clip(copied.id,&copied)&&copied.pattern!=fresh.pattern);press(MINUS,1);assert(pt_count()==2&&arrange_clip(copied.id,&copied)&&copied.pattern==fresh.pattern);
    assert(ops==original_ops&&step_count==original_steps&&!memcmp(source,steps,sizeof steps));
    FILE*direct_trace=draw_trace;char separate_path[4096];snprintf(separate_path,sizeof separate_path,"%s-separate.txt",argv[2]);draw_trace=fopen(separate_path,"w");assert(draw_trace);
    press(B1,0);arrange_cursor=7680;shot();action(13);assert(atomic_load(&arrangement_editor)&&bridge_clip==fresh.id&&bridge_separated);
    int part_index=pt_pattern_index(fresh.pattern);struct pt_entry part_entry;assert(part_index>=0&&pt_info((unsigned)part_index,&part_entry));
    struct step_event two[2]={notes[0].event,notes[0].event};two[0].time=0;two[0].data[32]=0;two[1].time=960;two[1].data[32]=1;
    struct pr_plan addition={0};addition.revision=part_entry.revision;addition.added=two;addition.added_count=2;assert(pt_commit((unsigned)part_index,&addition));
    struct pr_note selection[2];assert(pt_notes((unsigned)part_index,selection,2,NULL)==2);selection[0].selected=1;uint64_t child=0;
    assert(pt_arr_separate(fresh.id,fresh.pattern,pt_serial(),selection,2,0,&child));bridge_active=0;bridge_separated(child);
    struct pt_arr_clip part;struct pt_arr_track part_row;assert(!atomic_load(&arrangement_editor)&&arrange_selected==child&&pt_arr_clip_by_id(child,&part)&&pt_arr_track_at((unsigned)arrange_row,&part_row)&&part_row.id==part.track&&arrange_cursor==part.start);shot();
    fclose(draw_trace);draw_trace=direct_trace;
    press(B1,0);arrange_cursor=15360;assert(song_ui_owns(PHONE_BUTTON,0x6f,0,1));unsigned char held[16]={0};held[8]=0x6f;held[9]=1;assert(song_ui_button(held,0));turn(DATA_ENCODER,1,0);assert(arrange_range&&arrange_data_held);
    old_ticket=song_ui_ticket();pt_reset();arrange_tick();held[9]=0;assert(song_ui_button_ticket(held,0,old_ticket)&&!arrange_data_held&&!arrange_selected);
    fclose(draw_trace);draw_trace=legacy_trace;
    fclose(draw_trace);draw_trace=NULL;forget(screen_obj);
    puts("PASS Arrange direct workflow: B3 New/Open/Edit/Apply, DATA selection, B2 range length, preview/cancel without Store writes, one-step Undo, atomic variation of one reference, unassigned fresh project, stale held DATA release and original native Song untouched");
    puts("PASS Arrange UI: real SongScreen overlay, Sequential/Export regression, paired ownership, blocked running entry/play/record, cursor/fader/zoom/scroll, insert/reference copy/rejected overlap, Chop/join/shared history, mute/solo, confirmed deletion, project reset and held releases; native song unchanged. Fixtures; no application build or hardware.");
    return 0;
}
