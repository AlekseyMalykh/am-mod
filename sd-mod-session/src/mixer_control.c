#include "mixer_control.h"
#include "fx_catalog.h"
#include "phone_input.h"
#include <time.h>
#define MIXER_HISTORY 16u
#include <math.h>
#include <stdlib.h>
#include <string.h>
struct mixer_control {
    struct mixer_control_ops ops;
    struct mixer_project model,desired,history[MIXER_HISTORY];
    unsigned history_start,history_count,history_pos;
    int history_request,history_step;
    struct phone_input name;
    unsigned naming;
    char name_preview[80];
    struct mixer_view_state view,back;
    struct mixer_meter_snapshot meter;
    struct mixer_fx_monitor fx_signal;
    uint64_t fx_signal_ms,name_since;
    unsigned name_target;
    uint64_t revision,epoch,ticket,submitted,present[2];
    double rate;
    unsigned active,shift,owned_buttons,owned_pads,held_tracks,temporary,dirty,exit_requested,fx_editing;
    unsigned pickup,last_fader_valid,last_fader;
    float anchors[8];
    unsigned catalog,parameter_page,fx_copied;
    struct fx_stage fx_clipboard;
    const char*message;
};
static unsigned history_index(const struct mixer_control*c,unsigned pos)
{return (c->history_start+pos)%MIXER_HISTORY;}
static void history_record(struct mixer_control*c,const struct mixer_project*p)
{
    c->history_count=c->history_pos+1;
    if(c->history_count==MIXER_HISTORY){c->history_start=(c->history_start+1)%MIXER_HISTORY;--c->history_count;}
    c->history_pos=c->history_count++;
    c->history[history_index(c,c->history_pos)]=*p;
}
static uint64_t name_time(void)
{
    struct timespec now={0};
    timespec_get(&now,TIME_UTC);
    return (uint64_t)now.tv_sec*1000+(unsigned long)now.tv_nsec/1000000;
}
static void name_turn(struct phone_input*name,int delta)
{
    static const char alphabet[]=" ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.";
    phone_commit(name);
    if(!delta||name->cursor>=name->limit)return;
    int position=0;
    if(name->cursor<name->length)
        for(unsigned i=0;alphabet[i];++i)if((unsigned char)alphabet[i]==name->text[name->cursor]){position=(int)i;break;}
    int length=sizeof(alphabet)-1;
    position=(position+delta)%length;
    if(position<0)position+=length;
    if(name->cursor==name->length)++name->length;
    name->text[name->cursor]=(unsigned char)alphabet[position];
}
static unsigned count(unsigned mask){unsigned n=0;while(mask){n+=mask&1;mask>>=1;}return n;}
static struct mixer_channel*selected(struct mixer_control*c){return c->view.master?&c->desired.master:c->desired.track+c->view.primary;}
static void pickup(struct mixer_control*c){c->pickup=0;c->last_fader_valid=0;}
static void changed(struct mixer_control*c){c->dirty=1;c->message="";}
static int one(const struct mixer_control*c){return c->view.master||count(c->view.selected)==1;}
static void open_fx(struct mixer_control*);
static void clear_fx_page(struct mixer_control*c)
{
    c->catalog=c->view.fx_parameters=c->view.fx_delete=0;c->fx_signal_ms=0;
}
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
    if(!own_ack){
        c->history_start=c->history_pos=0;c->history_count=1;
        c->history[0]=*p;c->history_step=c->history_request=0;c->naming=0;
    }else if(c->history_step){
        c->history_pos=(unsigned)((int)c->history_pos+c->history_step);c->history_step=0;
    }else if(memcmp(&c->model,p,sizeof(*p)))history_record(c,p);
    c->model=*p;c->revision=revision;c->submitted=0;
    if(!own_ack||!c->dirty)c->desired=*p;
    if(!own_ack){c->dirty=0;c->view.colour_edit=0;pickup(c);if(c->view.page==MIXER_KNOB_FX)open_fx(c);}
    return 1;
}
int mixer_control_open(struct mixer_control*c,int ready,int blocked)
{
    if(!c||!ready||blocked||!c->epoch)return 0;
    clear_fx_page(c);c->name_since=0;
    c->active=1;c->view.page=MIXER_OVERVIEW;c->temporary=0;c->held_tracks=0;
    c->view.cursor=c->view.scroll=c->view.colour_edit=c->view.fx_delete=0;c->message="";++c->ticket;pickup(c);return 1;
}
int mixer_control_reject(struct mixer_control*c,uint64_t revision,const char *reason)
{
    if(!c||!revision||revision!=c->submitted)return 0;
    c->submitted=c->dirty=0;c->history_step=c->history_request=0;c->desired=c->model;c->message=reason?reason:"Update failed";
    if(c->view.page==MIXER_KNOB_FX)open_fx(c);
    pickup(c);return 1;
}
void mixer_control_close(struct mixer_control*c)
{
    if(c){c->active=0;c->naming=0;c->temporary=0;c->held_tracks=0;c->shift=c->view.colour_edit=0;++c->ticket;pickup(c);}
}
int mixer_control_active(const struct mixer_control*c){return c&&c->active;}
uint64_t mixer_control_name_light(const struct mixer_control*c,unsigned pad)
{return c&&c->naming?phone_light(&c->name,pad+1):0;}
uint64_t mixer_control_ticket(const struct mixer_control*c){return c?c->ticket:0;}
/* Device-agreed layout (07.11.2026): one Mixer page with Track cells.
 * B1 = back to the Mixer from PAD FX/KNOB FX, from the Mixer exit to Sample;
 * B2 = Mute, B3 = Solo of the selection; Fader = Level, K1 = Pan, K3 =
 * Colour, all absolute and equal for every selected Track; the last moved
 * control becomes the encoder parameter. */
