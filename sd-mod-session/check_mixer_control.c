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
    assert(mixer_control_confirm(c,&accepted,revision,epoch)&&mixer_control_open(c,1,0));
    /* Defaults: every Track Yellow, Master green. */
    for(unsigned i=0;i<8;++i)assert(accepted.track[i].colour==mixer_colour_at(MIXER_DEFAULT_TRACK_COLOUR));
    assert(accepted.master.colour==0x00c700);
    for(unsigned index=0;index<MIXER_COLOURS;++index){cc(c,0x12,index*128/MIXER_COLOURS+1);ack(c);
        assert(accepted.track[0].colour==mixer_colour_at(index)&&view(c).cursor==2);}
    unsigned unchanged=submissions;cc(c,0x12,(MIXER_COLOURS-1)*128/MIXER_COLOURS+1);mixer_control_tick(c);assert(submissions==unchanged);
    /* Encoder follows the last moved control: colour steps. */
    cc(c,0x64,127);ack(c);assert(accepted.track[0].colour==mixer_colour_at(MIXER_COLOURS-2));
    /* K3 on a group colours every selected Track. */
    pad(c,1,1);pad(c,2,1);pad(c,1,0);pad(c,2,0);cc(c,0x12,0);ack(c);
    assert(accepted.track[1].colour==mixer_colour_at(0)&&accepted.track[2].colour==mixer_colour_at(0)&&accepted.track[0].colour==mixer_colour_at(MIXER_COLOURS-2));
    /* Master colour is fixed. */
    pad(c,15,1);pad(c,15,0);unchanged=submissions;cc(c,0x12,64);mixer_control_tick(c);
    assert(submissions==unchanged&&accepted.master.colour==0x00c700&&!strcmp(mixer_control_message(c),"Master colour is fixed"));
    /* Pad Bank reset: Level 0 dB, Pan centre, Colour Yellow. */
    pad(c,1,1);pad(c,1,0);cc(c,0x14,1);cc(c,0x10,0);ack(c);assert(accepted.track[1].mix.pan==-1&&accepted.track[1].mix.level_db<-50);
    assert(mixer_control_reset(c,0x14,mixer_control_ticket(c))&&mixer_control_reset(c,0x10,mixer_control_ticket(c))&&mixer_control_reset(c,0x12,mixer_control_ticket(c)));ack(c);
    assert(accepted.track[1].mix.level_db==0&&accepted.track[1].mix.pan==0&&accepted.track[1].colour==mixer_colour_at(MIXER_DEFAULT_TRACK_COLOUR));
    mixer_control_destroy(c);
    puts("PASS Yellow/green defaults, absolute K3 colour for the selection, encoder follows last control, fixed Master colour and Pad Bank defaults");
}
static void fx_slots(void)
{
    mixer_defaults(&accepted);++revision;++epoch;struct mixer_control_ops ops={NULL,submit};
    struct mixer_control*c=mixer_control_create(&ops,48000);assert(c);
    assert(mixer_control_confirm(c,&accepted,revision,epoch)&&mixer_control_open(c,1,0));
    mixer_control_set_fx_editing(c,1);
    button(c,MIXER_KNOB);assert(!view(c).fx_catalog);
    button(c,MIXER_SELECT);assert(view(c).fx_catalog);
    button(c,MIXER_SELECT);ack(c);
    assert(accepted.track[0].fx.stage[0].eq.count==1);
    uint64_t ticket=mixer_control_ticket(c);
    mixer_control_button(c,MIXER_SHIFT,1,ticket);
    button(c,MIXER_B3);
    mixer_control_button(c,MIXER_SHIFT,0,ticket);
    ack(c);assert(accepted.track[0].fx.stage[0].eq.count==1);
    button(c,MIXER_SELECT);assert(view(c).fx_parameters);
    mixer_control_button(c,MIXER_SHIFT,1,ticket);
    for(unsigned i=0;i<3;++i)button(c,MIXER_B3);
    mixer_control_button(c,MIXER_SHIFT,0,ticket);
    cc(c,0x11,100);
    button(c,MIXER_B2);
    button(c,MIXER_B1);assert(!view(c).fx_parameters);
    button(c,MIXER_B2);ack(c);
    struct fx_stage copied=accepted.track[0].fx.stage[0];
    assert(copied.eq.count==3&&copied.eq.band[2].gain_db>0&&copied.bypass);
    pad(c,1,1);pad(c,1,0);assert(!view(c).fx_catalog);
    for(unsigned slot=0;slot<4;++slot){
        if(slot)cc(c,0x64,1);
        assert(view(c).fx_slot==slot);
        button(c,MIXER_B3);ack(c);
        assert(accepted.track[1].fx.count==slot+1);
        const struct fx_stage*pasted=&accepted.track[1].fx.stage[slot];
        assert(pasted->eq.count==3&&pasted->eq.band[2].gain_db==copied.eq.band[2].gain_db&&pasted->bypass);
    }
    button(c,MIXER_B3);ack(c);
    assert(accepted.track[1].fx.count==4);
    cc(c,0x64,127);assert(view(c).fx_slot==2);
    button(c,MIXER_SELECT);cc(c,0x11,0);button(c,MIXER_B1);ack(c);
    mixer_control_button(c,MIXER_SHIFT,1,ticket);cc(c,0x64,1);
    mixer_control_button(c,MIXER_SHIFT,0,ticket);ack(c);
    assert(view(c).fx_slot==3);
    assert(accepted.track[1].fx.stage[3].eq.band[0].gain_db==-18);
    assert(accepted.track[1].fx.stage[2].eq.band[0].gain_db==copied.eq.band[0].gain_db);
    button(c,MIXER_ERASE);assert(view(c).fx_delete);
    cc(c,0x64,127);assert(view(c).fx_slot==3);
    button(c,MIXER_B1);assert(!view(c).fx_delete);
    assert(accepted.track[1].fx.count==4);
    button(c,MIXER_ERASE);button(c,MIXER_B3);ack(c);
    assert(accepted.track[1].fx.count==3&&!view(c).fx_delete);
    mixer_control_button(c,MIXER_SHIFT,1,ticket);button(c,MIXER_B2);
    mixer_control_button(c,MIXER_SHIFT,0,ticket);
    ack(c);assert(accepted.track[1].fx_bypass==1&&!accepted.track[1].mix.mute);
    button(c,MIXER_B1);assert(view(c).page==MIXER_OVERVIEW);
    pad(c,8,1);pad(c,8,0);assert(view(c).fx_parameters&&view(c).fx_slot==0);
    unsigned bank=view(c).bank;
    button(c,MIXER_BANK);assert(view(c).bank==bank);
    button(c,MIXER_KNOB);assert(view(c).page==MIXER_OVERVIEW&&!view(c).fx_parameters);
    pad(c,11,1);pad(c,11,0);assert(view(c).fx_catalog&&view(c).fx_slot==3);
    button(c,MIXER_B1);assert(!view(c).fx_catalog);

    mixer_control_destroy(c);puts("PASS four Mixer FX slots, copy/paste, three EQ points, deletion and chain bypass");
}
static void send_name_history(void)
{
    mixer_defaults(&accepted);++revision;++epoch;
    struct mixer_control_ops ops={NULL,submit};
    struct mixer_control*c=mixer_control_create(&ops,48000);assert(c);
    assert(mixer_control_confirm(c,&accepted,revision,epoch)&&mixer_control_open(c,1,0));
    unsigned bank=view(c).bank;
    button(c,MIXER_BANK);assert(view(c).bank==bank);
    uint64_t ticket=mixer_control_ticket(c);
    cc(c,0x11,127);ack(c);assert(accepted.track[0].mix.send==1);
    mixer_control_button(c,MIXER_SHIFT,1,ticket);
    cc(c,0x11,32);ack(c);assert(accepted.send_to[0]==2);
    button(c,MIXER_B2);assert(view(c).name_edit);
    mixer_control_button(c,MIXER_SHIFT,0,ticket);
    cc(c,0x64,1);button(c,MIXER_B3);ack(c);assert(!strcmp(accepted.track[0].name,"Urack A"));
    mixer_control_button(c,MIXER_SHIFT,1,ticket);
    button(c,MIXER_BANK_PREVIOUS);ack(c);assert(!strcmp(accepted.track[0].name,"Track A"));
    button(c,MIXER_BANK_NEXT);ack(c);assert(!strcmp(accepted.track[0].name,"Urack A"));
    mixer_control_destroy(c);
}
int main(void)
{
    mixer_defaults(&accepted);accepted.track[0].mix.level_db=-3;accepted.track[1].mix.level_db=-9;accepted.track[2].mix.level_db=-6;
    struct mixer_control_ops ops={NULL,submit};struct mixer_control*c=mixer_control_create(&ops,48000);assert(c);
    assert(!mixer_control_open(c,0,0));assert(mixer_control_confirm(c,&accepted,revision,epoch));assert(!mixer_control_open(c,1,1));assert(mixer_control_open(c,1,0));
    assert(view(c).page==MIXER_OVERVIEW);
    /* PAD FX / KNOB FX pages: B1 or the same button returns to the Mixer. */
    button(c,MIXER_PAD_FX);assert(view(c).page==MIXER_ASSIGN);button(c,MIXER_B1);assert(view(c).page==MIXER_OVERVIEW&&mixer_control_active(c));
    button(c,MIXER_PAD_FX);button(c,MIXER_PAD_FX);assert(view(c).page==MIXER_OVERVIEW);
    button(c,MIXER_KNOB);assert(view(c).page==MIXER_KNOB_FX);button(c,MIXER_B1);assert(view(c).page==MIXER_OVERVIEW);
    /* Held pads group; Shift adds/removes one Track; the last stays. */
    pad(c,0,1);pad(c,1,1);pad(c,2,1);pad(c,1,0);pad(c,0,0);pad(c,2,0);assert(view(c).selected==7&&view(c).primary==0);
    uint64_t ticket=mixer_control_ticket(c);mixer_control_button(c,MIXER_SHIFT,1,ticket);
    pad(c,1,1);pad(c,1,0);assert(view(c).selected==5);pad(c,4,1);pad(c,4,0);assert(view(c).selected==0x15&&view(c).primary==4);
    pad(c,0,1);pad(c,0,0);pad(c,2,1);pad(c,2,0);assert(view(c).selected==0x10);pad(c,4,1);pad(c,4,0);assert(view(c).selected==0x10);
    pad(c,0,1);pad(c,0,0);pad(c,2,1);pad(c,2,0);mixer_control_button(c,MIXER_SHIFT,0,ticket);assert(view(c).selected==0x15);
    /* The first position anchors the group without jumping. */
    unsigned fader_submissions=submissions;
    cc(c,0x14,103);mixer_control_tick(c);assert(submissions==fader_submissions&&view(c).cursor==0);
    cc(c,0x14,115);ack(c);
    assert(accepted.track[0].mix.level_db==0&&accepted.track[2].mix.level_db==-3&&accepted.track[4].mix.level_db==3);
    cc(c,0x14,127);ack(c);
    assert(accepted.track[0].mix.level_db==3&&accepted.track[2].mix.level_db==0&&accepted.track[4].mix.level_db==6);
    assert(accepted.track[1].mix.level_db==-9);
    mixer_control_button(c,MIXER_SHIFT,1,ticket);
    cc(c,0x14,115);ack(c);
    assert(accepted.track[0].mix.level_db==3&&accepted.track[2].mix.level_db==3&&accepted.track[4].mix.level_db==3);
    mixer_control_button(c,MIXER_SHIFT,0,ticket);
    fader_submissions=submissions;
    cc(c,0x14,116);mixer_control_tick(c);assert(submissions==fader_submissions);
    cc(c,0x14,120);ack(c);
    assert(accepted.track[0].mix.level_db==4&&accepted.track[2].mix.level_db==4&&accepted.track[4].mix.level_db==4);
    mixer_control_button(c,MIXER_SHIFT,1,ticket);
    cc(c,0x14,0);ack(c);assert(isinf(accepted.track[2].mix.level_db));
    mixer_control_button(c,MIXER_SHIFT,0,ticket);
    /* Encoder changes the last moved parameter (Level) for the selection. */
    cc(c,0x64,1);ack(c);assert(accepted.track[0].mix.level_db==-60&&accepted.track[2].mix.level_db==-60);
    /* K1: one absolute Pan; encoder follows Pan. */
    cc(c,0x10,0);ack(c);assert(accepted.track[0].mix.pan==-1&&accepted.track[4].mix.pan==-1&&view(c).cursor==1);
    cc(c,0x64,10);ack(c);assert(fabsf(accepted.track[2].mix.pan+.8f)<1e-5f);
    cc(c,0x11,99);ack(c);assert(accepted.track[0].mix.send==99/127.f&&view(c).cursor==3);
    unsigned unchanged=submissions;button(c,MIXER_SELECT);button(c,MIXER_SELECT);
    button(c,MIXER_SELECT);assert(view(c).cursor==2);button(c,MIXER_SELECT);assert(view(c).cursor==3);button(c,MIXER_SELECT);assert(view(c).cursor==0);
    /* B2 Mute, B3 Solo for the selection. */
    button(c,MIXER_B2);ack(c);assert(accepted.track[0].mix.mute&&accepted.track[2].mix.mute&&accepted.track[4].mix.mute&&!accepted.track[1].mix.mute);
    button(c,MIXER_B3);ack(c);assert(accepted.track[0].mix.solo&&accepted.track[4].mix.solo);
    button(c,MIXER_B2);ack(c);assert(!accepted.track[0].mix.mute);
    /* Master: one, clears Track selection, no Mute/Solo, Level works. */
    pad(c,15,1);pad(c,15,0);assert(view(c).master&&!view(c).selected);button(c,MIXER_B2);assert(!strcmp(mixer_control_message(c),"Master has no Mute/Solo"));
    cc(c,0x14,103);ack(c);assert(accepted.master.mix.level_db==0);
    pad(c,0,1);pad(c,0,0);assert(!view(c).master&&view(c).selected==1);
    puts("PASS one Mixer page: PAD FX/KNOB FX back on B1, held/Shift multi-select, relative Fader, Shift+Fader, absolute K1 for the selection, encoder follows, B2 Mute, B3 Solo, Master");
    /* PAD FX: assign sounds to the selected Track; on Master they go to Master. */
    mixer_control_presence(c,1,UINT64_C(1)<<63,7);button(c,MIXER_PAD_FX);assert(view(c).page==MIXER_ASSIGN);pad(c,15,1);pad(c,15,0);
    assert(view(c).project->output[127]==0&&view(c).pending);assert(mixer_control_cursor(c,15)==2);ack(c);
    assert(accepted.output[127]==1);pad(c,14,1);pad(c,14,0);assert(!strcmp(mixer_control_message(c),"Empty pad"));
    unsigned bank_submissions=submissions;
    button(c,MIXER_BANK);assert(view(c).bank==0&&view(c).sound==14&&view(c).primary==0);
    button(c,MIXER_BANK_PREVIOUS);assert(view(c).bank==7&&view(c).sound==126);
    mixer_control_button(c,MIXER_SHIFT,1,ticket);button(c,MIXER_BANK);
    mixer_control_button(c,MIXER_SHIFT,0,ticket);assert(view(c).bank==6);
    button(c,MIXER_BANK_NEXT);assert(view(c).bank==7);
    mixer_control_tick(c);assert(submissions==bank_submissions);
    /* Select pads: the enlarged Track keeps Fader, K1, B2/B3 working. */
    cc(c,0x10,0);ack(c);assert(accepted.track[0].mix.pan==-1&&view(c).page==MIXER_ASSIGN);
    cc(c,0x14,103);ack(c);assert(accepted.track[0].mix.level_db==0);
    button(c,MIXER_B2);ack(c);assert(accepted.track[0].mix.mute);button(c,MIXER_B2);ack(c);assert(!accepted.track[0].mix.mute);
    button(c,MIXER_B1);pad(c,15,1);pad(c,15,0);button(c,MIXER_PAD_FX);assert(view(c).page==MIXER_ASSIGN);
    assert(mixer_control_cursor(c,15)==1);pad(c,15,1);pad(c,15,0);ack(c);assert(accepted.output[127]==0&&mixer_control_cursor(c,15)==2);
    button(c,MIXER_B1);pad(c,0,1);pad(c,0,0);
    unchanged=submissions;mixer_control_button(c,MIXER_SHIFT,1,ticket);button(c,MIXER_PAD_FX);mixer_control_button(c,MIXER_SHIFT,0,ticket);
    assert(view(c).page==MIXER_INSPECT);cc(c,0x10,127);cc(c,0x14,127);pad(c,15,1);pad(c,15,0);mixer_control_tick(c);assert(submissions==unchanged);
    button(c,MIXER_PAD_FX);assert(view(c).page==MIXER_OVERVIEW);
    pad(c,0,1);pad(c,1,1);pad(c,0,0);pad(c,1,0);button(c,MIXER_PAD_FX);assert(view(c).page==MIXER_OVERVIEW&&!strcmp(mixer_control_message(c),"Select one Track"));
    pad(c,0,1);pad(c,0,0);
    puts("PASS all-bank occupied-only routing, white cursor on the selected Track, Master assign, read-only Inspect");
    busy=1;cc(c,0x10,127);mixer_control_tick(c);assert(view(c).pending);busy=0;ack(c);
    cc(c,0x10,64);mixer_control_tick(c);cc(c,0x10,12);assert(mixer_control_confirm(c,&accepted,revision,epoch));ack(c);
    assert(mixer_project_valid(&accepted));
    /* B1 on the Mixer page closes it and asks the owner for native Sample. */
    ticket=mixer_control_ticket(c);assert(!mixer_control_exit_requested(c));
    assert(mixer_control_button(c,MIXER_B1,1,ticket)&&!mixer_control_active(c));
    assert(mixer_control_exit_requested(c)&&!mixer_control_exit_requested(c));assert(mixer_control_button(c,MIXER_B1,0,ticket));
    assert(mixer_control_open(c,1,0));ticket=mixer_control_ticket(c);
    assert(mixer_control_button(c,MIXER_B2,1,ticket));pad(c,3,1);
    assert(!mixer_control_button(c,MIXER_NATIVE_MODE,1,ticket)&&!mixer_control_active(c));assert(mixer_control_ticket(c)!=ticket);
    assert(mixer_control_button(c,MIXER_B2,0,ticket));assert(mixer_control_pad(c,3,0,ticket));assert(!mixer_control_button(c,MIXER_B2,1,mixer_control_ticket(c)));assert(!mixer_control_cc(c,0x10,1,ticket));
    assert(mixer_control_open(c,1,0));ticket=mixer_control_ticket(c);++epoch;++revision;assert(mixer_control_confirm(c,&accepted,revision,epoch)&&!mixer_control_active(c));assert(mixer_control_ticket(c)!=ticket);
    mixer_control_destroy(c);puts("PASS coalesced confirmed edits, busy retry, B1 exit to Sample, mode exit, stale tickets/paired release recovery and Load/New epoch invalidation");colour_controls();fx_slots();send_name_history();return 0;
}
