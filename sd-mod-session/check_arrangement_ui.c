/* Real SongScreen hook + production Arrange overlay, native graphics/model
 * fixtures. Runs the entire existing Sequential/Export regression first. */
#define MPC_PATTERN_TOOLS_TRIAL 1
#define SONG_UI_PROOF_MAIN sequential_main
#include "check_song_ui.c"
#undef SONG_UI_PROOF_MAIN
static unsigned transport_state=2;
unsigned step_native_transport_state(void){return transport_state;}
static FILE*draw_trace;
static uint32_t trace_ink;static float trace_size;
static void ink(void*g,uint32_t c){(void)g;trace_ink=c;}
static void box(void*g,int x,int y,int w,int h){(void)g;assert(x>=0&&y>=0&&w>=0&&h>=0&&x+w<=320&&y+h<=240);if(draw_trace)fprintf(draw_trace,"R %d %d %d %d %08x\n",x,y,w,h,trace_ink);}
static void font_make(uint64_t*f,int face,float size){(void)face;*f=0;trace_size=size;}
static void label(void*g,const uint64_t*s,int x,int y,int w,int h,int flags,int lines,float scale){
    (void)g;(void)s;(void)lines;(void)scale;assert(x>=0&&y>=0&&w>=0&&h>=0&&x+w<=320&&y+h<=240);
    if(draw_trace)fprintf(draw_trace,"T %d %d %d %d %08x %.1f @%x %s\n",x,y,w,h,trace_ink,trace_size,flags,drawn);
}
static void press(unsigned code,int shifted){
    unsigned char e[16]={0};e[8]=(unsigned char)code;e[9]=1;
    assert(song_ui_owns(PHONE_BUTTON,code,shifted,1));assert(song_ui_button(e,shifted));
    assert(song_ui_owns(PHONE_BUTTON,code,0,0));e[9]=0;assert(song_ui_button(e,0));
    assert(!song_ui_owns(PHONE_BUTTON,code,0,0));
}
static void turn(unsigned cc,unsigned value,int shifted){unsigned char e[16]={0};e[8]=(unsigned char)cc;e[9]=(unsigned char)value;assert(song_ui_owns(PHONE_CONTROLLER,cc,shifted,1));assert(song_ui_control_at(e,shifted));}
static void action(unsigned pick){press(B2,0);assert(arrange_menu==1);if(pick)turn(DATA_ENCODER,pick,0);press(B3,0);}
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
    turn(DATA_ENCODER,4,0);assert(arrange_cursor==3840);press(0x4b,0);assert(pt_arr_info(&a)&&a.clips==2);shot();
    struct pt_arr_clip right;assert(arrange_clip(arrange_selected,&right)&&right.start==3840&&right.offset==3840);
    press(MINUS,1);assert(pt_arr_info(&a)&&a.clips==1);press(PLUS,1);assert(pt_arr_info(&a)&&a.clips==2);
    press(B3,0);turn(DATA_ENCODER,124,0);assert(arrange_cursor==0);press(B3,0);action(7);assert(pt_arr_info(&a)&&a.clips==1);
    phase="Copy reference to second track and non-destructive rejected overlap";
    action(2);action(0);assert(arrange_row==1);turn(DATA_ENCODER,8,0);action(3);assert(pt_arr_info(&a)&&a.clips==2&&pt_count()==1);shot();
    uint64_t revision=a.revision;action(3);assert(pt_arr_info(&a)&&a.revision==revision&&a.clips==2);press(B1,0);
    action(8);struct pt_arr_track t;assert(pt_arr_track_at(1,&t)&&t.muted);action(9);assert(pt_arr_track_at(1,&t)&&t.solo);
    phase="Zoom/scroll, boundary Chop refused, delete requires confirmation";
    int64_t before_span=arrange_span;turn(DATA_ENCODER,1,1);assert(arrange_span<before_span);turn(0x14,127,1);assert(arrange_rows==5);
    turn(0x12,127,0);shot();press(0x4b,0);assert(pt_arr_info(&a)&&a.clips==2);
    action(10);assert(arrange_menu==3);press(B1,0);assert(pt_arr_info(&a)&&a.clips==2);
    action(10);press(B3,0);assert(pt_arr_info(&a)&&a.clips==1);press(MINUS,1);assert(pt_arr_info(&a)&&a.clips==2);
    phase="Play/Record held locally; Stop and Shift-B1 Sequential stay native";
    press(0x52,0);press(0x50,0);assert(!song_ui_owns(PHONE_BUTTON,0x51,0,1));
    assert(ops==original_ops&&step_count==original_steps&&!memcmp(source,steps,sizeof steps));
    uint64_t old_ticket=song_ui_ticket();press(B1,0);assert(!atomic_load(&arrangement_active));assert(!song_ui_owns(PHONE_BUTTON,B1,1,1));
    unsigned char stale[16]={0};stale[8]=K1;stale[9]=127;
    assert(song_ui_control_ticket(stale,0,old_ticket)&&ops==original_ops);
    stale[8]=B1;stale[9]=1;assert(song_ui_button_ticket(stale,0,old_ticket)&&!atomic_load(&export_menu_active));
    phase="New/Load invalidates editor, including held Chop up";
    press(B1,0);turn(DATA_ENCODER,63,0);press(B3,0);assert(atomic_load(&arrangement_active));
    assert(song_ui_owns(PHONE_BUTTON,0x4b,0,1));old_ticket=song_ui_ticket();pt_reset();arrange_tick();assert(!atomic_load(&arrangement_active));
    assert(song_ui_owns(PHONE_BUTTON,0x4b,0,0));unsigned char up[16]={0};up[8]=0x4b;assert(song_ui_button_ticket(up,0,old_ticket));assert(!song_ui_owns(PHONE_BUTTON,0x4b,0,0));
    assert(pt_arr_info(&a)&&!a.tracks&&!a.clips&&!pt_count());
    fclose(draw_trace);draw_trace=NULL;forget(screen_obj);
    puts("PASS Arrange UI: real SongScreen overlay, Sequential/Export regression, paired ownership, blocked running entry/play/record, cursor/fader/zoom/scroll, insert/reference copy/rejected overlap, Chop/join/shared history, mute/solo, confirmed deletion, project reset and held releases; native song unchanged. Fixtures; no application build or hardware.");
    return 0;
}