enum {PARAM_LEVEL,PARAM_PAN,PARAM_COLOUR,PARAM_SEND,PARAMS};
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
    struct mixer_channel*ch=selected(c);
    if(c->view.fx_slot>ch->fx.count)c->view.fx_slot=ch->fx.count;
    clear_fx_page(c);
    c->parameter_page=0;
    c->view.cursor=0;
}
void mixer_control_set_fx_editing(struct mixer_control*c,int on){if(c)c->fx_editing=on!=0;}
static int put_fx(struct mixer_control*c,const struct fx_stage*stage)
{
    struct fx_chain_config*chain=&selected(c)->fx;
    unsigned slot=c->view.fx_slot;
    if(slot<chain->count)chain->stage[slot]=*stage;
    else if(chain->count>=MIXER_FX_SLOTS||fx_chain_add(chain,stage,c->rate)!=FX_OK){
        c->message="FX chain full";return 0;
    }
    open_fx(c);changed(c);return 1;
}
static void each(struct mixer_control*c,void(*apply)(struct mixer_mix*,float),float value)
{
    if(c->view.master){apply(&c->desired.master.mix,value);changed(c);return;}
    for(unsigned i=0;i<8;++i)if(c->view.selected&(1u<<i))apply(&c->desired.track[i].mix,value);
    changed(c);
}
static void set_send(struct mixer_mix*m,float value){m->send=fmaxf(0,fminf(1,value));}
static void set_send_destination(struct mixer_control*c,unsigned destination)
{
    if(!mixer_set_send_to(&c->desired,c->view.selected,destination)){
        c->message="Send would create a loop";return;
    }
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
    if(!c||(unsigned)action>MIXER_ERASE)return 0;
    unsigned bit=1u<<action;
    if(action==MIXER_SHIFT){
        if(c->shift!=(down!=0))pickup(c);
        c->shift=down!=0;
        return 0;
    }
    if(!down&&(c->owned_buttons&bit)){c->owned_buttons&=~bit;return 1;}
    if(down&&(c->owned_buttons&bit))return 1;
    int bank_action=action>=MIXER_BANK&&action<=MIXER_BANK_PREVIOUS;
    if(action==MIXER_ERASE&&!c->naming&&(c->view.page!=MIXER_KNOB_FX||!c->fx_editing))return 0;
    int bank_page=c->view.page==MIXER_ASSIGN||c->view.page==MIXER_INSPECT;
    int history=bank_action&&action!=MIXER_BANK&&c->shift&&!c->naming;
    if(bank_action&&!bank_page&&!history&&!c->naming&&
       !(action==MIXER_BANK&&c->view.page==MIXER_OVERVIEW)&&c->view.page!=MIXER_KNOB_FX)return 0;
    if(action==MIXER_NATIVE_MODE){if(down&&c->active)mixer_control_close(c);return 0;}
    if(!c->active)return 0;
    if(ticket!=c->ticket)return 1;
    if(!down)return 1;
    c->owned_buttons|=bit;c->message="";
    if(c->naming){
        if(action==MIXER_B1)c->naming=0;
        else if(action==MIXER_B3){
            char name[MIXER_NAME_BYTES]={0};
            if(!phone_utf8(&c->name,name,sizeof(name))){c->message="Name required (max 31 bytes)";return 1;}
            if(strcmp(selected(c)->name,name)){memcpy(selected(c)->name,name,sizeof(name));c->name_since=0;changed(c);}
            c->naming=0;
        }else if(action==MIXER_ERASE||action==MIXER_B2)phone_delete(&c->name,!c->name.enabled&&c->name.cursor<c->name.length);
        else if(action==MIXER_SELECT||action==MIXER_BANK_NEXT)phone_move(&c->name,1);
        else if(action==MIXER_BANK_PREVIOUS)phone_move(&c->name,-1);
        return 1;
    }
    if(c->view.fx_delete){
        if(action==MIXER_B1)c->view.fx_delete=0;
        if(action==MIXER_B3){
            if(fx_chain_remove(&selected(c)->fx,c->view.fx_slot)==FX_OK){open_fx(c);changed(c);}
        }
        return 1;
    }
    if(action==MIXER_ERASE){
        if(!c->view.fx_parameters&&!c->catalog&&c->view.fx_slot<selected(c)->fx.count)c->view.fx_delete=1;
        return 1;
    }
    if(history){
        c->history_request=action==MIXER_BANK_PREVIOUS?-1:1;return 1;
    }
    if(bank_action){
        if(!bank_page)return 1;
        int previous=action==MIXER_BANK_PREVIOUS||(action==MIXER_BANK&&c->shift);
        c->view.bank=(c->view.bank+(previous?7:1))%8;
        c->view.sound=c->view.bank*16+c->view.sound%16;
    }else if(action==MIXER_B1){
        if(c->view.page==MIXER_KNOB_FX&&(c->catalog||c->view.fx_parameters)){open_fx(c);return 1;}
        if(c->view.page!=MIXER_OVERVIEW){c->view.page=MIXER_OVERVIEW;c->temporary=0;c->view.cursor=PARAM_LEVEL;}
        else{c->exit_requested=1;mixer_control_close(c);}
    }else if(c->view.page==MIXER_KNOB_FX&&c->fx_editing&&
             (action==MIXER_B2||action==MIXER_B3)&&!c->shift&&!c->view.fx_parameters){
        struct fx_chain_config*chain=&selected(c)->fx;
        if(action==MIXER_B2){
            if(c->view.fx_slot>=chain->count)c->message="Select an effect to copy";
            else{c->fx_clipboard=chain->stage[c->view.fx_slot];c->fx_copied=1;c->message="Effect copied";}
        }else if(!c->fx_copied)c->message="No effect copied";
        else if(put_fx(c,&c->fx_clipboard))c->message="Effect pasted";
    }else if(action==MIXER_B3&&c->shift&&c->view.page==MIXER_KNOB_FX&&c->view.fx_parameters){
        struct fx_chain_config*chain=&selected(c)->fx;
        unsigned slot=c->view.fx_slot;
        if(slot<chain->count&&chain->stage[slot].kind==FX_STAGE_EQ){
            struct fx_eq_config*eq=&chain->stage[slot].eq;
            if(eq->count>=3)c->message="Three EQ points maximum";
            else{
                unsigned point=eq->count++;
                eq->band[point]=(struct fx_eq_band){point==1?250:4000,0,1,FX_EQ_BELL,1};
                c->parameter_page=c->view.cursor=point;changed(c);
            }
        }
    }else if(action==MIXER_B2&&c->view.page==MIXER_KNOB_FX&&c->view.fx_parameters&&!c->shift){
        selected(c)->fx.stage[c->view.fx_slot].bypass^=1;changed(c);
    }else if(action==MIXER_B2&&c->shift&&c->view.page==MIXER_OVERVIEW){
        if(!one(c)){c->message="Select one Track";return 1;}
        phone_init(&c->name,MIXER_NAME_BYTES-1);
        if(phone_sync(&c->name,selected(c)->name,0))c->naming=1;
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
        clear_fx_page(c);c->view.page=c->shift?MIXER_INSPECT:MIXER_ASSIGN;c->temporary=1;
    }else if(action==MIXER_KNOB){
        if(c->view.page==MIXER_KNOB_FX){clear_fx_page(c);c->view.page=MIXER_OVERVIEW;c->temporary=0;c->view.cursor=PARAM_LEVEL;return 1;}
        if(c->fx_editing&&!one(c)){c->message="Select one Track";return 1;}
        c->view.page=MIXER_KNOB_FX;c->temporary=2;c->view.fx_slot=0;open_fx(c);
        if(!c->fx_editing)c->message="Track FX: coming later";
    }else if(action==MIXER_SELECT){
        if(c->view.page==MIXER_OVERVIEW)c->view.cursor=(c->view.cursor+1)%PARAMS;
        else if(c->view.page==MIXER_KNOB_FX&&c->fx_editing){struct mixer_channel*ch=selected(c);
            if(c->catalog){struct fx_stage stage;
                if(fx_mixer_catalog_stage(c->view.cursor,c->rate,&stage)){
                    if(stage.kind==FX_STAGE_EQ)stage.eq.count=1;
                    put_fx(c,&stage);
                }
            }else if(c->view.fx_slot==ch->fx.count)c->catalog=1;
            else if(c->shift){ch->fx.stage[c->view.fx_slot].bypass^=1;changed(c);}
            else c->view.fx_parameters=1;
        }
    }
    return 1;
}
int mixer_control_pad(struct mixer_control*c,unsigned pad,int down,uint64_t ticket)
{
    if(!c||pad>=16)return 0;
    unsigned bit=1u<<pad;
    if(!down&&(c->owned_pads&bit)){if(c->naming)phone_pad(&c->name,pad+1,0,name_time());c->owned_pads&=~bit;if(pad<8)c->held_tracks&=~bit;return 1;}
    if(down&&(c->owned_pads&bit))return 1;
    if(!c->active)return 0;
    if(ticket!=c->ticket)return 1;
    if(!down)return 1;
    c->owned_pads|=bit;
    if(c->naming){c->name.shift=c->shift;phone_pad(&c->name,pad+1,1,name_time());return 1;}
    int fx_pad=pad>=8&&pad<8+MIXER_FX_SLOTS;
    if(c->view.page==MIXER_KNOB_FX&&(c->view.fx_delete||(c->view.fx_parameters&&!fx_pad)))return 1;
    if(c->view.page==MIXER_ASSIGN||c->view.page==MIXER_INSPECT){unsigned sound=c->view.bank*16+pad;c->view.sound=sound;
        if(c->view.page==MIXER_ASSIGN){if(!(c->present[sound/64]&(UINT64_C(1)<<(sound%64))))c->message="Empty pad";
            else if(c->view.master){if(c->desired.output[sound]){c->desired.output[sound]=0;changed(c);}}
            else if(one(c)){unsigned output=c->view.primary+1;
                c->desired.output[sound]=(unsigned char)(c->desired.output[sound]==output?0:output);changed(c);}}
        return 1;
    }
    if(c->fx_editing&&fx_pad&&
       (c->view.page==MIXER_OVERVIEW||c->view.page==MIXER_KNOB_FX)){
        if(!one(c)){c->message="Select one Track";return 1;}
        unsigned slot=pad-8,used=selected(c)->fx.count;
        if(slot>used){c->message="Select the first free FX slot";return 1;}
        c->view.page=MIXER_KNOB_FX;c->temporary=2;c->view.fx_slot=slot;
        open_fx(c);
        if(slot<used)c->view.fx_parameters=1;
        else c->catalog=1;
        c->message="";
        return 1;
    }
    if(c->view.page==MIXER_KNOB_FX&&c->fx_editing){
        c->message="";
        if(pad<8){c->view.primary=pad;c->view.selected=bit;c->view.master=0;c->view.fx_slot=0;}
        else if(pad==15){c->view.master=1;c->view.selected=0;c->view.fx_slot=0;}
        else return 1;
        open_fx(c);return 1;
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
static void move_fader(struct mixer_control*c,unsigned value)
{
    unsigned mask=c->view.selected;
    float db=mixer_fader_db(value);
    if(c->view.master||!(mask&(mask-1))||c->shift){
        each(c,set_level,db);
        return;
    }
    if(!c->last_fader_valid){
        c->last_fader=value;
        for(unsigned i=0;i<MIXER_TRACKS;++i)
            if(mask&(1u<<i))c->anchors[i]=c->desired.track[i].mix.level_db;
        c->last_fader_valid=1;
        return;
    }
    /* Bound the common offset before applying it so a track reaching a
     * limit cannot change the balance of the group. Off tracks stay Off. */
    float low=-INFINITY,high=INFINITY;
    for(unsigned i=0;i<MIXER_TRACKS;++i)if((mask&(1u<<i))&&isfinite(c->anchors[i])){
        low=fmaxf(low,MIXER_LEVEL_MIN_DB-c->anchors[i]);
        high=fminf(high,MIXER_LEVEL_MAX_DB-c->anchors[i]);
    }
    float offset=mixer_fader_db(value?value:1)-mixer_fader_db(c->last_fader?c->last_fader:1);
    offset=fmaxf(low,fminf(high,offset));
    for(unsigned i=0;i<MIXER_TRACKS;++i)if((mask&(1u<<i))&&isfinite(c->anchors[i])){
        float level=c->anchors[i]+offset;
        if(c->desired.track[i].mix.level_db!=level){
            c->desired.track[i].mix.level_db=level;
            changed(c);
        }
    }
}
int mixer_control_reset(struct mixer_control*c,unsigned cc,uint64_t ticket)
{
    /* Pad Bank held + control: default value for the whole selection. */
    if(!c||!c->active)return 0;
    if(ticket!=c->ticket||(c->view.page!=MIXER_OVERVIEW&&c->view.page!=MIXER_ASSIGN))return 1;
    if(cc==0x14){pickup(c);each(c,set_level,0);c->view.cursor=PARAM_LEVEL;}
    else if(cc==0x10){each(c,set_pan,0);c->view.cursor=PARAM_PAN;}
    else if(cc==0x11&&!c->view.master){each(c,set_send,0);c->view.cursor=PARAM_SEND;}
    else if(cc==0x12){set_colour(c,MIXER_DEFAULT_TRACK_COLOUR);c->view.cursor=PARAM_COLOUR;}
    return 1;
}
static int fx_cc(struct mixer_control*c,unsigned cc,unsigned value)
{
    struct mixer_channel*ch=selected(c);
    if(c->view.fx_delete)return 1;
    if(cc==0x64){int delta=value>64?(int)value-128:value==64?0:(int)value;
        if(!delta)return 1;
        if(c->shift&&!c->catalog&&!c->view.fx_parameters){
            unsigned from=c->view.fx_slot;
            if(from<ch->fx.count){
                int to=(int)from+delta;
                if(to<0)to=0;
                unsigned last=ch->fx.count-1;
                if(last>=MIXER_FX_SLOTS)last=MIXER_FX_SLOTS-1;
                if((unsigned)to>last)to=(int)last;
                if((unsigned)to!=from&&fx_chain_move(&ch->fx,from,(unsigned)to)==FX_OK){
                    c->view.fx_slot=(unsigned)to;changed(c);
                }
            }
            return 1;
        }
        if(!c->catalog&&!c->view.fx_parameters){
            unsigned last=ch->fx.count<MIXER_FX_SLOTS?ch->fx.count:MIXER_FX_SLOTS-1;
            int next=(int)c->view.fx_slot+delta;
            if(next<0)next=0;
            c->view.fx_slot=(unsigned)next>last?last:(unsigned)next;
            open_fx(c);return 1;
        }
        unsigned limit=c->catalog?fx_mixer_catalog_count():5;
        if(!c->catalog&&c->view.fx_slot<ch->fx.count){
            const struct fx_stage*s=ch->fx.stage+c->view.fx_slot;
            if(s->kind==FX_STAGE_EQ)limit=s->eq.count?s->eq.count:1;
            else{
                unsigned n=fx_catalog_parameter_count(fx_catalog_find(s->native_id));
                limit=(n+5)/6;if(!limit)limit=1;
            }
        }
        int next=(int)c->view.cursor+delta;if(next<0)next=0;if((unsigned)next>=limit)next=(int)limit-1;c->view.cursor=(unsigned)next;
        if(!c->catalog)c->parameter_page=c->view.cursor;
        return 1;
    }
    if(!one(c)){c->message="Select one Track";return 1;}
    if(c->catalog){if(cc==0x10)c->view.cursor=value*fx_mixer_catalog_count()/128;return 1;}
    if(cc==0x14||!c->view.fx_parameters||c->view.fx_slot>=ch->fx.count)return 1;
    struct fx_stage*s=ch->fx.stage+c->view.fx_slot;
    if(s->kind!=FX_STAGE_EQ){unsigned position=c->parameter_page*6+(c->shift?3:0)+cc-0x10;
        int index=fx_catalog_parameter_index(fx_catalog_find(s->native_id),position);
        if(index>=0){s->parameter[index]=value/127.f;changed(c);}
    }else{if(!s->eq.count)fx_eq_default(&s->eq);
        struct fx_eq_band*b=s->eq.band+(c->parameter_page<s->eq.count?c->parameter_page:0);
        if(c->shift&&cc==0x10)b->enabled=value>=64;
        else if(c->shift&&cc==0x11)b->q=.2*pow(80,value/127.);
        else if(c->shift&&cc==0x12)s->eq.output_db=-24+value*36./127;
        else if(cc==0x10)b->hz=20*pow(1000,value/127.);
        else if(cc==0x11)b->gain_db=-18+value*36./127;
        else b->type=value*6/128;
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
    if(c->naming){
        if(cc==0x10&&c->name.enabled!=(value>=64))phone_enable(&c->name,value>=64);
        if(cc==0x64){
            int delta=value>64?(int)value-128:value==64?0:(int)value;
            if(c->name.enabled||c->shift)phone_move(&c->name,delta);
            else name_turn(&c->name,delta);
        }
        return 1;
    }
    if(c->view.page==MIXER_INSPECT){
        if(cc==0x12){c->view.bank=value*8/128;c->view.sound=c->view.bank*16+c->view.sound%16;}
        else if(cc==0x64){int delta=value>64?(int)value-128:(int)value;
            int sound=(int)c->view.sound+delta;if(sound<0)sound=0;if(sound>127)sound=127;c->view.sound=(unsigned)sound;c->view.bank=(unsigned)sound/16;}
        return 1; /* Inspect is read-only, including Fader and K1. */
    }
    /* PAD FX (Select pads): the enlarged Track on the left keeps Fader, K1,
     * K3 and the encoder exactly as on the Mixer page; banks via Bank/+/-. */
    if(c->view.page==MIXER_KNOB_FX&&c->fx_editing)return fx_cc(c,cc,value);
    if(c->view.page!=MIXER_OVERVIEW&&c->view.page!=MIXER_ASSIGN)return 1;
    if(cc==0x14){move_fader(c,value);c->view.cursor=PARAM_LEVEL;return 1;}
    if(cc==0x10){each(c,set_pan,value==64?0:((float)value-64)/(value<64?64:63));c->view.cursor=PARAM_PAN;return 1;}
    if(cc==0x12){set_colour(c,value*MIXER_COLOURS/128);c->view.cursor=PARAM_COLOUR;return 1;}
    if(cc==0x11){
        if(c->view.master)return 1;
        c->view.cursor=PARAM_SEND;
        if(c->shift)set_send_destination(c,value*9/128);
        else each(c,set_send,value/127.f);
        return 1;
    }
    int delta=value>64?(int)value-128:(int)value;if(!delta)return 1;
    const struct mixer_mix*m=primary_mix(c);
    if(c->view.cursor==PARAM_LEVEL){float db=isfinite(m->level_db)?m->level_db:-60.5f;db+=delta*.5f;
        pickup(c);each(c,set_level,db< -60?-INFINITY:db>6?6:db);}
    else if(c->view.cursor==PARAM_PAN)each(c,set_pan,m->pan+delta*.02f);
    else if(c->view.cursor==PARAM_SEND&&!c->view.master){
        if(c->shift){
            int to=c->desired.send_to[c->view.primary]+delta;
            set_send_destination(c,to<0?0:to>8?8:(unsigned)to);
        }else each(c,set_send,m->send+delta*.01f);
    }
    else if(c->view.cursor==PARAM_COLOUR&&!c->view.master){int index=mixer_colour_index(c->desired.track[c->view.primary].colour);
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
    if(!c||c->submitted)return;
    if(!c->dirty&&c->history_request){
        int next=(int)c->history_pos+c->history_request;
        if(next<0||next>=(int)c->history_count)c->message=c->history_request<0?"Nothing to undo":"Nothing to redo";
        else{
            c->history_step=c->history_request;
            c->desired=c->history[history_index(c,(unsigned)next)];c->dirty=1;pickup(c);
            if(c->view.page==MIXER_KNOB_FX)open_fx(c);
            c->message=c->history_step<0?"Mixer undo":"Mixer redo";
        }
        c->history_request=0;
    }
    if(!c->dirty)return;
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
void mixer_control_fx_monitor(struct mixer_control*c,const struct mixer_fx_monitor*signal,uint64_t now)
{
    c->fx_signal=*signal;c->fx_signal_ms=now;
}
void mixer_control_meter(struct mixer_control*c,const struct mixer_meter_snapshot*m,uint64_t now)
{
    if(c&&m&&m->epoch==c->epoch){c->meter=*m;c->view.meter_ms=now;}
}
void mixer_control_view(struct mixer_control*c,uint64_t now,struct mixer_view_state*out)
{
    if(c&&out){*out=c->view;out->fx_signal=c->fx_signal_ms&&now>=c->fx_signal_ms&&now-c->fx_signal_ms<=250?&c->fx_signal:NULL;out->project=c->view.page==MIXER_KNOB_FX||c->view.cursor==PARAM_SEND?&c->desired:&c->model;out->meter=&c->meter;out->epoch=c->epoch;out->now_ms=now;
        out->revision=c->revision;out->pending=c->submitted||c->dirty;out->fx_catalog=c->catalog;out->parameter_page=c->parameter_page;
        out->message=c->message;out->pickup=!c->pickup;out->present[0]=c->present[0];out->present[1]=c->present[1];}
    if(c&&out){
        unsigned target=(unsigned)c->view.page*9+(c->view.master?8:c->view.primary);
        if(now&&(!c->name_since||target!=c->name_target||now<c->name_since)){
            c->name_since=now;c->name_target=target;
        }
        out->name_elapsed=now>=c->name_since?now-c->name_since:0;
        if(c->naming){
            phone_tick(&c->name,name_time());
            phone_view(&c->name,18,c->name_preview,sizeof(c->name_preview));
            out->name_edit=c->name_preview;out->name_pads=c->name.enabled;
        }
        out->shifted=c->shift;out->fx_editing=c->fx_editing;out->rate=c->rate;}
}
const char*mixer_control_message(const struct mixer_control*c){return c?c->message:"";}
