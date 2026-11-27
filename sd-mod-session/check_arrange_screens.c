/* Documentation screens: production Arrange painter on the SongScreen fixture (r31a).
 * Real song_ui.c/arrangement_ui.inc code; graphics calls are recorded, fonts substituted later. */
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
static int64_t fake_tick=-1;
int64_t step_native_clock_ticks(uint64_t*serial,unsigned*running){if(serial)*serial=1;if(running)*running=fake_tick>=0;return fake_tick<0?0:fake_tick;}
static void mark(const char*name){if(draw_trace)fprintf(draw_trace,"# %s\n",name);}
static uint64_t capture(const char*name,int64_t length,const int*pads,const int64_t*times,unsigned n){
    int mapping[128],outputs[128];for(int i=0;i<128;++i){mapping[i]=i;outputs[i]=0;}
    struct pr_note notes[16]={0};assert(n<=16);
    for(unsigned i=0;i<n;++i){notes[i].sound=pads[i];notes[i].event.type=3;notes[i].event.time=times[i];float v=.8f;memcpy(notes[i].event.data,&v,4);
        int64_t gate=240;memcpy(notes[i].event.data+24,&gate,8);notes[i].event.data[32]=(unsigned char)pads[i];notes[i].event.data[34]=100;}
    (void)notes;unsigned before=pt_count();assert(pt_capture(NULL,0,length,1,"song-ui-program",mapping,outputs,3)&&pt_count()==before+1); /* lane pictures need names and lengths only */
    struct pt_entry e;assert(pt_info(before,&e));assert(pt_rename(e.id,name));return e.id;
}
int main(int argc,char**argv){
    assert(argc==3);assert(!sequential_main(2,argv));
    screen=screen_obj;atomic_store(&shown,1);pt_init(elf_image);
    const int kick[]={36,38,36,38,40,40,40,40};const int64_t kt[]={0,960,1920,2880,0,480,1440,2400};
    const int bass[]={41,41,43,45};const int64_t bt[]={0,1440,1920,2880};
    const int keys[]={48,52,55};const int64_t ct[]={0,0,0};
    capture("Beat A",3840,kick,kt,8);capture("Beat B",3840,kick,kt,4);capture("Bass line",7680,bass,bt,4);capture("Chords",7680,keys,ct,3);
    press(B1,1);assert(atomic_load(&arrangement_active));
    stub(0x102d5e0,ink);stub(0x102e000,box);stub(0x1082810,font_make);stub(0x10817b0,label);
    draw_trace=fopen(argv[2],"w");assert(draw_trace);
    const char*names[]={"Drums","Bass","Keys","FX"};uint64_t track[4];
    for(unsigned i=0;i<4;++i){action(0);struct pt_arr_track t;assert(pt_arr_track_at(i,&t));track[i]=t.id;assert(pt_arr_rename_track(t.id,names[i]));}
    assert(pt_arr_length(16*3840));
    uint64_t id;
    for(int bar=0;bar<8;++bar)assert(pt_arr_add_clip(track[0],bar==3||bar==7?1:0,bar*3840,3840,&id));
    for(int bar=0;bar<8;bar+=2)assert(pt_arr_add_clip(track[1],2,bar*3840,7680,&id));
    assert(pt_arr_add_clip(track[2],3,2*3840,7680,&id));assert(pt_arr_add_clip(track[2],3,6*3840,7680,&id));
    arrange_tick();arrange_span=8*3840;arrange_left=0;arrange_rows=4;arrange_row=1;arrange_cursor=2*3840;arrange_top=0;
    /* 1: lane with a selected block and the song playing at bar 4.3 */
    {struct pt_arr_clip c;arrange_row=1;arrange_cursor=2*3840+960;assert(arrange_target(&c));arrange_selected=c.id;}
    transport_state=3;arrange_play_state=2;arrange_was_running=1;arrange_play_end=16*3840;arrange_play_from=0;arrange_play_to=0;fake_tick=3*3840+1920;
    toast_until=0;mark("lane-playing");shot();
    arrange_play_state=0;fake_tick=-1;transport_state=2;arrange_selected=0;
    /* 2: multi-selection being moved with Note Repeat held (preview) */
    arrange_multi_count=0;{struct pt_arrangement a;assert(pt_arr_info(&a));
        for(unsigned j=0;j<a.clips;++j){struct pt_arr_clip c;assert(pt_arr_clip_at(j,&c));if(c.track==track[0]&&c.start>=4*3840&&c.start<6*3840)arrange_multi[arrange_multi_count++]=c.id;}}
    arrange_cursor=4*3840;arrange_row=0;arrange_nr_multi=1;arrange_nr_dt=0;arrange_nr_drow=3;arrange_nr_first=4*3840;arrange_nr_top=0;
    toast_until=0;say("Move to bar 5.1, +3 tracks: release");mark("multi-move");shot();
    arrange_nr_multi=0;arrange_nr_dt=0;arrange_nr_drow=0;arrange_multi_count=0;
    /* 3: Actions menu, top and the Block group */
    toast_until=0;arrange_row=0;arrange_cursor=3840;mark("actions");press(B3,1);assert(arrange_menu==1);shot();
    turn(DATA_ENCODER,4,0);mark("actions-block");shot();press(B1,0);
    /* 4: rename a pattern with the Mod text input */
    action(19);assert(arrange_menu==7);phone_move(&arrange_name,-1);mark("rename");shot();
    phone_enable(&arrange_name,0);mark("rename-encoder");shot();press(B1,0);assert(!arrange_menu);
    /* 5: zoomed in: bar grid with beat lines, scrolled */
    toast_until=0;arrange_span=2*3840;arrange_left=3*3840;arrange_cursor=3*3840+1920;arrange_row=0;mark("zoom");shot();
    fclose(draw_trace);draw_trace=NULL;forget(screen_obj);
    puts("PASS Arrange documentation screens recorded");
    return 0;
}
