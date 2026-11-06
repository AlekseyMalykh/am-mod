#include "src/mixer_control.h"
#include "src/fx_catalog.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static struct mixer_project accepted;
static uint64_t revision=1,epoch=10;
static unsigned submissions,busy;
static int submit(void*u,const struct mixer_project*p,uint64_t expected,uint64_t*next)
{(void)u;if(busy)return 0;assert(expected==revision);accepted=*p;*next=++revision;++submissions;return 1;}
static struct mixer_view_state view(struct mixer_control*c){struct mixer_view_state s;mixer_control_view(c,1000,&s);return s;}
static void button(struct mixer_control*c,enum mixer_action a){uint64_t t=mixer_control_ticket(c);assert(mixer_control_button(c,a,1,t));assert(mixer_control_button(c,a,0,t));}
static void cc(struct mixer_control*c,unsigned k,unsigned v){assert(mixer_control_cc(c,k,v,mixer_control_ticket(c)));}
static void pad(struct mixer_control*c,unsigned p,int down){assert(mixer_control_pad(c,p,down,mixer_control_ticket(c)));}
static void ack(struct mixer_control*c){mixer_control_tick(c);assert(mixer_control_confirm(c,&accepted,revision,epoch));}
static void colour_controls(void)
{
    mixer_defaults(&accepted);++revision;++epoch;struct mixer_control_ops ops={NULL,submit};
    struct mixer_control*c=mixer_control_create(&ops,48000);assert(c);
    assert(mixer_control_confirm(c,&accepted,revision,epoch)&&mixer_control_open(c,1,0));button(c,MIXER_B1);
    for(unsigned index=0;index<MIXER_COLOURS;++index){
        struct mixer_project expected=accepted;expected.track[0].colour=mixer_colour_at(index);
        cc(c,0x12,(index*128+MIXER_COLOURS-1)/MIXER_COLOURS);ack(c);
        assert(!memcmp(&accepted,&expected,sizeof(expected))&&view(c).cursor==5&&view(c).scroll==1);
        unsigned unchanged=submissions;cc(c,0x12,(index*128+MIXER_COLOURS-1)/MIXER_COLOURS);mixer_control_tick(c);assert(submissions==unchanged);
    }
    accepted.track[0].colour=0x123456;++revision;assert(mixer_control_confirm(c,&accepted,revision,epoch));
    unsigned unchanged=submissions;button(c,MIXER_B1);button(c,MIXER_B1);mixer_control_tick(c);
    assert(accepted.track[0].colour==0x123456&&submissions==unchanged);
    cc(c,0x64,5);assert(view(c).cursor==5&&view(c).scroll==1);button(c,MIXER_SELECT);assert(view(c).colour_edit);
    cc(c,0x64,127);ack(c);assert(accepted.track[0].colour==mixer_colour_at(MIXER_COLOURS-1));
    unchanged=submissions;cc(c,0x64,1);mixer_control_tick(c);assert(submissions==unchanged);
    cc(c,0x64,117);ack(c);assert(accepted.track[0].colour==mixer_colour_at(0));
    cc(c,0x64,127);mixer_control_tick(c);assert(accepted.track[0].colour==mixer_colour_at(0));
    button(c,MIXER_SELECT);assert(!view(c).colour_edit);cc(c,0x64,123);assert(view(c).cursor==0&&!view(c).scroll);
    cc(c,0x64,5);button(c,MIXER_SELECT);button(c,MIXER_PAD_FX);assert(!view(c).colour_edit);
    button(c,MIXER_PAD_FX);assert(view(c).cursor==5&&view(c).scroll==1&&!view(c).colour_edit);
    pad(c,0,1);pad(c,1,1);pad(c,0,0);pad(c,1,0);unchanged=submissions;
    cc(c,0x12,127);button(c,MIXER_SELECT);mixer_control_tick(c);
    assert(submissions==unchanged&&!view(c).colour_edit&&!strcmp(mixer_control_message(c),"Select one Track"));
    pad(c,15,1);pad(c,15,0);struct mixer_project expected=accepted;expected.master.colour=mixer_colour_at(11);
    cc(c,0x12,127);ack(c);assert(!memcmp(&accepted,&expected,sizeof(expected)));
    uint64_t ticket=mixer_control_ticket(c);assert(!mixer_control_button(c,MIXER_SHIFT,1,ticket));
    button(c,MIXER_PAD_FX);assert(!mixer_control_button(c,MIXER_SHIFT,0,ticket));
    unchanged=submissions;cc(c,0x12,0);mixer_control_tick(c);assert(view(c).page==MIXER_INSPECT&&submissions==unchanged);
    button(c,MIXER_PAD_FX);button(c,MIXER_SELECT);assert(view(c).colour_edit);
    assert(!mixer_control_button(c,MIXER_NATIVE_MODE,1,ticket));assert(!view(c).colour_edit);
    mixer_control_destroy(c);
    puts("PASS twelve user colours, no-op edits, preserved custom RGB, scroll/focus/navigation, group guard, independent Master and read-only Inspect");
}
static void fx_slots(void)
{
    mixer_defaults(&accepted);++revision;++epoch;struct mixer_control_ops ops={NULL,submit};
    struct mixer_control*c=mixer_control_create(&ops,48000);assert(c);
    assert(mixer_control_confirm(c,&accepted,revision,epoch)&&mixer_control_open(c,1,0));
    button(c,MIXER_B2);cc(c,0x64,4);button(c,MIXER_SELECT);assert(view(c).fx_slot==4&&view(c).fx_catalog);
    button(c,MIXER_SELECT);ack(c);assert(accepted.track[0].fx.count==1&&view(c).fx_slot==0&&!view(c).fx_catalog);
    pad(c,1,1);pad(c,1,0);assert(view(c).fx_catalog);
    pad(c,0,1);pad(c,0,0);assert(!view(c).fx_catalog);
    mixer_control_destroy(c);puts("PASS empty FX slot appends at actual chain position; editor refreshes after Track change");
}
int main(void)
{
    mixer_defaults(&accepted);accepted.track[0].mix.level_db=-3;accepted.track[1].mix.level_db=-9;accepted.track[2].mix.level_db=-6;
    struct mixer_control_ops ops={NULL,submit};struct mixer_control*c=mixer_control_create(&ops,48000);assert(c);
    assert(!mixer_control_open(c,0,0));assert(mixer_control_confirm(c,&accepted,revision,epoch));assert(!mixer_control_open(c,1,1));assert(mixer_control_open(c,1,0));
    button(c,MIXER_B1);assert(view(c).page==MIXER_MIX);button(c,MIXER_B2);assert(view(c).page==MIXER_FX);button(c,MIXER_B2);assert(view(c).page==MIXER_OVERVIEW);
    button(c,MIXER_B3);button(c,MIXER_PAD_FX);assert(view(c).page==MIXER_ASSIGN);button(c,MIXER_KNOB);assert(view(c).page==MIXER_KNOB_FX);
    button(c,MIXER_KNOB);assert(view(c).page==MIXER_ROUTE);button(c,MIXER_PAD_FX);button(c,MIXER_B1);button(c,MIXER_B1);assert(view(c).page==MIXER_OVERVIEW);
    pad(c,0,1);pad(c,1,1);pad(c,2,1);pad(c,1,0);pad(c,0,0);pad(c,2,0);assert(view(c).selected==7&&view(c).primary==0);
    cc(c,0x14,1);assert(!submissions&&!strcmp(mixer_control_message(c),"PICKUP"));cc(c,0x14,98);ack(c);
    float before[3];for(unsigned i=0;i<3;++i)before[i]=accepted.track[i].mix.level_db;
    cc(c,0x14,106);ack(c);for(unsigned i=1;i<3;++i)assert(fabsf((accepted.track[i].mix.level_db-accepted.track[0].mix.level_db)-(before[i]-before[0]))<1e-6f);
    cc(c,0x14,127);ack(c);assert(accepted.track[0].mix.level_db<=6.001f);
    uint64_t ticket=mixer_control_ticket(c);assert(!mixer_control_button(c,MIXER_SHIFT,1,ticket));button(c,MIXER_B1);ack(c);assert(accepted.track[0].mix.mute&&accepted.track[1].mix.mute&&accepted.track[2].mix.mute);
    button(c,MIXER_B3);ack(c);assert(accepted.track[0].mix.solo&&accepted.track[2].mix.solo);assert(!mixer_control_button(c,MIXER_SHIFT,0,ticket));
    pad(c,15,1);pad(c,15,0);assert(view(c).master);button(c,MIXER_B1);assert(view(c).page==MIXER_MIX);pad(c,0,1);pad(c,0,0);assert(!view(c).master&&view(c).selected==1);
    float gain=accepted.track[0].mix.level_db;cc(c,0x10,0);ack(c);assert(accepted.track[0].mix.pan==-1&&accepted.track[0].mix.level_db==gain);
    puts("PASS controller tabs/return context, held-pad groups, Master, relative Fader balance/pickup, group Mute/Solo and K1 Pan without Volume");
    mixer_control_presence(c,1,UINT64_C(1)<<63,7);button(c,MIXER_PAD_FX);pad(c,15,1);pad(c,15,0);
    assert(view(c).project->output[127]==0&&view(c).pending);uint32_t led;assert(mixer_control_led(c,15,&led)&&led==0x181818);ack(c);
    assert(accepted.output[127]==1&&mixer_control_led(c,15,&led)&&led==accepted.track[0].colour);pad(c,14,1);pad(c,14,0);assert(!strcmp(mixer_control_message(c),"Empty pad"));
    unsigned bank_submissions=submissions;
    button(c,MIXER_BANK);assert(view(c).bank==0&&view(c).sound==14&&view(c).primary==0);
    button(c,MIXER_BANK_PREVIOUS);assert(view(c).bank==7&&view(c).sound==126);
    mixer_control_button(c,MIXER_SHIFT,1,ticket);button(c,MIXER_BANK);
    mixer_control_button(c,MIXER_SHIFT,0,ticket);assert(view(c).bank==6);
    button(c,MIXER_BANK_NEXT);assert(view(c).bank==7);
    cc(c,0x12,0);assert(view(c).bank==0&&view(c).sound==14);
    cc(c,0x12,127);assert(view(c).bank==7&&view(c).sound==126);
    mixer_control_tick(c);assert(submissions==bank_submissions);
    button(c,MIXER_PAD_FX);button(c,MIXER_KNOB);assert(view(c).fx_catalog);cc(c,0x10,0);button(c,MIXER_SELECT);ack(c);assert(accepted.track[0].fx.count==1&&accepted.track[0].fx.stage[0].kind==FX_STAGE_EQ);
    accepted.track[0].fx.stage[0].eq.count=8;for(unsigned i=4;i<8;++i)accepted.track[0].fx.stage[0].eq.band[i]=accepted.track[0].fx.stage[0].eq.band[0];
    ++revision;assert(mixer_control_confirm(c,&accepted,revision,epoch));
    cc(c,0x64,7);cc(c,0x11,90);ack(c);assert(accepted.track[0].fx.stage[0].eq.band[7].gain_db>0);button(c,MIXER_SELECT);ack(c);assert(accepted.track[0].fx.stage[0].bypass);
    ticket=mixer_control_ticket(c);mixer_control_button(c,MIXER_SHIFT,1,ticket);cc(c,0x10,127);cc(c,0x11,0);cc(c,0x12,127);ack(c);
    assert(accepted.track[0].fx.stage[0].eq.band[7].type==FX_EQ_NOTCH&&!accepted.track[0].fx.stage[0].eq.band[7].enabled&&accepted.track[0].fx.stage[0].eq.output_db==12);
    button(c,MIXER_SELECT);ack(c);assert(!accepted.track[0].fx.count&&view(c).fx_catalog);mixer_control_button(c,MIXER_SHIFT,0,ticket);
    cc(c,0x10,(5*128+fx_catalog_count()-1)/fx_catalog_count());button(c,MIXER_SELECT);ack(c);
    assert(accepted.track[0].fx.stage[0].native_id==0x61465276);cc(c,0x64,5);cc(c,0x12,127);ack(c);assert(accepted.track[0].fx.stage[0].parameter[17]==1);
    unsigned unchanged=submissions;mixer_control_button(c,MIXER_SHIFT,1,ticket);button(c,MIXER_PAD_FX);mixer_control_button(c,MIXER_SHIFT,0,ticket);
    assert(view(c).page==MIXER_INSPECT);cc(c,0x10,127);cc(c,0x14,127);pad(c,15,1);pad(c,15,0);mixer_control_tick(c);assert(submissions==unchanged);
    button(c,MIXER_BANK_NEXT);assert(view(c).bank==0&&view(c).sound==15);
    button(c,MIXER_BANK_PREVIOUS);assert(view(c).bank==7&&view(c).sound==127);
    mixer_control_tick(c);assert(submissions==unchanged);
    button(c,MIXER_PAD_FX);assert(view(c).page==MIXER_MIX);
    accepted.track[2].mix.level_db=-INFINITY;++revision;assert(mixer_control_confirm(c,&accepted,revision,epoch));
    pad(c,0,1);pad(c,2,1);pad(c,0,0);pad(c,2,0);cc(c,0x14,127);cc(c,0x14,103);ack(c);
    assert(isinf(accepted.track[2].mix.level_db)&&accepted.track[2].mix.level_db<0);
    pad(c,2,1);pad(c,2,0);cc(c,0x14,0);cc(c,0x14,70);ack(c);assert(isfinite(accepted.track[2].mix.level_db));
    accepted.track[2].mix.level_db=accepted.track[3].mix.level_db=-INFINITY;++revision;assert(mixer_control_confirm(c,&accepted,revision,epoch));
    pad(c,2,1);pad(c,3,1);pad(c,2,0);pad(c,3,0);unchanged=submissions;cc(c,0x14,60);mixer_control_tick(c);
    assert(submissions==unchanged&&strstr(mixer_control_message(c),"Off"));pad(c,0,1);pad(c,0,0);
    puts("PASS all-bank occupied-only routing, confirmed-only LEDs, shared FX model, eight-band EQ controls and bypass");
    busy=1;cc(c,0x10,127);mixer_control_tick(c);assert(view(c).pending);busy=0;ack(c);
    cc(c,0x10,64);mixer_control_tick(c);cc(c,0x10,12);assert(mixer_control_confirm(c,&accepted,revision,epoch));ack(c);
    assert(mixer_project_valid(&accepted));
    ticket=mixer_control_ticket(c);assert(mixer_control_button(c,MIXER_B1,1,ticket));enum mixer_page once=view(c).page;
    assert(mixer_control_button(c,MIXER_B1,1,ticket)&&view(c).page==once);pad(c,3,1);
    assert(!mixer_control_button(c,MIXER_NATIVE_MODE,1,ticket)&&!mixer_control_active(c));assert(mixer_control_ticket(c)!=ticket);
    assert(mixer_control_button(c,MIXER_B1,0,ticket));assert(mixer_control_pad(c,3,0,ticket));assert(!mixer_control_button(c,MIXER_B1,1,mixer_control_ticket(c)));assert(!mixer_control_cc(c,0x10,1,ticket));
    assert(mixer_control_open(c,1,0));ticket=mixer_control_ticket(c);++epoch;++revision;assert(mixer_control_confirm(c,&accepted,revision,epoch)&&!mixer_control_active(c));assert(mixer_control_ticket(c)!=ticket);
    mixer_control_destroy(c);puts("PASS coalesced confirmed edits, busy retry, mode exit, stale tickets/paired release recovery and Load/New epoch invalidation");colour_controls();fx_slots();return 0;
}
