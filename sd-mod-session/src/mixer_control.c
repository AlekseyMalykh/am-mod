#include "mixer_control.h"
#include "fx_catalog.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
struct mixer_control {
    struct mixer_control_ops ops;
    struct mixer_project model,desired;
    struct mixer_view_state view,back;
    struct mixer_meter_snapshot meter;
    uint64_t revision,epoch,ticket,submitted,present[2];
    double rate;
    unsigned active,shift,owned_buttons,owned_pads,held_tracks,temporary,dirty,exit_requested,fx_editing;
    unsigned pickup,last_fader_valid,last_fader,anchor;
    float anchors[8];
    unsigned catalog,parameter_page;
    const char*message;
};
static unsigned count(unsigned mask){unsigned n=0;while(mask){n+=mask&1;mask>>=1;}return n;}
static struct mixer_channel*selected(struct mixer_control*c){return c->view.master?&c->desired.master:c->desired.track+c->view.primary;}
static void pickup(struct mixer_control*c){c->pickup=0;c->last_fader_valid=0;}
static void changed(struct mixer_control*c){c->dirty=1;c->message="";}
static int one(const struct mixer_control*c){return c->view.master||count(c->view.selected)==1;}
static void open_fx(struct mixer_control*);
struct mixer_control*mixer_control_create(const struct mixer_control_ops*ops,double rate)
{
    if(!ops||!ops->submit||!isfinite(rate)||rate<8000||rate>192000)return NULL;
    struct mixer_control*c=calloc(1,sizeof(*c));if(!c)return NULL;
    c->ops=*ops;c->rate=rate;c->ticket=1;c->view.selected=1;c->message="";
    mixer_defaults(&c->model);c->desired=c->model;return c;
}
void mixer_control_destroy(struct mixer_control*c){free(c);}
int mixer_control_confirm(struct mixer_control*c,const struct mixer_project*p,uint64_t revision,uint64_t epoch)
{
    if(!c||!epoch||!mixer_project_valid(p)||(epoch==c->epoch&&revision<c->revision))return 0;
    int replacement=epoch!=c->epoch;
    if(replacement){mixer_control_close(c);c->epoch=epoch;c->submitted=c->dirty=0;}
    if(!replacement&&c->submitted&&revision<c->submitted)return 0;
    if(!replacement&&revision==c->revision)return 1;
    int own_ack=c->submitted&&revision==c->submitted;
    c->model=*p;c->revision=revision;c->submitted=0;
    if(!own_ack||!c->dirty)c->desired=*p;
    if(!own_ack){c->dirty=0;c->view.colour_edit=0;pickup(c);}
    return 1;
}
int mixer_control_open(struct mixer_control*c,int ready,int blocked)
{
    if(!c||!ready||blocked||!c->epoch)return 0;
    c->active=1;c->view.page=MIXER_OVERVIEW;c->temporary=0;c->held_tracks=0;
    c->view.cursor=c->view.scroll=c->view.colour_edit=0;c->message="";++c->ticket;pickup(c);return 1;
}
int mixer_control_reject(struct mixer_control*c,uint64_t revision,const char *reason)
{
    if(!c||!revision||revision!=c->submitted)return 0;
    c->submitted=c->dirty=0;c->desired=c->model;c->message=reason?reason:"Update failed";
    if(c->view.page==MIXER_KNOB_FX)open_fx(c);
    pickup(c);return 1;
}
void mixer_control_close(struct mixer_control*c)
{
    if(c){c->active=0;c->temporary=0;c->held_tracks=0;c->shift=c->view.colour_edit=0;++c->ticket;pickup(c);}
}
int mixer_control_active(const struct mixer_control*c){return c&&c->active;}
uint64_t mixer_control_ticket(const struct mixer_control*c){return c?c->ticket:0;}
/* Device-agreed layout (07.11.2026): one Mixer page with Track cells.
 * B1 = back to the Mixer from PAD FX/KNOB FX, from the Mixer exit to Sample;
 * B2 = Mute, B3 = Solo of the selection; Fader = Level, K1 = Pan, K3 =
 * Colour, all absolute and equal for every selected Track; the last moved
 * control becomes the encoder parameter. Track FX editing is not offered. */
