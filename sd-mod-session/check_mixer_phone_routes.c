#define PHONE_RGB_TEST 1
#define MPC_MIXER_LIVE 1
#include "src/phone_native.c"
#include <assert.h>

struct mixer_session { struct mixer_control *control; };
struct mixer_control *mixer_session_control(struct mixer_session *s) { return s ? s->control : NULL; }
int mixer_session_status(const struct mixer_session *s) { return s ? 1 : -2; }
void mixer_session_ui_tick(struct mixer_session *s) { (void)s; }
void note_native_repaint_sample(void) {}
void note_native_repaint_sample_region(unsigned x,unsigned y,unsigned w,unsigned h)
{ assert(!x && y==102 && w==320 && h==93); }
int note_native_sample_visible(void) { return 1; }
int workflow_blocks_sample_controls(void) { return 0; }
int note_native_active(void) { return 0; }
int music_ui_active(void) { return 0; }
int step_native_active(void) { return 0; }
int workflow_active(void) { return 0; }
int music_ui_exit_saving(void) { return 0; }
int note_native_owns(unsigned c) { (void)c; return 0; }
int music_ui_owns(unsigned c) { (void)c; return 0; }
int step_native_owns(unsigned c) { (void)c; return 0; }
int music_ui_button_owns(unsigned c) { (void)c; return 0; }
int step_native_button_owns(unsigned c) { (void)c; return 0; }
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
static void *top_modal(int i) { assert(!i); return NULL; }
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

    struct mixer_project project;
    mixer_defaults(&project);
    struct mixer_control_ops ops={NULL,submit};
    struct mixer_session session={mixer_control_create(&ops,48000)};
    assert(session.control && mixer_control_confirm(session.control,&project,1,1));
    mixer_ui_session=&session;
    mixer_binding=1;
    mixer_entry_init(&mixer_entry,500);
    mixer_publish_route();

    button(0x31,1);
    pad(8,1);
    button(0x31,0);
    pad(8,0);
    pad(4,1);
    pad(4,0);
    button(0x0c,1);
    drain();
    struct mixer_view_state view;
    mixer_control_view(session.control,0,&view);
    assert(view.page==MIXER_MIX && view.primary==4 && !stock_pad[0] && !stock_pad[1]);
    unsigned char knob[16]={0};
    knob[8]=0x10;
    controller(knob);
    drain();
    mixer_control_view(session.control,0,&view);
    assert(view.pending && !stock_cc);

    button(7,1);
    button(7,0);
    button(0x0c,0);
    drain();
    assert(!mixer_control_active(session.control) && stock_down[7]==1 && stock_up[7]==1);
    assert(!stock_down[0x0c] && !stock_up[0x0c]);
    button(0x0c,1);
    button(0x0c,0);
    assert(stock_down[0x0c]==1 && stock_up[0x0c]==1);

    /* Mode exit remains paired when there is room for only one event. */
    assert(mixer_control_open(session.control,1,0));
    mixer_publish_route();
    struct phone_event dummy={0};
    for(unsigned i=0;i<PHONE_EVENT_CAPACITY-1;++i) assert(phone_events_push(&dummy));
    button(6,1);
    button(6,0);
    assert(stock_down[6]==1 && stock_up[6]==1);
    drain();
    mixer_tick_ui();
    assert(!mixer_control_active(session.control));

    button(0x31,1);
    pad(8,1);
    button(0x31,0);
    drain();
    mixer_entry.started_ms=now_ms()-500;
    mixer_tick_ui();
    assert(stock_pad[1]==1 && !stock_pad[0] && !atomic_load(&native_buttons_down[0x31]));

    /* Invalidation and a saturated FIFO must still deliver the native up. */
    for(unsigned i=0;i<PHONE_EVENT_CAPACITY-1;++i) assert(phone_events_push(&dummy));
    assert(!phone_events_push(&dummy));
    mixer_project_changing();
    pad(8,0);
    assert(phone_events_pending()==PHONE_EVENT_CAPACITY);
    drain();
    assert(stock_pad[0]==1 && !mixer_native_pads && !mixer_pad_pairs[8]);
    assert(!atomic_load(&mixer_route) && !mixer_control_active(session.control));
    mixer_control_destroy(session.control);
    munmap(image,0x7500000);
    puts("PASS Mixer through production input hooks and shared FIFO: short entry, track/tab, K1, Seq exit, late up, long replay and reserved up during project change");
}
