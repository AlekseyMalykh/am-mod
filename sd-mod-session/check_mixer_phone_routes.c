#define PHONE_RGB_TEST 1
#define MPC_MIXER_LIVE 1
#include "src/phone_native.c"
#include "src/fx_catalog.h"
#include <assert.h>
#include <unistd.h>

struct mixer_session { struct mixer_control *control; };
struct mixer_control *mixer_session_control(struct mixer_session *s) { return s ? s->control : NULL; }
int mixer_session_status(const struct mixer_session *s) { return s ? 1 : -2; }
void mixer_session_flush_fx(struct mixer_session *s) { (void)s; }
static unsigned fixture_fx_mode;
unsigned music_fx_mode_active(void){return fixture_fx_mode;}
unsigned music_text_input(void) { return 0; }
void mixer_session_ui_tick(struct mixer_session *s) { (void)s; }
void note_native_repaint_sample(void) {}
void note_native_repaint_sample_region(unsigned x,unsigned y,unsigned w,unsigned h)
{ assert(!x && !y && w==320 && h==198); }
int note_native_sample_visible(void) { return 1; }
int note_native_sample_shown(void) { return 1; }
int music_ui_sequence_visible(void) { return 0; }
void sample_tuner_request(void) {}
static unsigned fixture_workflow_blocks;
int workflow_blocks_sample_controls(void) { return fixture_workflow_blocks; }
int note_native_active(void) { return 0; }
unsigned music_hide_play_cursor(void){return 0;}
int music_pad_lighting_mode(void) { return 1; }
int music_pad_colour(unsigned pad) { return (int)(pad%16); }
uint32_t music_user_rgb(unsigned raw) { return 0x102030u+raw; }
uint32_t music_service_rgb(unsigned role) { (void)role; return 0xffffffu; }
uint32_t music_led_correct(uint32_t rgb) { return rgb; }
uint32_t music_relation_rgb(unsigned role) { (void)role; return 0xffffffu; }
int music_ui_active(void) { return 0; }
static uint64_t fixture_palette_ticket;
uint64_t music_ui_palette_ticket(void) { return fixture_palette_ticket; }
static unsigned fixture_knob_fx;
int music_ui_knob_fx_active(void) { return fixture_knob_fx; }
static unsigned fixture_levels,fixture_levels_mode;
int music_lighting_levels_state(unsigned *e,unsigned *m,unsigned *o)
{ *e=fixture_levels;*m=fixture_levels_mode;*o=3;return 1; }
static unsigned fixture_step_active,fixture_step_owned=~0u;
int step_native_active(void) { return fixture_step_active!=0; }
int step_native_browser_bank(int bank) { static int current=5;if(bank>=0)current=bank;return current; }
int mixer_live_native_binding(struct mixer_native_binding *binding)
{ memset(binding,0,sizeof(*binding));binding->epoch=1;binding->program=(void*)0x2000;return 1; }
static unsigned fixture_workflow_visible;
int workflow_active(void) { return fixture_workflow_visible; }
static unsigned fixture_seq_variant,fixture_seq_toggles;
int seq_mutes_variant(void){return fixture_seq_variant;}
void seq_mutes_set_variant(int on){fixture_seq_variant=!!on;}
static int fixture_sequence_index;
static uint64_t fixture_sequence_mutes[2];
int seq_mutes_current(void){return fixture_sequence_index;}
int music_sequence_mutes_get(unsigned sequence,uint64_t out[2]){
    out[0]=sequence==0?fixture_sequence_mutes[0]:0;out[1]=sequence==0?fixture_sequence_mutes[1]:0;
    return (out[0]|out[1])!=0;
}
int seq_mutes_clear(void){return 1;}
int seq_mutes_toggle(unsigned bank,unsigned pad){assert(bank==5&&pad==3);++fixture_seq_toggles;return 1;}
int seq_mutes_pad_state(unsigned bank,unsigned pad){(void)bank;(void)pad;return 0;}
int music_ui_exit_saving(void) { return 0; }
int music_ui_transfer_light(unsigned p,uint64_t*l) { (void)p;(void)l;return 0; }
int note_native_owns(unsigned c) { (void)c; return 0; }
static unsigned fixture_music_cc=~0u,fixture_step_cc=~0u;
int music_ui_owns(unsigned c) { return c==fixture_music_cc; }
int step_native_owns(unsigned c) { return c==fixture_step_cc; }
static unsigned fixture_music_owned=~0u;
int music_ui_button_owns(unsigned c) { return c==fixture_music_owned; }
int step_native_button_owns(unsigned c) { return c==fixture_step_owned; }
int workflow_owns(unsigned k,unsigned c) { (void)k; (void)c; return 0; }
int workflow_button(void *e,uint64_t t) { (void)e; (void)t; return 0; }
uint64_t step_native_ticket(void) { return 1; }
uint64_t workflow_ticket(void) { return 1; }
int step_native_pad_route(uint64_t t) { (void)t; return 0; }
void step_native_queue_fault(void) {}
void workflow_fault(void) {}
void note_native_button_requested(unsigned c,unsigned d) { (void)c; (void)d; }
void music_ui_requested(unsigned c,unsigned d) { (void)c; (void)d; }
void step_native_requested(unsigned c,unsigned d) { (void)c; (void)d; }