enum {PARAM_LEVEL,PARAM_PAN,PARAM_COLOUR,PARAMS};
static void group_gate(struct mixer_control*c,int solo)
{
    if(c->view.master){c->message="Master has no Mute/Solo";return;}
    int all=1;for(unsigned i=0;i<8;++i)if(c->view.selected&(1u<<i))if(!(solo?c->desired.track[i].mix.solo:c->desired.track[i].mix.mute))all=0;
    for(unsigned i=0;i<8;++i)if(c->view.selected&(1u<<i)){
        if(solo)c->desired.track[i].mix.solo=!all;else c->desired.track[i].mix.mute=!all;}
    changed(c);
}
static void open_fx(struct mixer_control*c)
{
    struct mixer_channel*ch=selected(c);c->catalog=c->fx_editing&&c->view.fx_slot>=ch->fx.count;c->parameter_page=0;
    c->view.cursor=0;
}
void mixer_control_set_fx_editing(struct mixer_control*c,int on){if(c)c->fx_editing=on!=0;}
static void each(struct mixer_control*c,void(*apply)(struct mixer_mix*,float),float value)
{
    if(c->view.master){apply(&c->desired.master.mix,value);changed(c);return;}
    for(unsigned i=0;i<8;++i)if(c->view.selected&(1u<<i))apply(&c->desired.track[i].mix,value);
    changed(c);
}
static void set_level(struct mixer_mix*m,float db){m->level_db=db;}
static void set_pan(struct mixer_mix*m,float pan){m->pan=pan<-1?-1:pan>1?1:pan;}
static void set_colour(struct mixer_control*c,unsigned index)
{
    if(c->view.master){c->message="Master colour is fixed";return;}
    uint32_t rgb=mixer_colour_at(index<MIXER_COLOURS?index:MIXER_COLOURS-1);
    for(unsigned i=0;i<8;++i)if((c->view.selected&(1u<<i))&&c->desired.track[i].colour!=rgb){c->desired.track[i].colour=rgb;changed(c);}
}
static const struct mixer_mix*primary_mix(const struct mixer_control*c)
{return c->view.master?&c->desired.master.mix:&c->desired.track[c->view.primary].mix;}
int mixer_control_button(struct mixer_control*c,enum mixer_action action,int down,uint64_t ticket)
{
    if(!c||(unsigned)action>MIXER_BANK_PREVIOUS)return 0;
    unsigned bit=1u<<action;
    if(action==MIXER_SHIFT){c->shift=down!=0;return 0;}
    if(!down&&(c->owned_buttons&bit)){c->owned_buttons&=~bit;return 1;}
    if(down&&(c->owned_buttons&bit))return 1;
    int bank_action=action>=MIXER_BANK;
    if(bank_action&&c->view.page!=MIXER_ASSIGN&&c->view.page!=MIXER_INSPECT)return 0;
    if(action==MIXER_NATIVE_MODE){if(down&&c->active)mixer_control_close(c);return 0;}
    if(!c->active)return 0;
    if(ticket!=c->ticket)return 1;
    if(!down)return 1;
    c->owned_buttons|=bit;c->message="";
    if(bank_action){
        int previous=action==MIXER_BANK_PREVIOUS||(action==MIXER_BANK&&c->shift);
        c->view.bank=(c->view.bank+(previous?7:1))%8;
        c->view.sound=c->view.bank*16+c->view.sound%16;
    }else if(action==MIXER_B1){
        if(c->view.page!=MIXER_OVERVIEW){c->view.page=MIXER_OVERVIEW;c->temporary=0;c->view.cursor=PARAM_LEVEL;}
        else{c->exit_requested=1;mixer_control_close(c);}
    }else if(action==MIXER_B2&&c->shift){
        /* Shift+B2: FX Bypass of the selection or Master. */
        if(c->view.master)c->desired.master.fx_bypass^=1;
        else for(unsigned i=0;i<8;++i)if(c->view.selected&(1u<<i))c->desired.track[i].fx_bypass^=1;
        changed(c);
    }else if(action==MIXER_B2||action==MIXER_B3){
        if(c->view.page==MIXER_OVERVIEW||c->view.page==MIXER_ASSIGN)group_gate(c,action==MIXER_B3);
    }else if(action==MIXER_PAD_FX){
        if(c->view.page==MIXER_ASSIGN||c->view.page==MIXER_INSPECT){c->view.page=MIXER_OVERVIEW;c->temporary=0;return 1;}
        if(!c->shift&&!one(c)){c->message="Select one Track";return 1;}
        c->view.page=c->shift?MIXER_INSPECT:MIXER_ASSIGN;c->temporary=1;
    }else if(action==MIXER_KNOB){
        if(c->view.page==MIXER_KNOB_FX){c->view.page=MIXER_OVERVIEW;c->temporary=0;c->view.cursor=PARAM_LEVEL;return 1;}
        if(c->fx_editing&&!one(c)){c->message="Select one Track";return 1;}
        c->view.page=MIXER_KNOB_FX;c->temporary=2;open_fx(c);
        if(!c->fx_editing)c->message="Track FX: coming later";
    }else if(action==MIXER_SELECT){
        if(c->view.page==MIXER_OVERVIEW)c->view.cursor=(c->view.cursor+1)%PARAMS;
        else if(c->view.page==MIXER_KNOB_FX&&c->fx_editing){struct mixer_channel*ch=selected(c);
            if(c->catalog){struct fx_stage stage;
                if(fx_catalog_stage(c->view.cursor,c->rate,&stage)){
                    if(c->view.fx_slot<ch->fx.count)ch->fx.stage[c->view.fx_slot]=stage;
                    else{
                        if(fx_chain_add(&ch->fx,&stage,c->rate)!=FX_OK){c->message="FX chain full";return 1;}
                        c->view.fx_slot=ch->fx.count-1;
                    }
                    c->catalog=0;c->parameter_page=c->view.cursor=0;changed(c);}
            }else if(c->shift){if(fx_chain_remove(&ch->fx,c->view.fx_slot)==FX_OK){c->catalog=1;changed(c);}}
            else if(c->view.fx_slot<ch->fx.count){ch->fx.stage[c->view.fx_slot].bypass^=1;changed(c);}
        }
    }
    return 1;
}
int mixer_control_pad(struct mixer_control*c,unsigned pad,int down,uint64_t ticket)
{
    if(!c||pad>=16)return 0;
    unsigned bit=1u<<pad;
    if(!down&&(c->owned_pads&bit)){c->owned_pads&=~bit;if(pad<8)c->held_tracks&=~bit;return 1;}
    if(down&&(c->owned_pads&bit))return 1;
    if(!c->active)return 0;
    if(ticket!=c->ticket)return 1;
    if(!down)return 1;
    c->owned_pads|=bit;
    if(c->view.page==MIXER_ASSIGN||c->view.page==MIXER_INSPECT){unsigned sound=c->view.bank*16+pad;c->view.sound=sound;
        if(c->view.page==MIXER_ASSIGN){if(!(c->present[sound/64]&(UINT64_C(1)<<(sound%64))))c->message="Empty pad";
            else if(c->view.master){if(c->desired.output[sound]){c->desired.output[sound]=0;changed(c);}}
            else if(one(c)){unsigned output=c->view.primary+1;
                c->desired.output[sound]=(unsigned char)(c->desired.output[sound]==output?0:output);changed(c);}}
        return 1;
    }
    if(pad<8){
        if(c->shift&&!c->view.master){
            /* Shift: add or remove one Track; the last Track stays selected. */
            unsigned next=c->view.selected^bit;
            if(next){c->view.selected=next;if(next&bit)c->view.primary=pad;
                else if(c->view.primary==pad){for(unsigned i=0;i<8;++i)if(next&(1u<<i)){c->view.primary=i;break;}}}
        }else if(!c->held_tracks||c->view.master){c->view.selected=bit;c->view.primary=pad;}
        else c->view.selected|=bit;
        c->held_tracks|=bit;c->view.master=0;pickup(c);
    }else if(pad==15){c->view.master=1;c->view.selected=0;c->held_tracks=0;pickup(c);}
    c->view.colour_edit=0;
    if(c->view.page==MIXER_KNOB_FX&&(pad<8||pad==15))open_fx(c);
    return 1;
}
float mixer_fader_db(unsigned value)
{
    if(!value)return -INFINITY;
    if(value>127)value=127;
    return value<=103?-60.f+(value-1)*60.f/102.f:(value-103)*.25f;
}
int mixer_control_reset(struct mixer_control*c,unsigned cc,uint64_t ticket)
{
    /* Pad Bank held + control: default value for the whole selection. */
    if(!c||!c->active)return 0;
    if(ticket!=c->ticket||(c->view.page!=MIXER_OVERVIEW&&c->view.page!=MIXER_ASSIGN))return 1;
    if(cc==0x14){each(c,set_level,0);c->view.cursor=PARAM_LEVEL;}
    else if(cc==0x10){each(c,set_pan,0);c->view.cursor=PARAM_PAN;}
    else if(cc==0x12){set_colour(c,MIXER_DEFAULT_TRACK_COLOUR);c->view.cursor=PARAM_COLOUR;}
    return 1;
}
/* Track FX editor (kept for the native FX tests; off on the device UI). */
static int fx_cc(struct mixer_control*c,unsigned cc,unsigned value)
{
    struct mixer_channel*ch=selected(c);
    if(cc==0x64){int delta=value>64?(int)value-128:(int)value;
        unsigned limit=c->catalog?fx_catalog_count():5;
        if(!c->catalog&&c->view.fx_slot<ch->fx.count){
            const struct fx_stage*s=ch->fx.stage+c->view.fx_slot;
            if(s->kind==FX_STAGE_EQ)limit=s->eq.count?s->eq.count:1;
            else{const struct fx_catalog_entry*e=fx_catalog_find(s->native_id);unsigned n=0;
                if(e)for(unsigned i=0;i<e->count;++i)if(e->parameter[i]&&*e->parameter[i])++n;
                limit=(n+2)/3;if(!limit)limit=1;}
        }
        int next=(int)c->view.cursor+delta;if(next<0)next=0;if((unsigned)next>=limit)next=(int)limit-1;c->view.cursor=(unsigned)next;
        if(!c->catalog)c->parameter_page=c->view.cursor;
        return 1;
    }
    if(!one(c)){c->message="Select one Track";return 1;}
    if(c->catalog){if(cc==0x10)c->view.cursor=value*fx_catalog_count()/128;return 1;}
    if(cc==0x14||c->view.fx_slot>=ch->fx.count)return 1;
    struct fx_stage*s=ch->fx.stage+c->view.fx_slot;
    if(s->kind==FX_STAGE_NATIVE){unsigned position=c->parameter_page*3+cc-0x10,index=FX_CHAIN_PARAMETERS,n=0;
        const struct fx_catalog_entry*e=fx_catalog_find(s->native_id);
        if(e)for(unsigned i=0;i<e->count;++i)if(e->parameter[i]&&*e->parameter[i]){if(n++==position){index=i;break;}}
        if(index<FX_CHAIN_PARAMETERS){s->parameter[index]=value/127.f;changed(c);}
    }else{if(!s->eq.count)fx_eq_default(&s->eq);
        struct fx_eq_band*b=s->eq.band+(c->parameter_page<s->eq.count?c->parameter_page:0);
        if(c->shift&&cc==0x10)b->type=value*6/128;
        else if(c->shift&&cc==0x11)b->enabled=value>=64;
        else if(c->shift&&cc==0x12)s->eq.output_db=-24+value*36./127;
        else if(cc==0x10)b->hz=20*pow(1000,value/127.);
        else if(cc==0x11)b->gain_db=-18+value*36./127;
        else b->q=.2*pow(80,value/127.);
        if(b->hz>c->rate*.45)b->hz=c->rate*.45;
        if(fx_eq_valid(&s->eq,c->rate))changed(c);
    }
    return 1;
}
int mixer_control_cc(struct mixer_control*c,unsigned cc,unsigned value,uint64_t ticket)
{
    if(!c||!c->active)return 0;
    if(cc!=0x10&&cc!=0x11&&cc!=0x12&&cc!=0x14&&cc!=0x64)return 0;
    if(ticket!=c->ticket||value>127)return 1;
    if(c->view.page==MIXER_INSPECT){
        if(cc==0x12){c->view.bank=value*8/128;c->view.sound=c->view.bank*16+c->view.sound%16;}
        else if(cc==0x64){int delta=value>64?(int)value-128:(int)value;
            int sound=(int)c->view.sound+delta;if(sound<0)sound=0;if(sound>127)sound=127;c->view.sound=(unsigned)sound;c->view.bank=(unsigned)sound/16;}
        return 1; /* Inspect is read-only, including Fader and K1. */
    }
    /* PAD FX (Select pads): the enlarged Track on the left keeps Fader, K1,
     * K3 and the encoder exactly as on the Mixer page; banks via Bank/+/-. */
    if(c->view.page==MIXER_KNOB_FX&&c->fx_editing)return fx_cc(c,cc,value);
    if(c->view.page!=MIXER_OVERVIEW&&c->view.page!=MIXER_ASSIGN)return 1; /* Track FX: not editable yet */
    if(cc==0x14){each(c,set_level,mixer_fader_db(value));c->view.cursor=PARAM_LEVEL;return 1;}
    if(cc==0x10){each(c,set_pan,value==64?0:((float)value-64)/(value<64?64:63));c->view.cursor=PARAM_PAN;return 1;}
    if(cc==0x12){set_colour(c,value*MIXER_COLOURS/128);c->view.cursor=PARAM_COLOUR;return 1;}
    if(cc==0x11)return 1; /* K2: empty */
    int delta=value>64?(int)value-128:(int)value;if(!delta)return 1;
    const struct mixer_mix*m=primary_mix(c);
    if(c->view.cursor==PARAM_LEVEL){float db=isfinite(m->level_db)?m->level_db:-60.5f;db+=delta*.5f;
        each(c,set_level,db< -60?-INFINITY:db>6?6:db);}
    else if(c->view.cursor==PARAM_PAN)each(c,set_pan,m->pan+delta*.02f);
    else if(!c->view.master){int index=mixer_colour_index(c->desired.track[c->view.primary].colour);
        int next=index<0?0:index+delta;if(next<0)next=0;if(next>=(int)MIXER_COLOURS)next=MIXER_COLOURS-1;set_colour(c,(unsigned)next);}
    return 1;
}
int mixer_control_exit_requested(struct mixer_control*c)
{
    if(!c||!c->exit_requested)return 0;
    c->exit_requested=0;return 1;
}
void mixer_control_tick(struct mixer_control*c)
{
    if(!c||!c->dirty||c->submitted)return;
    uint64_t revision=0;
    if(c->ops.submit(c->ops.user,&c->desired,c->revision,&revision)&&revision>c->revision){c->submitted=revision;c->dirty=0;}
    else c->message="Audio update pending";
}
void mixer_control_presence(struct mixer_control*c,uint64_t lo,uint64_t hi,unsigned bank)
{
    if(c&&bank<8){c->present[0]=lo;c->present[1]=hi;
        if(c->view.bank!=bank)c->view.sound=bank*16+c->view.sound%16;
        c->view.bank=bank;}
}
int mixer_control_led(const struct mixer_control*c,unsigned pad,uint32_t*rgb)
{
    if(!c||!c->active||pad>=16||!rgb||(c->view.page!=MIXER_ASSIGN&&c->view.page!=MIXER_INSPECT))return 0;
    unsigned sound=c->view.bank*16+pad,output=c->model.output[sound];
    if(!(c->present[sound/64]&(UINT64_C(1)<<(sound%64))))*rgb=0;
    else if(!output)*rgb=0x181818;
    else{uint32_t colour=c->model.track[output-1].colour;
        *rgb=!c->view.master&&output==c->view.primary+1?colour:((colour&0xfcfcfc)>>2);}
    return 1;
}
int mixer_control_cursor(const struct mixer_control*c,unsigned pad)
{
    /* Device feedback: no Track colours on the pads. 2 = sound on the selected
     * Track (Master when Master is selected), shown as the blinking white
     * cursor; 1 = other loaded sound; 0 = empty. Uses the desired routing so
     * several quick presses all show before audio confirms them. */
    if(!c||!c->active||pad>=16||(c->view.page!=MIXER_ASSIGN&&c->view.page!=MIXER_INSPECT))return -1;
    unsigned sound=c->view.bank*16+pad,output=c->desired.output[sound];
    unsigned target=c->view.master?0:c->view.primary+1;
    if(!(c->present[sound/64]&(UINT64_C(1)<<(sound%64))))return 0;
    return output==target?2:1;
}
int mixer_control_output(const struct mixer_control*c,unsigned sound)
{
    return c&&sound<MIXER_PADS?(int)c->desired.output[sound]:-1;
}
int mixer_control_set_output(struct mixer_control*c,unsigned sound,unsigned output)
{
    if(!c||sound>=MIXER_PADS||output>8)return 0;
    if(c->desired.output[sound]!=output){c->desired.output[sound]=(unsigned char)output;changed(c);}
    return 1;
}
void mixer_control_meter(struct mixer_control*c,const struct mixer_meter_snapshot*m,uint64_t now)
{
    if(c&&m&&m->epoch==c->epoch){c->meter=*m;c->view.meter_ms=now;}
}
void mixer_control_view(struct mixer_control*c,uint64_t now,struct mixer_view_state*out)
{
    if(c&&out){*out=c->view;out->project=&c->model;out->meter=&c->meter;out->epoch=c->epoch;out->now_ms=now;
        out->revision=c->revision;out->pending=c->submitted||c->dirty;out->fx_catalog=c->catalog;out->parameter_page=c->parameter_page;
        out->message=c->message;out->pickup=!c->pickup;out->present[0]=c->present[0];out->present[1]=c->present[1];}
    if(c&&out)out->shifted=c->shift;
}
const char*mixer_control_message(const struct mixer_control*c){return c?c->message:"";}
