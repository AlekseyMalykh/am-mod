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
    unsigned active,shift,owned_buttons,owned_pads,held_tracks,temporary,dirty;
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
static void temporary(struct mixer_control*c,unsigned kind,enum mixer_page page)
{
    c->view.colour_edit=0;
    if(c->temporary==kind){enum mixer_page back=c->back.page;unsigned cursor=c->back.cursor,scroll=c->back.scroll;
        c->temporary=0;c->view.page=back;c->view.cursor=cursor;c->view.scroll=scroll;
    }else{if(!c->temporary)c->back=c->view;c->temporary=kind;c->view.page=page;c->view.cursor=c->view.scroll=0;}
}
static void set_colour(struct mixer_control*c,unsigned index)
{
    if(!one(c)){c->message="Select one Track";return;}
    uint32_t rgb=mixer_colour_at(index);
    if(selected(c)->colour!=rgb){selected(c)->colour=rgb;changed(c);}
}
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
    struct mixer_channel*ch=selected(c);c->catalog=c->view.fx_slot>=ch->fx.count;c->parameter_page=0;
    c->view.cursor=0;
}
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
    }else if(action<=MIXER_B3){
        if(c->shift){if(action==MIXER_B1||action==MIXER_B3)group_gate(c,action==MIXER_B3);
            else if(one(c)){selected(c)->fx_bypass^=1;changed(c);}else c->message="Select one Track";
        }else{enum mixer_page page=(enum mixer_page)(action+1);c->view.page=!c->temporary&&c->view.page==page?MIXER_OVERVIEW:page;
            c->temporary=0;c->view.cursor=c->view.scroll=c->view.colour_edit=0;}
    }else if(action==MIXER_PAD_FX){
        if(c->temporary==1){temporary(c,1,MIXER_ASSIGN);return 1;}
        if((!one(c)||c->view.master)&&!c->shift){c->message="Select one Track";return 1;}
        temporary(c,1,c->shift?MIXER_INSPECT:MIXER_ASSIGN);
    }else if(action==MIXER_KNOB){
        if(c->temporary==2){temporary(c,2,MIXER_KNOB_FX);return 1;}
        if(!one(c)){c->message="Select one Track";return 1;}
        temporary(c,2,c->shift?MIXER_ADVANCED:MIXER_KNOB_FX);open_fx(c);
    }else if(action==MIXER_SELECT){
        if(c->view.page==MIXER_KNOB_FX&&!one(c)){c->message="Select one Track";return 1;}
        if(c->view.page==MIXER_FX){if(!one(c)){c->message="Select one Track";return 1;}
            c->view.fx_slot=c->view.cursor;temporary(c,2,MIXER_KNOB_FX);open_fx(c);}
        else if(c->view.page==MIXER_KNOB_FX){struct mixer_channel*ch=selected(c);
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
        }else if(c->view.page==MIXER_MIX){
            if(c->view.cursor==2)group_gate(c,0);else if(c->view.cursor==3)group_gate(c,1);
            else if(c->view.cursor==4&&one(c)){selected(c)->fx_bypass^=1;changed(c);}
            else if(c->view.cursor==5){if(one(c))c->view.colour_edit^=1;else c->message="Select one Track";}
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
            else if(one(c)&&!c->view.master){unsigned output=c->view.primary+1;
                c->desired.output[sound]=(unsigned char)(c->desired.output[sound]==output?0:output);changed(c);}}
        return 1;
    }
    if(pad<8){if(!c->held_tracks||c->view.master){c->view.selected=bit;c->view.primary=pad;}
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
static void fader(struct mixer_control*c,unsigned value)
{
    float target=selected(c)->mix.level_db,position=mixer_fader_db(value);
    unsigned group=!c->view.master&&count(c->view.selected)>1;
    if(group&&!isfinite(target)){c->message="Select one track to raise from Off";return;}
    if(!c->pickup){float last=c->last_fader_valid?mixer_fader_db(c->last_fader):position;
        int hit=(!isfinite(target)&&!value)||(isfinite(target)&&isfinite(position)&&
            (fabsf(position-target)<=.6f||(isfinite(last)&&((last<=target&&position>=target)||(last>=target&&position<=target)))));
        c->last_fader=value;c->last_fader_valid=1;
        if(!hit){c->message="PICKUP";return;}
        c->pickup=1;c->anchor=value;
        for(unsigned i=0;i<8;++i)c->anchors[i]=c->desired.track[i].mix.level_db;
    }
    if(group){if(!isfinite(position))position=-60;
        float delta=position-mixer_fader_db(c->anchor),low=-1000,high=1000;
        for(unsigned i=0;i<8;++i)if((c->view.selected&(1u<<i))&&isfinite(c->anchors[i])){
            low=fmaxf(low,-60-c->anchors[i]);high=fminf(high,6-c->anchors[i]);}
        delta=fminf(high,fmaxf(low,delta));
        for(unsigned i=0;i<8;++i)if((c->view.selected&(1u<<i))&&isfinite(c->anchors[i]))c->desired.track[i].mix.level_db=c->anchors[i]+delta;
    }else selected(c)->mix.level_db=position;
    changed(c);
}
int mixer_control_cc(struct mixer_control*c,unsigned cc,unsigned value,uint64_t ticket)
{
    if(!c||!c->active)return 0;
    if(cc!=0x10&&cc!=0x11&&cc!=0x12&&cc!=0x14&&cc!=0x64)return 0;
    if(ticket!=c->ticket||value>127)return 1;
    if(c->view.page==MIXER_INSPECT||c->view.page==MIXER_ASSIGN){
        if(cc==0x12){c->view.bank=value*8/128;c->view.sound=c->view.bank*16+c->view.sound%16;}
        else if(cc==0x64){int delta=value>64?(int)value-128:(int)value;
            int sound=(int)c->view.sound+delta;if(sound<0)sound=0;if(sound>127)sound=127;c->view.sound=(unsigned)sound;c->view.bank=(unsigned)sound/16;}
        return 1; /* Inspect is read-only, including Fader and K1. */
    }
    if(cc==0x14){fader(c,value);return 1;}
    struct mixer_channel*ch=selected(c);
    if(c->view.page==MIXER_MIX&&cc==0x12){
        c->view.cursor=5;c->view.scroll=1;set_colour(c,value*MIXER_COLOURS/128);return 1;
    }
    if(cc==0x64){int delta=value>64?(int)value-128:(int)value;
        if(c->view.page==MIXER_MIX&&c->view.colour_edit){
            if(!delta)return 1;
            int index=mixer_colour_index(ch->colour);
            int next=index<0?(delta>0?0:(int)MIXER_COLOURS-1):index+delta;
            if(next<0)next=0;
            if(next>=(int)MIXER_COLOURS)next=MIXER_COLOURS-1;
            set_colour(c,(unsigned)next);return 1;
        }
        if(c->view.page==MIXER_FX&&c->shift){
            if(!one(c)){c->message="Select one Track";return 1;}
            int next=(int)c->view.cursor+delta;
            if(next>=0&&(unsigned)next<ch->fx.count&&c->view.cursor<ch->fx.count&&
               fx_chain_move(&ch->fx,c->view.cursor,(unsigned)next)==FX_OK){c->view.cursor=(unsigned)next;changed(c);}
            return 1;
        }
        unsigned limit=c->view.page==MIXER_KNOB_FX&&c->catalog?fx_catalog_count():c->view.page==MIXER_OVERVIEW?8:c->view.page==MIXER_MIX?6:5;
        if(c->view.page==MIXER_KNOB_FX&&!c->catalog&&c->view.fx_slot<ch->fx.count){
            const struct fx_stage*s=ch->fx.stage+c->view.fx_slot;
            if(s->kind==FX_STAGE_EQ)limit=s->eq.count?s->eq.count:1;
            else{const struct fx_catalog_entry*e=fx_catalog_find(s->native_id);unsigned n=0;
                if(e)for(unsigned i=0;i<e->count;++i)if(e->parameter[i]&&*e->parameter[i])++n;
                limit=(n+2)/3;if(!limit)limit=1;}
        }
        int next=(int)c->view.cursor+delta;if(next<0)next=0;if((unsigned)next>=limit)next=(int)limit-1;c->view.cursor=(unsigned)next;
        if(c->view.page==MIXER_MIX){if(c->view.cursor<c->view.scroll)c->view.scroll=c->view.cursor;else if(c->view.cursor>=c->view.scroll+5)c->view.scroll=c->view.cursor-4;}
        if(c->view.page==MIXER_KNOB_FX&&!c->catalog){c->parameter_page=c->view.cursor;}
        return 1;
    }
    if(c->view.page==MIXER_ADVANCED){c->message="Sidechain unavailable";return 1;}
    if(c->view.page==MIXER_KNOB_FX){
        if(!one(c)){c->message="Select one Track";return 1;}
        if(c->catalog){if(cc==0x10)c->view.cursor=value*fx_catalog_count()/128;return 1;}
        if(c->view.fx_slot>=ch->fx.count)return 1;
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
        }return 1;
    }
    if(cc==0x10){ch->mix.pan=value==64?0:((float)value-64)/(value<64?64:63);changed(c);}
    else if(cc==0x11)c->message="Send unavailable";
    else if(c->view.page==MIXER_ASSIGN||c->view.page==MIXER_INSPECT)c->view.bank=value*8/128;
    return 1;
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