static unsigned stock_down[112],stock_up[112],stock_pad[2],stock_cc;
static unsigned bank_led[128];
static void receive_bank_light(void *sender,unsigned value,unsigned code)
{
    assert(sender==(void*)0x4000&&code<128);
    bank_led[code]=value;
}
static void receive_button(void *e)
{
    unsigned code=FIELD(e,8,unsigned char);
    if (FIELD(e,9,unsigned char)) ++stock_down[code];
    else ++stock_up[code];
}
static void receive_pad(void *e)
{
    assert(FIELD(e,8,unsigned char)==8);
    unsigned down=FIELD(e,16,unsigned char);
    assert(down<2);
    if(down) assert(atomic_load(&native_buttons_down[0x31]));
    ++stock_pad[down];
}
static void receive_cc(void *e) { (void)e; ++stock_cc; }
static int modal_open;
static void *top_modal(int i) { assert(!i); return modal_open?(void*)1:NULL; }
static int midi_down(void *e,int z) { assert(!z); return FIELD(e,0,unsigned char)==1; }
static int midi_up(void *e,int one) { assert(one==1); return FIELD(e,0,unsigned char)==0; }
static void stub(uintptr_t offset,void *fn)
{
    uint32_t code[]={0x58000050,0xd61f0200};
    memcpy((void*)(base+offset),code,8);
    memcpy((void*)(base+offset+8),&fn,8);
}
static int submit(void *u,const struct mixer_project *p,uint64_t old,uint64_t *next)
{ (void)u; (void)p; *next=old+1; return 1; }
static void pad(unsigned number,int down)
{
    _Alignas(8) unsigned char e[40]={0};
    e[8]=number;
    e[16]=down;
    FIELD(e,32,int)=3;
    pad_event(e);
}
static void button(unsigned code,int down)
{
    _Alignas(8) unsigned char e[16]={0};
    e[8]=code;
    e[9]=down;
    buttons(e);
}
static void drain(void)
{
    struct phone_event e;
    while(phone_events_pop(&e)) {
        if(mixer_dispatch(&e)) continue;
        if(e.kind==PHONE_BUTTON) forward_buttons(e.native);
    }
}
int main(void)
{
    void *image=mmap(NULL,0x7500000,7,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    assert(image!=MAP_FAILED);
    base=(uintptr_t)image;
    stub(phone_hooks[H_BUTTONS].trampoline,receive_button);
    stub(phone_hooks[H_CONTROLLER].trampoline,receive_cc);
    stub(phone_hooks[H_PAD].trampoline,receive_pad);
    stub(0x10f87a0,top_modal);
    stub(0xddfed0,midi_down);
    stub(0xddff20,midi_up);
    __builtin___clear_cache(image,(char*)image+0x7500000);

    /* Exercise the real button producer, including an undrained UI FIFO:
     * Stop release must immediately end the tuner chord. */
    button(0x51,1);button(0x51,0);button(0x4b,1);
    assert(!atomic_load(&tuner_win));button(0x4b,0);drain();
    assert(stock_down[0x4b]==1&&stock_up[0x4b]==1);
    button(0x51,1);button(0x4b,1);
    assert(atomic_load(&tuner_win));button(0x4b,0);
    assert(!atomic_load(&tuner_eat[0x4b]));
    button(0x51,0);drain();assert(!atomic_load(&stop_requested));
    button(0x4b,1);button(0x4b,0);assert(!atomic_load(&tuner_win));
    button(0x4b,1);button(0x4b,0);drain();
    assert(stock_down[0x4b]==2&&stock_up[0x4b]==2);
    memset(stock_down,0,sizeof stock_down);memset(stock_up,0,sizeof stock_up);
    puts("PASS Stop release immediately ends Stop+Chop tuner gesture; sequential Chop stays native, held chord opens and paired releases/close work with queued input");

    _Alignas(8) unsigned char confirm[16]={0};confirm[8]=0x0e;confirm[9]=1;
    assert(!step_confirm_to_native(confirm,4));
    modal_open=1;assert(step_confirm_to_native(confirm,4)&&stock_down[0x0e]==1);
    modal_open=0;confirm[9]=0;
    assert(step_confirm_to_native(confirm,0)&&stock_up[0x0e]==1);
    assert(!step_confirm_to_native(confirm,0));
    step_confirm_deadline=now_ms()+350;
    atomic_store(&step_confirm_waiting,1);
    confirm[9]=1;assert(step_confirm_to_native(confirm,4));
    confirm[9]=0;assert(step_confirm_to_native(confirm,4));
    assert(stock_down[0x0e]==1&&stock_up[0x0e]==1);
    modal_open=1;step_confirm_tick();modal_open=0;
    assert(stock_down[0x0e]==2&&stock_up[0x0e]==2);


    struct mixer_project project;
    mixer_defaults(&project);
    struct mixer_control_ops ops={NULL,submit};
    struct mixer_session session={mixer_control_create(&ops,48000)};
    assert(session.control && mixer_control_confirm(session.control,&project,1,1));
    mixer_ui_session=&session;
    mixer_binding=1;
    mixer_entry_init(&mixer_entry,500);
    mixer_publish_route();
    native_bank_light=receive_bank_light;
    mixer_bank_lights_tracks(0x81);
    mixer_bank_light((void*)0x4000,1,BANK_LED_FIRST+5);
    assert(bank_led[BANK_LED_FIRST]&&bank_led[BANK_LED_FIRST+7]&&!bank_led[BANK_LED_FIRST+5]);
    mixer_bank_lights_tracks(-1);
    for(unsigned i=0;i<BANK_LED_COUNT;++i)
        mixer_bank_light((void*)0x4000,i==5,BANK_LED_FIRST+i);
    assert(bank_led[BANK_LED_FIRST+5]==1);

    /* A lone PAD FX is held back only briefly, then reaches the native app
     * unchanged (released early, or after the combo window). */
    button(1,1);assert(!stock_down[1]);button(1,0);assert(stock_down[1]==1&&stock_up[1]==1);
    button(0,1);assert(!stock_down[0]);usleep(300000);mixer_tick_ui();assert(stock_down[0]==1);
    button(0,0);assert(stock_up[0]==1);
    /* PAD FX + KNOB FX opens the Mixer; neither press nor release is native. */
    button(1,1);button(0,1);button(1,0);button(0,0);drain();
    assert(mixer_control_active(session.control)&&stock_down[1]==1&&stock_up[1]==1&&stock_down[0]==1&&stock_up[0]==1);
    for(unsigned shifted=0;shifted<2;++shifted){
        button(0x31,(int)shifted);
        const unsigned blocked[]={0x4a,0x4b,0x28};
        for(unsigned i=0;i<3;++i){unsigned code=blocked[i];
            unsigned before_down=stock_down[code],before_up=stock_up[code];
            button(code,1);button(code,0);drain();
            assert(stock_down[code]==before_down&&stock_up[code]==before_up&&!atomic_load(&tuner_win));
        }
    }
    button(0x31,0);drain();
    uint64_t before_route=atomic_load(&mixer_route);
    button(0x4a,1);atomic_store(&mixer_route,0);button(0x4a,0);
    unsigned before_loop=stock_down[0x4a];button(0x4a,1);button(0x4a,0);drain();assert(stock_down[0x4a]==before_loop+1);
    atomic_store(&mixer_route,before_route);
    puts("PASS Mixer blocks Loop/Reverse, Chop/Note On and 16 Levels in both Shift layers; swallowed releases survive exit and normal Loop works outside");

    assert(bank_led[BANK_LED_FIRST]==1&&!bank_led[BANK_LED_FIRST+5]);
    mixer_bank_light((void*)0x4000,0,BANK_LED_FIRST+5);
    mixer_bank_light((void*)0x4000,1,BANK_LED_FIRST+6);
    mixer_bank_light((void*)0x4000,1,0x41);
    assert(!bank_led[BANK_LED_FIRST+6]&&bank_led[0x41]);
    assert(mixer_control_button(session.control,MIXER_KNOB,1,mixer_control_ticket(session.control)));
    assert(mixer_control_button(session.control,MIXER_KNOB,0,mixer_control_ticket(session.control)));
    mixer_publish_route();assert(!bank_led[BANK_LED_FIRST+6]);
    assert(mixer_control_button(session.control,MIXER_B1,1,mixer_control_ticket(session.control)));
    assert(mixer_control_button(session.control,MIXER_B1,0,mixer_control_ticket(session.control)));
    mixer_publish_route();assert(!bank_led[BANK_LED_FIRST+6]);
    assert(mixer_control_button(session.control,MIXER_B2,1,mixer_control_ticket(session.control)));
    assert(mixer_control_button(session.control,MIXER_B2,0,mixer_control_ticket(session.control)));
    mixer_publish_route();assert(bank_led[BANK_LED_FIRST+6]==1);
    assert(mixer_control_button(session.control,MIXER_B1,1,mixer_control_ticket(session.control)));
    assert(mixer_control_button(session.control,MIXER_B1,0,mixer_control_ticket(session.control)));
    mixer_publish_route();assert(!bank_led[BANK_LED_FIRST+6]);
    pad(4,1);
    pad(4,0);
    drain();
    struct mixer_view_state view;
    mixer_control_view(session.control,0,&view);
    assert(view.page==MIXER_OVERVIEW && view.primary==4 && !stock_pad[0] && !stock_pad[1]);
    for(unsigned i=0;i<BANK_LED_COUNT;++i)assert(bank_led[BANK_LED_FIRST+i]==(i==4));
    /* All three Routing tabs use the real production input and RGB owner. */
    unsigned original_bank=view.bank;
    button(0x0d,1);button(0x0d,0);button(0x0e,1);button(0x0e,0);drain();
    mixer_control_view(session.control,0,&view);
    assert(view.page==MIXER_INSPECT&&view.route_tab==MIXER_ROUTE_SENDS);
    assert(!mixer_view_sound_pads(&view));
    assert(bank_led[BANK_LED_FIRST+4]==1&&!bank_led[BANK_LED_FIRST+6]);
    uint64_t ignored;assert(!mixer_leds_light(0,0,&ignored));
    assert(atomic_load(&mixer_select_lights[4])==pad_light(view.project->track[4].colour,1.f));
    pad(1,1);pad(1,0);drain();mixer_control_view(session.control,0,&view);
    assert(view.primary==1&&view.bank==original_bank&&bank_led[BANK_LED_FIRST+1]);
    button(0x0e,1);button(0x0e,0);drain();
    pad(15,1);pad(15,0);drain();mixer_control_view(session.control,0,&view);
    assert(view.route_tab==MIXER_ROUTE_KEYS&&view.master);
    for(unsigned i=0;i<BANK_LED_COUNT;++i)assert(!bank_led[BANK_LED_FIRST+i]);
    pad(4,1);pad(4,0);pad(9,1);pad(9,0);drain();mixer_control_view(session.control,0,&view);
    assert(view.primary==4&&!view.master&&view.fx_slot==0); /* First free slot clamps. */
    button(0x0d,1);button(0x0d,0);button(0x0d,1);button(0x0d,0);drain();
    mixer_control_view(session.control,0,&view);assert(mixer_view_sound_pads(&view));
    assert(bank_led[BANK_LED_FIRST+6]);
    button(0x0c,1);button(0x0c,0);drain();
    puts("PASS Routing Pads owns sounds/bank, Sends and Sidechain own Track RGB/selection/A-H including Master and FX slots; transitions restore the pad bank");
    uint64_t empty_fx=pad_light(0x808080u,.22f);
    for(unsigned i=8;i<12;++i)assert(atomic_load(&mixer_select_lights[i])==empty_fx);
    for(unsigned i=0;i<3;++i){
        assert(fx_catalog_stage(0,48000,&project.track[4].fx.stage[i]));
        project.track[4].fx.count=i+1;
        assert(mixer_control_confirm(session.control,&project,2+i,1));
        mixer_publish_route();
        for(unsigned slot=0;slot<4;++slot)
            assert(atomic_load(&mixer_select_lights[8+slot])==(slot<=i?pad_light(mixer_fx_slot_rgb(slot),.90f):empty_fx));
    }
    pad(4,1);pad(5,1);pad(5,0);pad(4,0);drain();
    for(unsigned i=0;i<BANK_LED_COUNT;++i)assert(bank_led[BANK_LED_FIRST+i]==(i==4||i==5));
    button(0,1);button(0,0);drain();
    for(unsigned i=0;i<BANK_LED_COUNT;++i)assert(bank_led[BANK_LED_FIRST+i]==(i==4||i==5));
    pad(15,1);pad(15,0);drain();
    for(unsigned i=0;i<BANK_LED_COUNT;++i)assert(!bank_led[BANK_LED_FIRST+i]);
    mixer_bank_light((void*)0x4000,1,BANK_LED_FIRST+2);
    assert(!bank_led[BANK_LED_FIRST+2]);
    mixer_bank_light((void*)0x4000,0,BANK_LED_FIRST+2);
    button(0x0c,1);button(0x0c,0);drain();
    pad(4,1);pad(5,1);pad(5,0);pad(4,0);drain();
    for(unsigned i=8;i<12;++i)assert(atomic_load(&mixer_select_lights[i])==empty_fx);
    pad(4,1);pad(4,0);drain();
    assert(atomic_load(&mixer_select_lights[8])==pad_light(mixer_fx_slot_rgb(0),.90f));
    button(0x23,1);button(0x23,0);drain();
    assert(stock_down[0x23]==0&&stock_up[0x23]==0); /* Mixer overview does not change the native bank. */
    button(1,1);button(1,0);drain();
    button(0x23,1);button(0x23,0);drain();
    mixer_control_view(session.control,0,&view);
    assert(view.page==MIXER_ASSIGN&&view.bank==6&&view.primary==4);
    button(0x36,1);button(0x36,0);drain();
    mixer_control_view(session.control,0,&view);
    assert(view.page==MIXER_ASSIGN&&view.primary==5&&view.bank==6);
    button(0x37,1);button(0x37,0);drain();
    mixer_control_view(session.control,0,&view);
    assert(view.primary==4&&view.bank==6);
    /* Keep the later ownership assertion relative to this verified pair. */
    stock_down[0x36]=stock_up[0x36]=0;
    assert(bank_led[BANK_LED_FIRST+6]==1);
    assert(step_native_browser_bank(-1)==6);
    project.output[97]=2;
    assert(mixer_control_confirm(session.control,&project,5,1));
    mixer_control_presence(session.control,0,UINT64_C(3)<<32,6);
    mixer_publish_route();
    assert(atomic_load(&mixer_select_lights[0])==pad_light(0x102030u,.18f));
    uint64_t other=atomic_load(&mixer_select_lights[1]);
    assert(other==pad_light(0x102031u,.28f)||other==pad_light(MIXER_MUTE_RGB,.90f));
    assert(atomic_load(&mixer_select_lights[2])==0);
    uint64_t colour;
    assert(mixer_leds_light(0,0x123456,&colour)); /* Palette owns unknown/native fallback too. */
    assert(stock_down[0x23]==0&&stock_up[0x23]==0);
    button(0x36,1);drain();
    button(1,1);button(1,0);drain(); /* Return while + is held. */
    button(0x36,0);drain();
    assert(!stock_down[0x36]&&!stock_up[0x36]);
    mixer_control_view(session.control,0,&view);
    assert(view.page==MIXER_OVERVIEW&&view.bank==6&&view.primary==5);
    assert(step_native_browser_bank(-1)==6&&!mixer_leds_light(0,0,&colour));
    pad(4,1);pad(4,0);drain();
    unsigned char knob[16]={0};
    knob[8]=0x10;
    controller(knob);
    drain();
    mixer_control_view(session.control,0,&view);
    assert(view.pending && !stock_cc);
    mixer_control_set_fx_editing(session.control,1);
    button(0,1);button(0,0);drain();
    knob[8]=0x14;knob[9]=115;controller(knob);drain();
    mixer_control_view(session.control,0,&view);
    assert(view.page==MIXER_KNOB_FX&&view.project->track[4].mix.level_db==3&&!stock_cc);
    button(0x2b,1);button(0x2b,0);drain();
    mixer_control_view(session.control,0,&view);
    assert(view.project->track[4].mix.mute&&!stock_down[0x2b]&&!stock_up[0x2b]);
    button(0x31,1);button(0x2b,1);button(0x2b,0);button(0x31,0);drain();
    mixer_control_view(session.control,0,&view);
    assert(!view.project->track[4].mix.mute&&!stock_down[0x2b]&&!stock_up[0x2b]);



    button(7,1);
    button(7,0);
    drain();
    assert(!mixer_control_active(session.control) && stock_down[7]==1 && stock_up[7]==1);
    button(0x2b,1);button(0x2b,0);
    assert(stock_down[0x2b]==1&&stock_up[0x2b]==1);

    button(0x0c,1);
    button(0x0c,0);
    assert(stock_down[0x0c]==1 && stock_up[0x0c]==1);

    /* B1 on the Mixer page leaves to Sample with one stock Sample press. */
    assert(mixer_control_open(session.control,1,0));mixer_publish_route();
    unsigned sample_down=stock_down[6],sample_up=stock_up[6];
    button(0x0c,1);drain();
    assert(!mixer_control_active(session.control)&&stock_down[6]==sample_down+1&&stock_up[6]==sample_up+1);
    assert(bank_led[BANK_LED_FIRST+6]==1);
    button(0x0c,0);drain();assert(stock_down[0x0c]==1&&stock_up[0x0c]==1);

    /* Mode exit remains paired when there is room for only one event. */
    assert(mixer_control_open(session.control,1,0));
    assert(mixer_control_button(session.control,MIXER_PAD_FX,1,mixer_control_ticket(session.control)));
    assert(mixer_control_button(session.control,MIXER_PAD_FX,0,mixer_control_ticket(session.control)));
    mixer_publish_route();
    assert(mixer_leds_light(0,0,&colour));
    struct phone_event dummy={0};
    for(unsigned i=0;i<PHONE_EVENT_CAPACITY-1;++i) assert(phone_events_push(&dummy));
    button(6,1);
    button(6,0);
    assert(stock_down[6]==sample_down+2 && stock_up[6]==sample_up+2);
    drain();
    mixer_tick_ui();
    assert(!mixer_control_active(session.control));
    assert(!mixer_leds_light(0,0,&colour));

    /* Shift+Pad9 is the stock Fader Settings again, on its first down. */
    button(0x31,1);
    pad(8,1);
    drain();
    assert(stock_pad[1]==1 && !mixer_pad_pairs[8]);
    pad(8,0);
    button(0x31,0);
    drain();
    assert(stock_pad[0]==1 && !atomic_load(&native_buttons_down[0x31]));

    fixture_palette_ticket=9;
    pad(14,1);pad(14,1); /* duplicate down must not reserve another up */
    fixture_palette_ticket=0;
    pad(14,0);
    struct phone_event colour_event;
    assert(phone_events_pop(&colour_event)&&colour_event.kind==PHONE_PALETTE_PAD&&
           colour_event.pad==14&&colour_event.down&&colour_event.workflow_ticket==9);
    assert(phone_events_pop(&colour_event)&&colour_event.kind==PHONE_PALETTE_PAD&&
           colour_event.pad==14&&!colour_event.down&&colour_event.workflow_ticket==9);
    assert(!phone_events_pop(&colour_event)&&!palette_pad_pairs[14]);

    fixture_levels=1;fixture_levels_mode=1;
    button(SAMPLE_MODE_BUTTON,1);button(SAMPLE_MODE_BUTTON,1);button(SAMPLE_MODE_BUTTON,0);
    assert(stock_down[LEVELS_MODE_BUTTON]==1&&stock_up[LEVELS_MODE_BUTTON]==1);
    fixture_levels_mode=8;
    button(SAMPLE_MODE_BUTTON,1);button(SAMPLE_MODE_BUTTON,0);
    assert(stock_down[CHOP_MODE_BUTTON]==1&&stock_up[CHOP_MODE_BUTTON]==1);
    modal_open=1;button(SAMPLE_MODE_BUTTON,1);button(SAMPLE_MODE_BUTTON,0);modal_open=0;
    button(SEQ_MODE_BUTTON,1);button(SAMPLE_MODE_BUTTON,1);button(SAMPLE_MODE_BUTTON,0);button(SEQ_MODE_BUTTON,0);
    assert(stock_down[CHOP_MODE_BUTTON]==1&&stock_up[CHOP_MODE_BUTTON]==1);
    fixture_levels=0;

    /* Am MOD uses one Knob FX button, including the Shift Track FX shortcut.
     * No unavailable/fault/held-Pad-FX path may expose stock Knob FX. */
    fixture_fx_mode=1;mixer_control_close(session.control);mixer_publish_route();
    unsigned native_knob_down=stock_down[0],native_knob_up=stock_up[0];
    button(0,1);button(0,0);drain();assert(mixer_control_active(session.control));
    mixer_control_close(session.control);mixer_publish_route();
    button(0x31,1);button(0,1);button(0,0);button(0x31,0);drain();
    mixer_control_view(session.control,0,&view);assert(view.page==MIXER_KNOB_FX);
    mixer_control_close(session.control);mixer_publish_route();
    atomic_store(&native_buttons_down[1],1);button(0,1);button(0,0);drain();
    atomic_store(&native_buttons_down[1],0);assert(mixer_control_active(session.control));
    mixer_control_close(session.control);mixer_publish_route();
    button(0,1);button(0,0);drain();pad(14,1);pad(14,0);drain();
    mixer_control_view(session.control,0,&view);assert(view.master==2&&view.page==MIXER_KNOB_FX);
    mixer_control_close(session.control);mixer_publish_route();
    /* Compressor deliberately has no Mixer shortcut; neither normal nor
     * Shift+KNOB FX may navigate via Sample. */
    atomic_store(&mixer_route,0);atomic_store(&mixer_last_reasons,2u|128u);
    button(0,1);button(0,0);drain();assert(!mixer_control_active(session.control));
    button(0x31,1);button(0,1);button(0,0);button(0x31,0);drain();assert(!mixer_control_active(session.control)&&!mixer_open_after);
    mixer_publish_route();
    atomic_store(&mixer_route,0);atomic_store(&mixer_last_reasons,~0u);
    button(0,1);button(0,0);button(0x31,1);button(0,1);button(0,0);button(0x31,0);drain();
    mixer_publish_route();atomic_store(&mixer_input_fault,1);button(0,1);button(0,0);drain();atomic_store(&mixer_input_fault,0);
    mixer_publish_route();
    for(unsigned i=0;i<PHONE_EVENT_CAPACITY;++i)assert(phone_events_push(&dummy));
    button(0,1);button(0,0);drain();
    assert(stock_down[0]==native_knob_down&&stock_up[0]==native_knob_up);
    puts("PASS Am MOD Knob FX entry/Shift FX page; held Pad FX, unavailable Mixer, input fault and full FIFO never forward native Knob FX edges");

    /* MIDI Config can own the first B2 while no custom page is active.
     * Numeric ownership must queue BOTH edges instead of hitting stock. */
    atomic_store(&mixer_route,0);fixture_music_owned=0x0d;
    unsigned old_down=stock_down[0x0d],old_up=stock_up[0x0d];
    button(0x0d,1);struct phone_event midi_entry;assert(phone_events_pop(&midi_entry));
    assert(midi_entry.kind==PHONE_BUTTON&&(midi_entry.pad&2u)&&midi_entry.native[8]==0x0d&&midi_entry.native[9]);
    button(0x0d,0);assert(phone_events_pop(&midi_entry));assert(midi_entry.kind==PHONE_BUTTON&&(midi_entry.pad&2u)&&!midi_entry.native[9]);
    assert(stock_down[0x0d]==old_down&&stock_up[0x0d]==old_up);fixture_music_owned=~0u;
    puts("PASS cold MIDI Tools entry through production producer: owned B2 down/up queue while custom UI is inactive");

    /* Step Edit Knob FX must bypass both the Stock FX gesture recognizer
     * and Am MOD Mixer entry, retaining both edges for the editor. */
    fixture_step_active=1;fixture_step_owned=0;
    for(unsigned mode=0;mode<2;++mode){
        fixture_fx_mode=mode;unsigned before_down=stock_down[0],before_up=stock_up[0];
        struct phone_event piano;
        button(0,1);assert(phone_events_pop(&piano));assert(piano.kind==PHONE_BUTTON&&(piano.pad&4u)&&piano.native[9]);
        button(0,0);assert(phone_events_pop(&piano));assert(piano.kind==PHONE_BUTTON&&(piano.pad&4u)&&!piano.native[9]);
        assert(stock_down[0]==before_down&&stock_up[0]==before_up);
    }
    fixture_step_active=0;fixture_step_owned=~0u;
    puts("PASS Step Edit Piano Roll Knob FX down/up through production producer in both Stock and Am MOD FX modes");
    fixture_knob_fx=1;unsigned char custom_cc[16]={0};custom_cc[8]=0x12;custom_cc[9]=127;
    unsigned before_cc=stock_cc;fixture_step_cc=0x12;controller(custom_cc);
    struct phone_event owned_cc;assert(phone_events_pop(&owned_cc));assert(owned_cc.kind==PHONE_CONTROLLER&&(owned_cc.pad&4u)&&stock_cc==before_cc);
    fixture_step_cc=~0u;fixture_music_cc=0x12;controller(custom_cc);
    assert(phone_events_pop(&owned_cc));assert(owned_cc.kind==PHONE_CONTROLLER&&(owned_cc.pad&2u)&&stock_cc==before_cc);
    fixture_music_cc=~0u;assert(forward_knob_fx(custom_cc)&&stock_cc==before_cc+1);fixture_knob_fx=0;
    fixture_knob_fx=1;unsigned char handoff[16]={0};handoff[9]=1;
    unsigned fx_down=stock_down[0],fx_up=stock_up[0];close_background_knob_fx(handoff,4);
    assert(stock_down[0]==fx_down+1&&stock_up[0]==fx_up+1);
    handoff[8]=0x0d;close_background_knob_fx(handoff,2);assert(stock_down[0]==fx_down+2&&stock_up[0]==fx_up+2);
    handoff[9]=0;close_background_knob_fx(handoff,2);assert(stock_down[0]==fx_down+2);fixture_knob_fx=0;
    puts("PASS Piano Roll and MIDI Tools retain their knobs over a background stock Knob FX page; ordinary FX controls remain native");

    /* Project change revokes the route. */
    mixer_project_changing();
    drain();
    assert(!mixer_native_pads && !mixer_pad_pairs[8]);
    assert(!atomic_load(&mixer_route) && !mixer_control_active(session.control));
    mixer_control_destroy(session.control);
    /* A visible hidden Chop root is not a browser or custom workflow.
     * Sequence mute controls must work there, while Mixer/browser retain theirs. */
    fixture_workflow_visible=1;fixture_workflow_blocks=0;step_native_browser_bank(5);atomic_store(&native_mute_mode,1);
    assert(!seqmute_badge_active());fixture_sequence_mutes[1]=1ull<<63;assert(seqmute_badge_active());
    fixture_sequence_index=1;assert(!seqmute_badge_active());fixture_sequence_index=128;assert(!seqmute_badge_active());
    fixture_sequence_index=-1;assert(!seqmute_badge_active());fixture_sequence_index=0;fixture_sequence_mutes[1]=0;
    atomic_store(&native_shift_requested,0);atomic_store(&native_buttons_down[0x31],0);atomic_store(&naming_active,0);
    assert(seqmute_button(0x0e,1)&&fixture_seq_variant);assert(seqmute_button(0x0e,0));
    int seq_selected=-1;
    assert(!strcmp(phone_seqmute_header(1,&seq_selected),"Clear Seq")&&!seq_selected);
    assert(!strcmp(phone_seqmute_header(2,&seq_selected),"Seq: On")&&seq_selected);
    atomic_store(&native_shift_requested,1);assert(!phone_seqmute_header(2,&seq_selected));atomic_store(&native_shift_requested,0);
    assert(seqmute_pad(3,1,0)&&fixture_seq_toggles==1);assert(seqmute_pad(3,0,1));
    fixture_workflow_blocks=1;assert(!seqmute_button(0x0e,1));fixture_workflow_blocks=0;
    atomic_store(&mixer_route,MIXER_OPEN);assert(!seqmute_button(0x0e,1));atomic_store(&mixer_route,0);
    assert(seqmute_button(0x0d,1)&&seqmute_button(0x0d,0));
    atomic_store(&native_mute_mode,0);seqmute_tick();assert(!fixture_seq_variant);
    assert(!phone_seqmute_header(2,&seq_selected));
    fixture_workflow_visible=0;
    puts("PASS Sequence Pad Mutes work over hidden Chop root and respect browser/Mixer ownership and paired releases");
    munmap(image,0x7500000);
    puts("PASS Mixer through production input hooks and shared FIFO: PAD FX+KNOB FX entry, lone FX presses native, track select, Assign bank, K1, Seq exit, B1 exit to Sample, stock Shift+Pad9 and project change");
}
