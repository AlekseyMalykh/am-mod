#include "src/mixer_input.h"
#include <assert.h>
#include <stdio.h>
static int submit(void*u,const struct mixer_project*p,uint64_t old,uint64_t*next)
{(void)u;(void)p;*next=old+1;return 1;}
static void gestures(void)
{
    struct mixer_entry g;mixer_entry_init(&g,0);
    assert(mixer_entry_pad(&g,8,1,1000,1,1,0,0)==MIXER_ENTRY_PASS);
    assert(mixer_entry_pad(&g,8,1,1000,1,1,1,1)==MIXER_ENTRY_PASS);
    assert(mixer_entry_pad(&g,8,1,1000,1,0,1,0)==MIXER_ENTRY_PASS);
    assert(mixer_entry_pad(&g,7,1,1000,1,1,1,0)==MIXER_ENTRY_PASS);
    assert(mixer_entry_pad(&g,8,1,1000,1,1,1,0)==MIXER_ENTRY_CONSUME);
    assert(mixer_entry_pad(&g,8,1,1200,1,1,1,0)==MIXER_ENTRY_CONSUME);
    assert(mixer_entry_tick(&g,1499,1,1,0)==MIXER_ENTRY_CONSUME);
    /* The chord is captured on down: releasing Shift first is still short. */
    assert(mixer_entry_pad(&g,8,0,1499,1,0,1,0)==MIXER_ENTRY_OPEN);
    assert(mixer_entry_tick(&g,1500,1,1,0)==MIXER_ENTRY_PASS);
    assert(mixer_entry_pad(&g,8,1,2000,1,1,1,0)==MIXER_ENTRY_CONSUME);
    assert(mixer_entry_tick(&g,2500,1,1,0)==MIXER_ENTRY_LONG_DOWN);
    assert(mixer_entry_tick(&g,3000,1,1,0)==MIXER_ENTRY_PASS);
    mixer_entry_cancel(&g);
    assert(mixer_entry_pad(&g,8,0,3000,2,0,0,1)==MIXER_ENTRY_NATIVE_UP);
    assert(mixer_entry_pad(&g,8,1,4000,2,1,1,0)==MIXER_ENTRY_CONSUME);
    assert(mixer_entry_pad(&g,8,0,4500,2,0,1,0)==MIXER_ENTRY_LONG_PAIR);
    assert(mixer_entry_tick(&g,4501,2,1,0)==MIXER_ENTRY_PASS);
    assert(mixer_entry_pad(&g,8,1,5000,2,1,1,0)==MIXER_ENTRY_CONSUME);
    assert(mixer_entry_tick(&g,5200,3,1,0)==MIXER_ENTRY_CANCELLED);
    assert(mixer_entry_pad(&g,8,0,5600,3,1,1,0)==MIXER_ENTRY_CONSUME);
    assert(mixer_entry_pad(&g,8,1,6000,3,1,1,0)==MIXER_ENTRY_CONSUME);
    assert(mixer_entry_tick(&g,6100,3,0,0)==MIXER_ENTRY_CANCELLED);
    assert(mixer_entry_pad(&g,8,0,6600,3,1,1,0)==MIXER_ENTRY_CONSUME);
    assert(mixer_entry_pad(&g,8,1,7000,3,1,1,0)==MIXER_ENTRY_CONSUME);
    mixer_entry_cancel(&g);
    assert(mixer_entry_pad(&g,8,0,7001,3,1,1,0)==MIXER_ENTRY_CONSUME);
    /* Unsigned elapsed time remains correct near UINT64_MAX; no deadline wrap. */
    assert(mixer_entry_pad(&g,8,1,UINT64_MAX-600,3,1,1,0)==MIXER_ENTRY_CONSUME);
    assert(mixer_entry_tick(&g,UINT64_MAX-100,3,1,0)==MIXER_ENTRY_LONG_DOWN);
    assert(mixer_entry_pad(&g,8,0,UINT64_MAX,3,1,1,0)==MIXER_ENTRY_NATIVE_UP);
    puts("PASS short/long entry exclusivity, exact threshold, delayed FIFO tick, Shift-first release, cancellation, stale ticket and native down/up pairing");
}
static struct mixer_view_state view(struct mixer_control*c)
{struct mixer_view_state v;mixer_control_view(c,0,&v);return v;}
static void keys(void)
{
    struct mixer_project p;mixer_defaults(&p);struct mixer_control_ops ops={NULL,submit};
    struct mixer_control*c=mixer_control_create(&ops,48000);assert(c);
    assert(mixer_control_confirm(c,&p,1,1));assert(mixer_control_open(c,1,0));
    uint64_t t=mixer_control_ticket(c);
    for(unsigned code=0x0c;code<=0x0e;++code){
        assert(mixer_input_button(c,code,1,t));assert(view(c).page==(enum mixer_page)(code-0x0b));
        assert(mixer_input_button(c,code,0,t));
    }
    assert(mixer_input_button(c,1,1,t)&&view(c).page==MIXER_ASSIGN);
    assert(mixer_input_button(c,1,0,t));
    assert(mixer_input_button(c,0x23,1,t)&&view(c).bank==1);
    assert(mixer_input_button(c,0x23,0,t));
    assert(mixer_input_button(c,0x37,1,t)&&view(c).bank==0);
    assert(mixer_input_button(c,0x37,0,t));
    assert(mixer_input_button(c,0x36,1,t)&&view(c).bank==1);
    assert(mixer_input_button(c,0,1,t)&&view(c).page==MIXER_KNOB_FX);
    assert(mixer_input_button(c,0x36,0,t)); /* Captured up after changing tabs. */
    assert(!mixer_input_button(c,0x36,1,t));
    assert(mixer_input_button(c,0x6f,1,t)&&view(c).pending);
    assert(mixer_input_button(c,0x6f,0,t));
    /* Sample closes before forwarding; late FX release remains consumed. */
    assert(!mixer_input_button(c,6,1,t)&&!mixer_control_active(c));
    assert(mixer_input_button(c,0,0,t));assert(!mixer_input_button(c,0,1,t));
    assert(!mixer_input_button(c,0x51,1,t));assert(!mixer_input_button(c,0x52,1,t));
    assert(mixer_control_open(c,1,0));t=mixer_control_ticket(c);
    assert(!mixer_input_button(c,0x31,1,t));
    assert(!mixer_input_button(c,7,1,t)&&!mixer_control_active(c));
    assert(!mixer_input_button(c,0x31,0,t));
    mixer_control_destroy(c);puts("PASS pinned raw button IDs route tabs/FX; mode exit forwards; outside Mixer stock buttons/transport/Shift remain forwarded");
}
int main(void){gestures();keys();return 0;}
